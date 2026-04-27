# ESP32 WROOM + EdgeImpulse 传感器开发指南

> 适用于：ESP-IDF v5.1.1 · EdgeImpulse 固件平台 · ESP32 WROOM 开发板  
> 当前已支持传感器：MPU6050（六轴 IMU）、ADC 模拟传感器、摄像头

---

## 目录

1. [项目简介](#1-项目简介)
2. [硬件连接](#2-硬件连接)
3. [目录结构](#3-目录结构)
4. [传感器框架原理](#4-传感器框架原理)
5. [MPU6050 驱动说明](#5-mpu6050-驱动说明)
6. [编译与烧录](#6-编译与烧录)
7. [EdgeImpulse 数据采集流程](#7-edgeimpulse-数据采集流程)
8. [新增传感器教程](#8-新增传感器教程)
9. [常见问题](#9-常见问题)

---

## 1. 项目简介

本项目是基于 [EdgeImpulse 官方 ESP32 固件](https://github.com/edgeimpulse/firmware-espressif-esp32) 的教学定制版本，将默认的 LIS3DHTR 加速度计替换为更常见的 **MPU6050 六轴 IMU**，方便在实验课中使用低成本、易采购的传感器完成嵌入式机器学习实验。

**主要功能：**
- 通过串口 AT 指令与 EdgeImpulse Studio 通信
- 采集 MPU6050 六轴数据（加速度计 + 陀螺仪）用于手势/动作识别
- 支持 ADC 模拟传感器采集
- 支持 TensorFlow Lite 模型本地推理

---

## 2. 硬件连接

### MPU6050 接线（I2C）

| MPU6050 引脚 | ESP32 WROOM 引脚 | 说明 |
|------------|----------------|------|
| VCC        | 3.3V           | 供电（注意是 3.3V，不是 5V）|
| GND        | GND            | 地  |
| SDA        | GPIO 21        | I2C 数据线 |
| SCL        | GPIO 22        | I2C 时钟线 |
| AD0        | GND            | I2C 地址选择，接 GND → 地址 0x68 |
| INT        | 不接           | 中断引脚，本项目未使用 |

> **注意**：MPU6050 模块板上通常已有上拉电阻，无需外接。若通信不稳定，可在 SDA/SCL 与 3.3V 之间各加一个 4.7kΩ 电阻。

### ADC 模拟传感器

| 传感器引脚 | ESP32 WROOM 引脚 |
|----------|----------------|
| 信号输出   | GPIO 34（ADC1_CH6）|
| VCC      | 3.3V 或 5V     |
| GND      | GND            |

---

## 3. 目录结构

```
firmware-espressif-esp32/
│
├── main/
│   ├── main.cpp                        # 程序入口，初始化传感器
│   └── CMakeLists.txt                  # 主构建脚本
│
├── components/                         # ESP-IDF 自定义组件
│   ├── MPU6050_ESP-IDF/                # ★ MPU6050 驱动组件
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       ├── MPU6050.cpp             # 驱动实现
│   │       └── include/
│   │           └── MPU6050.h           # 驱动头文件（寄存器/枚举/类定义）
│   ├── LIS3DHTR_ESP-IDF/               # 原 LIS3DHTR 驱动（保留备用）
│   └── esp32-camera/                   # 摄像头驱动
│
├── edge-impulse/
│   └── ingestion-sdk-platform/
│       └── sensors/                    # ★ 传感器接入层（主要修改区域）
│           ├── ei_inertial_sensor.h    # 惯性传感器声明（轴数、量程、频率）
│           ├── ei_inertial_sensor.cpp  # 惯性传感器实现（读取 MPU6050）
│           ├── ei_analogsensor.h       # ADC 传感器
│           ├── ei_analogsensor.cpp
│           ├── ei_camera.h             # 摄像头
│           └── ei_fusion_sensors_config.h  # 传感器融合配置
│
├── firmware-sdk/
│   ├── ei_fusion.h                     # 传感器融合框架定义
│   └── ei_fusion.cpp                   # 传感器融合框架实现
│
├── edge-impulse-sdk/                   # EdgeImpulse ML 推理 SDK（勿修改）
├── tflite-model/                       # 编译好的 TFLite 模型
├── model-parameters/                   # 模型元数据
├── sdkconfig                           # ESP-IDF 配置（idf.py menuconfig 生成）
├── partitions.csv                      # Flash 分区表
├── CMakeLists.txt                      # 根构建脚本
└── SENSOR_GUIDE.md                     # 本文档
```

---

## 4. 传感器框架原理

EdgeImpulse 固件使用**传感器融合框架**统一管理所有传感器，新增传感器只需遵循固定接口。

### 数据流

```
MPU6050 硬件
    │  I2C（400kHz）
    ▼
MPU6050 驱动（components/MPU6050_ESP-IDF）
    │  getMotion6()  ←  一次 14 字节突发读取
    ▼
ei_inertial_sensor.cpp
    │  ei_fusion_inertial_read_data()  返回 float 数组
    ▼
ei_fusion 框架（firmware-sdk/ei_fusion.cpp）
    │  按采样频率定时调用
    ▼
EdgeImpulse Studio（通过串口 AT 指令）
```

### 核心数据结构 `ei_device_fusion_sensor_t`

每个传感器必须填写这个结构体并注册到融合框架：

```cpp
static const ei_device_fusion_sensor_t inertial_sensor = {
    "Inertial",              // Studio 中显示的传感器名称
    6,                       // 轴数（MPU6050 = 3 加速度 + 3 陀螺仪）
    { 20.0f, 62.5f, 100.0f },// 支持的采样频率（Hz）
    {                        // 每个轴的名称和单位
        {"accX", "m/s2"}, {"accY", "m/s2"}, {"accZ", "m/s2"},
        {"gyrX", "dps"},  {"gyrY", "dps"},  {"gyrZ", "dps"},
    },
    &ei_fusion_inertial_read_data,  // 读取数据的函数指针
    0
};
```

### 融合配置文件

`edge-impulse/ingestion-sdk-platform/sensors/ei_fusion_sensors_config.h`：

```c
#define NUM_FUSION_SENSORS   2   // 当前注册的传感器模块数量
#define NUM_MAX_FUSIONS      2   // 最大传感器组合数
#define FUSION_FREQUENCY  12.5f  // 多传感器融合默认频率
```

> 每新增一个传感器，`NUM_FUSION_SENSORS` 和 `NUM_MAX_FUSIONS` 需要 +1。

---

## 5. MPU6050 驱动说明

### I2C 配置（`components/MPU6050_ESP-IDF/src/include/MPU6050.h`）

```c
#define MPU6050_I2C_SDA_IO      21      // SDA 引脚
#define MPU6050_I2C_SCL_IO      22      // SCL 引脚
#define MPU6050_I2C_NUM         I2C_NUM_0
#define MPU6050_I2C_FREQ_HZ     400000  // 400kHz 快速模式
#define MPU6050_DEFAULT_ADDRESS 0x68    // AD0=GND
```

### 主要 API

```cpp
MPU6050 mpu;

mpu.begin(MPU6050_DEFAULT_ADDRESS); // 初始化，含 WHO_AM_I 验证和设备复位
mpu.isConnection();                 // 返回 bool，检查连接是否正常
mpu.setAccelRange(MPU6050_ACCEL_RANGE_2G);    // 加速度量程：2/4/8/16G
mpu.setGyroRange(MPU6050_GYRO_RANGE_250DPS);  // 陀螺仪量程：250/500/1000/2000 dps
mpu.setDLPF(MPU6050_DLPF_44HZ);              // 低通滤波器带宽
mpu.setSampleRateDivider(9);                  // 采样率 = 1000/(9+1) = 100Hz

// 读取六轴数据（推荐：一次 I2C 突发读取，效率最高）
float ax, ay, az, gx, gy, gz;
mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
// ax/ay/az 单位：g（重力加速度）
// gx/gy/gz 单位：°/s（角速度）

// 也可分开读取
mpu.getAcceleration(&ax, &ay, &az);
mpu.getGyroscope(&gx, &gy, &gz);

// 温度（调试用）
float temp = mpu.getTemperatureCelsius();
```

### 默认初始化配置

| 参数 | 默认值 | 说明 |
|------|--------|------|
| 时钟源 | X轴陀螺仪 PLL | 比内部 RC 振荡器更稳定 |
| 加速度量程 | ±2g | 灵敏度 16384 LSB/g |
| 陀螺仪量程 | ±250 dps | 灵敏度 131 LSB/(°/s) |
| DLPF | 44Hz | 滤除高频噪声 |
| 采样率 | 100Hz | SMPLRT_DIV=9 |

---

## 6. 编译与烧录

### 环境要求

- ESP-IDF **v5.1.1**（必须此版本）
- Python 3.8+

### 首次配置环境

```bash
# 安装 ESP-IDF（参考官方文档）
# https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32/get-started/

# 激活 ESP-IDF 环境
get_idf   # 或：. $IDF_PATH/export.sh
```

### 编译

```bash
cd firmware-espressif-esp32
idf.py build
```

### 烧录

```bash
# 将 /dev/ttyUSB0 替换为实际串口号
# Linux/Mac: /dev/ttyUSB0 或 /dev/ttyACM0
# Windows:   COM3 等
idf.py -p /dev/ttyUSB0 flash monitor
```

### 验证运行

烧录成功后串口会输出：

```
Hello from Edge Impulse Device SDK.
Compiled on Apr 27 2026 ...
Type AT+HELP to see a list of commands.
```

---

## 7. EdgeImpulse 数据采集流程

1. **安装 Edge Impulse CLI**
   ```bash
   npm install -g edge-impulse-cli
   ```

2. **连接设备**
   ```bash
   edge-impulse-daemon --port /dev/ttyUSB0 --baudrate 115200
   ```

3. **登录 EdgeImpulse Studio**：https://studio.edgeimpulse.com

4. **采集数据**：在 Studio 的 *Data acquisition* 页面选择传感器：
   - `Inertial`：MPU6050 六轴（accX/Y/Z + gyrX/Y/Z）
   - `ADC sensor`：模拟传感器

5. **训练模型** → **导出 Arduino/ESP-IDF 库** → 替换 `tflite-model/` 目录

6. **推理测试**：发送 AT 指令 `AT+RUNIMPULSE` 启动实时推理

---

## 8. 新增传感器教程

以新增 **DHT11 温湿度传感器** 为例，完整演示添加步骤。

### 第一步：创建驱动组件

```
components/
└── DHT11_ESP-IDF/
    ├── CMakeLists.txt
    └── src/
        ├── include/
        │   └── DHT11.h
        └── DHT11.cpp
```

**`CMakeLists.txt`**：
```cmake
if(IDF_TARGET STREQUAL "esp32" OR IDF_TARGET STREQUAL "esp32s3")
  set(COMPONENT_SRCS src/DHT11.cpp)
  set(COMPONENT_ADD_INCLUDEDIRS src/include)
  set(COMPONENT_PRIV_REQUIRES driver)
  register_component()
endif()
```

**`DHT11.h`（最简模板）**：
```cpp
#ifndef DHT11_H
#define DHT11_H
#include <stdint.h>

class DHT11 {
public:
    bool begin(int gpio_pin);
    bool isConnection(void);
    float getTemperature(void);   // 返回摄氏度
    float getHumidity(void);      // 返回 %RH
private:
    int pin;
};
#endif
```

### 第二步：创建传感器接入文件

在 `edge-impulse/ingestion-sdk-platform/sensors/` 下新建两个文件：

**`ei_env_sensor.h`**：
```cpp
#ifndef EI_ENV_SENSOR_H
#define EI_ENV_SENSOR_H

#include "firmware-sdk/ei_fusion.h"

#define ENV_AXIS_SAMPLED  2          // 温度 + 湿度

bool ei_env_sensor_init(void);
float *ei_fusion_env_read_data(int n_samples);

static const ei_device_fusion_sensor_t env_sensor = {
    "Environmental",                 // Studio 中的名称
    ENV_AXIS_SAMPLED,
    { 1.0f, 5.0f, 10.0f },          // 支持的采样频率（Hz）
    {
        {"temperature", "celsius"},
        {"humidity",    "percent"},
    },
    &ei_fusion_env_read_data,
    0
};

#endif
```

**`ei_env_sensor.cpp`**：
```cpp
#include "ei_env_sensor.h"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "edge-impulse-sdk/porting/ei_logging.h"
#include "DHT11.h"

static float env_data[ENV_AXIS_SAMPLED];
static DHT11 dht;

bool ei_env_sensor_init(void)
{
    if (!dht.begin(4)) {             // GPIO 4 接 DHT11 数据线
        EI_LOGW("DHT11 init failed\n");
        return false;
    }
    if (!ei_add_sensor_to_fusion_list(env_sensor)) {
        EI_LOGE("Failed to register Environmental sensor\n");
        return false;
    }
    return true;
}

float *ei_fusion_env_read_data(int n_samples)
{
    env_data[0] = dht.getTemperature();
    env_data[1] = dht.getHumidity();
    return env_data;
}
```

### 第三步：在 main.cpp 中注册

```cpp
// 在 main.cpp 顶部添加
#include "ei_env_sensor.h"

// 在 app_main() 中添加
if (ei_env_sensor_init() == false) {
    ei_printf("Environmental sensor initialization failed\r\n");
}
```

### 第四步：更新融合配置

`edge-impulse/ingestion-sdk-platform/sensors/ei_fusion_sensors_config.h`：

```c
// 原来是 2，每新增一个传感器 +1
#define NUM_FUSION_SENSORS   3
#define NUM_MAX_FUSIONS      3
```

### 第五步：编译验证

```bash
idf.py build
```

无报错即完成。在 EdgeImpulse Studio 的传感器列表中会出现 `Environmental` 选项。

---

### 常用传感器扩展参考

| 传感器 | 接口 | 轴/通道 | 推荐用途 |
|--------|------|---------|----------|
| DHT11/DHT22 | 单总线 GPIO | 2（温度+湿度） | 环境监测 |
| BMP280 | I2C（GPIO21/22）| 2（气压+温度） | 海拔估算 |
| MAX30102 | I2C（GPIO21/22）| 2（心率+血氧）| 健康监测 |
| INMP441 | I2S | 音频流 | 声音分类 |
| HC-SR04 | GPIO 触发+回波 | 1（距离）| 接近检测 |
| MQ-2/MQ-135 | ADC | 1（电压值）| 气体检测 |

> I2C 传感器（BMP280、MAX30102）可与 MPU6050 **共用同一 I2C 总线**（GPIO21/22），只需使用不同的 I2C 地址，无需修改总线初始化代码。

---

## 9. 常见问题

**Q：烧录后串口无输出？**  
A：检查串口号是否正确；按住 BOOT 键再上电可进入下载模式。

**Q：MPU6050 初始化失败（`Failed to connect to MPU6050`）？**  
A：
1. 用万用表确认 3.3V 和 GND 接线正确
2. 确认 AD0 引脚接 GND（地址 0x68）
3. 用 I2C 扫描代码确认设备地址：`idf.py -p PORT monitor` 后发 `AT+SENSORLIST`
4. 尝试降低 I2C 频率：将 `MPU6050_I2C_FREQ_HZ` 从 400000 改为 100000

**Q：多个 I2C 传感器能共用 GPIO21/22 吗？**  
A：可以，只要地址不冲突。但驱动初始化时第二个 `i2c_driver_install()` 会报错，需改为判断：
```cpp
if (i2c_driver_install(...) != ESP_OK) {
    // 总线已由其他传感器初始化，忽略错误继续
}
```

**Q：采样率设置后实际频率不准？**  
A：EdgeImpulse 的采样定时依赖 FreeRTOS tick，默认精度约 ±5%。如需高精度，可在 `sdkconfig` 中将 `CONFIG_FREERTOS_HZ` 从 100 改为 1000。

**Q：如何在 Studio 中只用加速度计（不用陀螺仪）？**  
A：在 EdgeImpulse Studio 的 *Create impulse* 页面，输入块选择 `Inertial` 后，取消勾选 gyrX/gyrY/gyrZ 三个轴即可，无需修改固件。

**Q：怎么更换 I2C 引脚？**  
A：修改 `components/MPU6050_ESP-IDF/src/include/MPU6050.h` 中的：
```c
#define MPU6050_I2C_SDA_IO   21   // 改为目标 GPIO
#define MPU6050_I2C_SCL_IO   22   // 改为目标 GPIO
```
重新编译烧录即可。
