/*
 * MPU6050 6-axis IMU driver for ESP-IDF
 *
 * Uses the legacy I2C master API (i2c_master_write_to_device /
 * i2c_master_write_read_device) consistent with the existing LIS3DHTR driver
 * in this project.
 */

#include "MPU6050.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_idf_version.h"
#include "esp_log.h"

static const char *TAG = "MPU6050";

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#define DELAY_MS(ms) vTaskDelay((ms) / portTICK_RATE_MS)

/* -------------------------------------------------------------------------- */

MPU6050::MPU6050()
    : devAddr(MPU6050_DEFAULT_ADDRESS),
      accelSens(MPU6050_ACCEL_SENS_2G),
      gyroSens(MPU6050_GYRO_SENS_250DPS)
{
}

bool MPU6050::begin(uint8_t address)
{
    devAddr = address;

    /* Reset pins to a known default state before I2C config.
     * Needed when other code (e.g. LED setup) previously set these
     * GPIOs as push-pull outputs, which blocks open-drain I2C. */
    gpio_reset_pin((gpio_num_t)MPU6050_I2C_SDA_IO);
    gpio_reset_pin((gpio_num_t)MPU6050_I2C_SCL_IO);

    i2c_config_t conf = {
        .mode          = I2C_MODE_MASTER,
        .sda_io_num    = MPU6050_I2C_SDA_IO,
        .scl_io_num    = MPU6050_I2C_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
    };
    conf.master.clk_speed = MPU6050_I2C_FREQ_HZ;

    i2c_param_config(MPU6050_I2C_NUM, &conf);

    /* Suppress the built-in error log on "already installed" — expected
     * when begin() is called a second time for address retry. */
    esp_log_level_set("i2c", ESP_LOG_NONE);
    esp_err_t err = i2c_driver_install(MPU6050_I2C_NUM, conf.mode, 0, 0, 0);
    esp_log_level_set("i2c", ESP_LOG_WARN);

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }

    DELAY_MS(200);   /* allow MPU6050 power-on startup (datasheet: 30 ms min) */

    if (!isConnection()) {
        return false;
    }

    /* Reset device, then wake up with PLL locked to X-axis gyro clock */
    writeRegister(MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_DEVICE_RESET);
    DELAY_MS(100);
    writeRegister(MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_XGYRO);
    DELAY_MS(10);

    /* 100 Hz sample rate: SMPLRT_DIV = (gyro_output_rate / sample_rate) - 1
     * With DLPF enabled gyro output = 1 kHz, DIV = 9, 100 Hz */
    setSampleRateDivider(9);
    setDLPF(MPU6050_DLPF_44HZ);

    setAccelRange(MPU6050_ACCEL_RANGE_2G);
    setGyroRange(MPU6050_GYRO_RANGE_250DPS);

    return true;
}

bool MPU6050::isConnection(void)
{
    return (getDeviceID() == MPU6050_WHO_AM_I_VALUE);
}

uint8_t MPU6050::getDeviceID(void)
{
    return readRegister(MPU6050_REG_WHO_AM_I);
}

void MPU6050::setSampleRateDivider(uint8_t div)
{
    writeRegister(MPU6050_REG_SMPLRT_DIV, div);
}

void MPU6050::setDLPF(mpu6050_dlpf_t dlpf)
{
    writeRegister(MPU6050_REG_CONFIG, (uint8_t)dlpf & 0x07);
}

void MPU6050::setAccelRange(mpu6050_accel_range_t range)
{
    uint8_t reg = readRegister(MPU6050_REG_ACCEL_CONFIG);
    reg = (reg & ~MPU6050_ACCEL_FS_MASK) | (uint8_t)range;
    writeRegister(MPU6050_REG_ACCEL_CONFIG, reg);

    switch (range) {
        case MPU6050_ACCEL_RANGE_2G:  accelSens = MPU6050_ACCEL_SENS_2G;  break;
        case MPU6050_ACCEL_RANGE_4G:  accelSens = MPU6050_ACCEL_SENS_4G;  break;
        case MPU6050_ACCEL_RANGE_8G:  accelSens = MPU6050_ACCEL_SENS_8G;  break;
        case MPU6050_ACCEL_RANGE_16G: accelSens = MPU6050_ACCEL_SENS_16G; break;
    }
}

void MPU6050::setGyroRange(mpu6050_gyro_range_t range)
{
    uint8_t reg = readRegister(MPU6050_REG_GYRO_CONFIG);
    reg = (reg & ~MPU6050_GYRO_FS_MASK) | (uint8_t)range;
    writeRegister(MPU6050_REG_GYRO_CONFIG, reg);

    switch (range) {
        case MPU6050_GYRO_RANGE_250DPS:  gyroSens = MPU6050_GYRO_SENS_250DPS;  break;
        case MPU6050_GYRO_RANGE_500DPS:  gyroSens = MPU6050_GYRO_SENS_500DPS;  break;
        case MPU6050_GYRO_RANGE_1000DPS: gyroSens = MPU6050_GYRO_SENS_1000DPS; break;
        case MPU6050_GYRO_RANGE_2000DPS: gyroSens = MPU6050_GYRO_SENS_2000DPS; break;
    }
}

void MPU6050::getAcceleration(float *ax, float *ay, float *az)
{
    uint8_t buf[6];
    readRegisters(MPU6050_REG_ACCEL_XOUT_H, buf, 6);

    *ax = (float)(int16_t)((buf[0] << 8) | buf[1]) / accelSens;
    *ay = (float)(int16_t)((buf[2] << 8) | buf[3]) / accelSens;
    *az = (float)(int16_t)((buf[4] << 8) | buf[5]) / accelSens;
}

void MPU6050::getGyroscope(float *gx, float *gy, float *gz)
{
    uint8_t buf[6];
    readRegisters(MPU6050_REG_GYRO_XOUT_H, buf, 6);

    *gx = (float)(int16_t)((buf[0] << 8) | buf[1]) / gyroSens;
    *gy = (float)(int16_t)((buf[2] << 8) | buf[3]) / gyroSens;
    *gz = (float)(int16_t)((buf[4] << 8) | buf[5]) / gyroSens;
}

void MPU6050::getMotion6(float *ax, float *ay, float *az,
                          float *gx, float *gy, float *gz)
{
    /* Burst read 14 bytes starting at ACCEL_XOUT_H:
     * [0..5]  accel XYZ (big-endian int16)
     * [6..7]  temperature (skipped)
     * [8..13] gyro XYZ (big-endian int16) */
    uint8_t buf[14];
    readRegisters(MPU6050_REG_ACCEL_XOUT_H, buf, 14);

    *ax = (float)(int16_t)((buf[0]  << 8) | buf[1])  / accelSens;
    *ay = (float)(int16_t)((buf[2]  << 8) | buf[3])  / accelSens;
    *az = (float)(int16_t)((buf[4]  << 8) | buf[5])  / accelSens;
    *gx = (float)(int16_t)((buf[8]  << 8) | buf[9])  / gyroSens;
    *gy = (float)(int16_t)((buf[10] << 8) | buf[11]) / gyroSens;
    *gz = (float)(int16_t)((buf[12] << 8) | buf[13]) / gyroSens;
}

float MPU6050::getTemperatureCelsius(void)
{
    uint8_t buf[2];
    readRegisters(MPU6050_REG_TEMP_OUT_H, buf, 2);
    int16_t raw = (int16_t)((buf[0] << 8) | buf[1]);
    /* From datasheet: Temp_degC = raw / 340.0 + 36.53 */
    return (float)raw / 340.0f + 36.53f;
}

/* --------------------------------------------------------------------------
 * Private I2C helpers
 * -------------------------------------------------------------------------- */

void MPU6050::writeRegister(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    i2c_master_write_to_device(MPU6050_I2C_NUM, devAddr, buf, sizeof(buf),
                               MPU6050_I2C_TIMEOUT_MS / portTICK_RATE_MS);
}

uint8_t MPU6050::readRegister(uint8_t reg)
{
    uint8_t data = 0;
    i2c_master_write_read_device(MPU6050_I2C_NUM, devAddr, &reg, 1, &data, 1,
                                 MPU6050_I2C_TIMEOUT_MS / portTICK_RATE_MS);
    return data;
}

void MPU6050::readRegisters(uint8_t reg, uint8_t *buf, uint8_t len)
{
    i2c_master_write_read_device(MPU6050_I2C_NUM, devAddr, &reg, 1, buf, len,
                                 MPU6050_I2C_TIMEOUT_MS / portTICK_RATE_MS);
}
