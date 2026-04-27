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

/* Constant defines -------------------------------------------------------- */
#define CONVERT_G_TO_MS2    9.80665f

/* imu_data layout: [accX, accY, accZ, gyrX, gyrY, gyrZ] */
static float imu_data[INERTIAL_AXIS_SAMPLED];

static MPU6050 mpu;

bool ei_inertial_init(void)
{
    if (mpu.begin(MPU6050_DEFAULT_ADDRESS) == false) {
        EI_LOGW("Failed to connect to MPU6050!\n");
        return false;
    }

    ei_sleep(100);

    mpu.setAccelRange(MPU6050_ACCEL_RANGE_2G);
    mpu.setGyroRange(MPU6050_GYRO_RANGE_250DPS);
    mpu.setSampleRateDivider(9);   /* 100 Hz with 44 Hz DLPF */
    mpu.setDLPF(MPU6050_DLPF_44HZ);

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
