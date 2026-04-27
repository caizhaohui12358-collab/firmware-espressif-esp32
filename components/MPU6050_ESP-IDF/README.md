# MPU6050_ESP-IDF

## Introduction

ESP-IDF driver for the InvenSense MPU-6050 6-axis IMU (3-axis accelerometer + 3-axis gyroscope) using the I2C interface. Written for the EdgeImpulse ESP32 WROOM firmware, with an API style consistent with the existing LIS3DHTR component in this project.

- Accelerometer range: ±2 / ±4 / ±8 / ±16 g
- Gyroscope range: ±250 / ±500 / ±1000 / ±2000 °/s
- Configurable DLPF (5 – 260 Hz) and sample rate (up to 1 kHz)
- Single 14-byte burst read for all 6 axes (`getMotion6`)
- Compatible with ESP-IDF v5.x (uses legacy `i2c_master_write_to_device` API)

Reference: [natanaeljr/esp32-MPU-driver](https://github.com/natanaeljr/esp32-MPU-driver) · InvenSense MPU-6050 Product Specification Rev 3.4

## Hardware

| MPU6050 Pin | ESP32 WROOM Pin | Note |
|-------------|----------------|------|
| VCC         | 3.3V           | Do **not** use 5V |
| GND         | GND            | |
| SDA         | GPIO 21        | I2C data |
| SCL         | GPIO 22        | I2C clock |
| AD0         | GND            | I2C address = 0x68 |
| INT         | —              | Not used |

## How to Install

Clone or copy this folder into your project's `components/` directory:

```bash
git clone https://github.com/caizhaohui12358-collab/firmware-espressif-esp32
# component is at components/MPU6050_ESP-IDF/
```

## Usage

```cpp
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "MPU6050.h"

static MPU6050 mpu;

extern "C" int app_main()
{
    if (!mpu.begin(MPU6050_DEFAULT_ADDRESS)) {
        printf("ERR: failed to connect to MPU6050!\n");
        return -1;
    }

    mpu.setAccelRange(MPU6050_ACCEL_RANGE_2G);
    mpu.setGyroRange(MPU6050_GYRO_RANGE_250DPS);
    mpu.setDLPF(MPU6050_DLPF_44HZ);
    mpu.setSampleRateDivider(9);   // 100 Hz

    float ax, ay, az, gx, gy, gz;

    while (true) {
        // Single burst read (most efficient)
        mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
        printf("acc: %.3f  %.3f  %.3f g\n", ax, ay, az);
        printf("gyr: %.3f  %.3f  %.3f dps\n", gx, gy, gz);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
```

## API Reference

| Method | Description |
|--------|-------------|
| `begin(address)` | Init I2C, verify WHO_AM_I, reset device, apply defaults |
| `isConnection()` | Returns `true` if WHO_AM_I == 0x68 |
| `setAccelRange(range)` | `MPU6050_ACCEL_RANGE_2G/4G/8G/16G` |
| `setGyroRange(range)` | `MPU6050_GYRO_RANGE_250DPS/500/1000/2000DPS` |
| `setDLPF(dlpf)` | `MPU6050_DLPF_260HZ` … `MPU6050_DLPF_5HZ` |
| `setSampleRateDivider(div)` | Rate = 1000 / (div + 1) Hz (when DLPF enabled) |
| `getMotion6(&ax,&ay,&az,&gx,&gy,&gz)` | Burst read all 6 axes |
| `getAcceleration(&ax,&ay,&az)` | Accel only, unit: **g** |
| `getGyroscope(&gx,&gy,&gz)` | Gyro only, unit: **°/s** |
| `getTemperatureCelsius()` | On-chip temperature sensor |

## License

MIT License. Developed for EdgeImpulse ESP32 WROOM lab course.
