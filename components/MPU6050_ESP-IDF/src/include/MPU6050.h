/*
 * MPU6050 6-axis IMU driver for ESP-IDF
 *
 * Supports accelerometer (±2/4/8/16 g) and gyroscope (±250/500/1000/2000 °/s)
 * via I2C. Intended for use with EdgeImpulse on ESP32 WROOM.
 *
 * Reference: https://github.com/natanaeljr/esp32-MPU-driver
 * InvenSense MPU-6050 Product Specification Rev 3.4
 */

#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

/* I2C bus configuration ---------------------------------------------------- */
#define MPU6050_I2C_SCL_IO          22
#define MPU6050_I2C_SDA_IO          21
#define MPU6050_I2C_NUM             I2C_NUM_0
#define MPU6050_I2C_FREQ_HZ         400000
#define MPU6050_I2C_TIMEOUT_MS      1000

/* I2C device addresses ----------------------------------------------------- */
#define MPU6050_DEFAULT_ADDRESS     0x68    /* AD0 pin = GND */
#define MPU6050_ADDRESS_AD0_HIGH    0x69    /* AD0 pin = VCC */

/* Register map ------------------------------------------------------------- */
#define MPU6050_REG_SMPLRT_DIV      0x19
#define MPU6050_REG_CONFIG          0x1A
#define MPU6050_REG_GYRO_CONFIG     0x1B
#define MPU6050_REG_ACCEL_CONFIG    0x1C
#define MPU6050_REG_FIFO_EN         0x23
#define MPU6050_REG_INT_ENABLE      0x38
#define MPU6050_REG_INT_STATUS      0x3A
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_REG_ACCEL_XOUT_L    0x3C
#define MPU6050_REG_ACCEL_YOUT_H    0x3D
#define MPU6050_REG_ACCEL_YOUT_L    0x3E
#define MPU6050_REG_ACCEL_ZOUT_H    0x3F
#define MPU6050_REG_ACCEL_ZOUT_L    0x40
#define MPU6050_REG_TEMP_OUT_H      0x41
#define MPU6050_REG_TEMP_OUT_L      0x42
#define MPU6050_REG_GYRO_XOUT_H     0x43
#define MPU6050_REG_GYRO_XOUT_L     0x44
#define MPU6050_REG_GYRO_YOUT_H     0x45
#define MPU6050_REG_GYRO_YOUT_L     0x46
#define MPU6050_REG_GYRO_ZOUT_H     0x47
#define MPU6050_REG_GYRO_ZOUT_L     0x48
#define MPU6050_REG_USER_CTRL       0x6A
#define MPU6050_REG_PWR_MGMT_1      0x6B
#define MPU6050_REG_PWR_MGMT_2      0x6C
#define MPU6050_REG_WHO_AM_I        0x75

/* PWR_MGMT_1 bits ---------------------------------------------------------- */
#define MPU6050_PWR1_DEVICE_RESET   0x80
#define MPU6050_PWR1_SLEEP          0x40
#define MPU6050_PWR1_CLKSEL_XGYRO   0x01   /* PLL with X-axis gyro ref (recommended) */

/* ACCEL_CONFIG full-scale range -------------------------------------------- */
#define MPU6050_ACCEL_FS_2G         0x00
#define MPU6050_ACCEL_FS_4G         0x08
#define MPU6050_ACCEL_FS_8G         0x10
#define MPU6050_ACCEL_FS_16G        0x18
#define MPU6050_ACCEL_FS_MASK       0x18

/* GYRO_CONFIG full-scale range --------------------------------------------- */
#define MPU6050_GYRO_FS_250DPS      0x00
#define MPU6050_GYRO_FS_500DPS      0x08
#define MPU6050_GYRO_FS_1000DPS     0x10
#define MPU6050_GYRO_FS_2000DPS     0x18
#define MPU6050_GYRO_FS_MASK        0x18

/* WHO_AM_I expected value --------------------------------------------------- */
#define MPU6050_WHO_AM_I_VALUE      0x68

/* Sensitivity scale factors (LSB per unit) --------------------------------- */
#define MPU6050_ACCEL_SENS_2G       16384.0f
#define MPU6050_ACCEL_SENS_4G       8192.0f
#define MPU6050_ACCEL_SENS_8G       4096.0f
#define MPU6050_ACCEL_SENS_16G      2048.0f

#define MPU6050_GYRO_SENS_250DPS    131.0f
#define MPU6050_GYRO_SENS_500DPS    65.5f
#define MPU6050_GYRO_SENS_1000DPS   32.8f
#define MPU6050_GYRO_SENS_2000DPS   16.4f

/* Enumerations ------------------------------------------------------------- */

typedef enum {
    MPU6050_ACCEL_RANGE_2G  = MPU6050_ACCEL_FS_2G,
    MPU6050_ACCEL_RANGE_4G  = MPU6050_ACCEL_FS_4G,
    MPU6050_ACCEL_RANGE_8G  = MPU6050_ACCEL_FS_8G,
    MPU6050_ACCEL_RANGE_16G = MPU6050_ACCEL_FS_16G,
} mpu6050_accel_range_t;

typedef enum {
    MPU6050_GYRO_RANGE_250DPS  = MPU6050_GYRO_FS_250DPS,
    MPU6050_GYRO_RANGE_500DPS  = MPU6050_GYRO_FS_500DPS,
    MPU6050_GYRO_RANGE_1000DPS = MPU6050_GYRO_FS_1000DPS,
    MPU6050_GYRO_RANGE_2000DPS = MPU6050_GYRO_FS_2000DPS,
} mpu6050_gyro_range_t;

/* DLPF (Digital Low Pass Filter) bandwidth --------------------------------- */
typedef enum {
    MPU6050_DLPF_260HZ = 0,
    MPU6050_DLPF_184HZ = 1,
    MPU6050_DLPF_94HZ  = 2,
    MPU6050_DLPF_44HZ  = 3,
    MPU6050_DLPF_21HZ  = 4,
    MPU6050_DLPF_10HZ  = 5,
    MPU6050_DLPF_5HZ   = 6,
} mpu6050_dlpf_t;

/* Driver class ------------------------------------------------------------- */

class MPU6050 {
public:
    MPU6050();

    bool begin(uint8_t address = MPU6050_DEFAULT_ADDRESS);
    bool isConnection(void);

    void setAccelRange(mpu6050_accel_range_t range);
    void setGyroRange(mpu6050_gyro_range_t range);
    void setDLPF(mpu6050_dlpf_t dlpf);
    void setSampleRateDivider(uint8_t div);

    /* Returns acceleration in g */
    void getAcceleration(float *ax, float *ay, float *az);

    /* Returns angular velocity in degrees/second */
    void getGyroscope(float *gx, float *gy, float *gz);

    /* Read all 6 axes at once (one burst I2C read, more efficient) */
    void getMotion6(float *ax, float *ay, float *az,
                    float *gx, float *gy, float *gz);

    float getTemperatureCelsius(void);
    uint8_t getDeviceID(void);

private:
    void writeRegister(uint8_t reg, uint8_t val);
    uint8_t readRegister(uint8_t reg);
    void readRegisters(uint8_t reg, uint8_t *buf, uint8_t len);

    uint8_t  devAddr;
    float    accelSens;
    float    gyroSens;
};

#endif /* MPU6050_H */
