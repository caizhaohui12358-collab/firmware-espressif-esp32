/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Include ----------------------------------------------------------------- */
#include <stdint.h>
#include <stdlib.h>

#include "ei_inertial_sensor.h"

#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "edge-impulse-sdk/porting/ei_logging.h"

#include "MPU6050.h"
#include "driver/i2c.h"

/* Constant defines -------------------------------------------------------- */
#define CONVERT_G_TO_MS2    9.80665f

static void i2c_scan(void)
{
    ei_printf("I2C bus scan (SDA=GPIO%d SCL=GPIO%d):\r\n",
              MPU6050_I2C_SDA_IO, MPU6050_I2C_SCL_IO);
    bool found = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        uint8_t dummy;
        esp_err_t ret = i2c_master_write_read_device(
            MPU6050_I2C_NUM, addr, &dummy, 0, &dummy, 1,
            100 / portTICK_PERIOD_MS);
        if (ret == ESP_OK) {
            ei_printf("  Found device at 0x%02X\r\n", addr);
            found = true;
        }
    }
    if (!found) {
        ei_printf("  No I2C devices found. Check wiring.\r\n");
    }
}

/* imu_data layout: [accX, accY, accZ, gyrX, gyrY, gyrZ] */
static float imu_data[INERTIAL_AXIS_SAMPLED];

static MPU6050 mpu;

bool ei_inertial_init(void)
{
    /* Auto-detect I2C address: 0x68 (AD0=GND) or 0x69 (AD0=VCC).
     * Many GY-521 modules pull AD0 high by default. */
    uint8_t addr = MPU6050_DEFAULT_ADDRESS;
    if (mpu.begin(addr) == false) {
        EI_LOGW("MPU6050 not found at 0x68, trying 0x69...\n");
        addr = MPU6050_ADDRESS_AD0_HIGH;
        if (mpu.begin(addr) == false) {
            /* Scan the bus to help diagnose wiring issues */
            i2c_scan();
            EI_LOGW("Failed to connect to MPU6050 at 0x68 and 0x69.\n"
                    "Check wiring: SDA->GPIO21, SCL->GPIO22, VCC->3.3V, GND->GND\n");
            return false;
        }
    }
    ei_printf("MPU6050 connected at I2C address 0x%02X\r\n", addr);

    if (ei_add_sensor_to_fusion_list(inertial_sensor) == false) {
        EI_LOGE("Failed to register Inertial sensor!\n");
        return false;
    }

    return true;
}

float *ei_fusion_inertial_read_data(int n_samples)
{
    float ax, ay, az, gx, gy, gz;

    /* Single burst read: accel XYZ + temp (skipped) + gyro XYZ */
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    /* Convert g → m/s² for accelerometer */
    imu_data[0] = ax * CONVERT_G_TO_MS2;
    imu_data[1] = ay * CONVERT_G_TO_MS2;
    imu_data[2] = az * CONVERT_G_TO_MS2;

    /* Gyroscope already in degrees/second */
    imu_data[3] = gx;
    imu_data[4] = gy;
    imu_data[5] = gz;

    return imu_data;
}
