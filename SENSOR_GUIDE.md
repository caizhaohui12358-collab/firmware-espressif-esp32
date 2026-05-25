# Sensor Guide — ESP32 Edge Impulse Firmware

本文档说明各传感器的接线方式、配置方法及在 Edge Impulse (EI) 平台上的使用注意事项。

---

## 目录

1. [INMP441 数字麦克风（I2S）](#1-inmp441-数字麦克风i2s)
2. [LIS3DHTR 三轴加速度计（I2C）](#2-lis3dhtr-三轴加速度计i2c)
3. [ADC 模拟传感器](#3-adc-模拟传感器)
4. [OV2640 / OV3660 摄像头](#4-ov2640--ov3660-摄像头)
5. [PSRAM 配置说明](#5-psram-配置说明)

---

## 1. INMP441 数字麦克风（I2S）

### 概述

INMP441 是一款全向 MEMS 数字麦克风，使用标准 I2S 接口，适合语音关键词检测、音频分类等 Edge Impulse 音频项目。

- 接口：I2S（数字）
- 采样率支持：8 kHz – 48 kHz
- 位深度：24-bit（固件以 16-bit 模式采集）
- 电源：3.3 V

### 接线图

| INMP441 引脚 | ESP32 默认 GPIO | 说明 |
|:---:|:---:|:---|
| VDD | 3.3V | 电源正极 |
| GND | GND | 电源负极 |
| SCK | GPIO **26** | 位时钟 (BCLK) |
| WS  | GPIO **32** | 字时钟 / 左右声道选择 (LRCLK) |
| SD  | GPIO **33** | 串行数据输出 |
| L/R | GND | 接地选择左声道（固件默认左通道采集） |

> **注意：** L/R 引脚必须连接，接 GND 为左声道，接 3.3V 为右声道。固件默认配置为左声道，勿悬空。

### 修改引脚

如需使用其他 GPIO，在 **`ei_microphone.h`** 的 include 之前定义以下宏即可覆盖默认值：

```c
// 在 CMakeLists.txt 中添加编译选项，或在项目配置头文件中定义
#define EI_MIC_I2S_SCK  14   // 自定义 SCK 引脚
#define EI_MIC_I2S_WS   15   // 自定义 WS 引脚
#define EI_MIC_I2S_SD   13   // 自定义 SD 引脚
```

或在 `main/CMakeLists.txt` 中追加编译定义：

```cmake
target_compile_definitions(${COMPONENT_TARGET} PUBLIC
    EI_MIC_I2S_SCK=14
    EI_MIC_I2S_WS=15
    EI_MIC_I2S_SD=13
)
```

### EI 平台使用流程

1. 在 Edge Impulse Studio 创建项目，选择 **Audio** 数据类型
2. 烧录固件后，通过 `edge-impulse-daemon` 连接设备
3. 使用 **Data Acquisition** 页面采集音频样本
4. 采样率推荐 **16 kHz**，每条样本 1–2 秒
5. 训练完成后，导出 ESP32 库并替换 `model-parameters/` 目录中的文件

### 常见问题

| 现象 | 可能原因 | 解决方法 |
|---|---|---|
| 采集到全零数据 | L/R 引脚悬空 | 将 L/R 接 GND |
| 音量极小 | 增益不足 | 代码中 `sampleBuffer[x] * 8` 的倍数可适当增大 |
| I2S 初始化失败 | 引脚冲突 | 检查所用 GPIO 是否被其他外设占用 |
| 编译报 `I2S_COMM_FORMAT_I2S` 警告 | IDF 版本差异 | ESP-IDF 5.x 中该常量已更名，可忽略警告或升级固件 |

---

## 2. LIS3DHTR 三轴加速度计（I2C）

### 接线图

| LIS3DHTR 引脚 | ESP32 默认 GPIO | 说明 |
|:---:|:---:|:---|
| VDD | 3.3V | 电源正极 |
| GND | GND | 电源负极 |
| SDA | GPIO **13** | I2C 数据 |
| SCL | GPIO **14** | I2C 时钟 |
| SDO/SA0 | GND | I2C 地址选择（接 GND → 地址 0x18；接 VDD → 0x19） |

### EI 平台使用流程

1. 在 EI Studio 选择 **Accelerometer** 数据类型
2. 三轴输出频率最高 **400 Hz**（典型使用 50–100 Hz）
3. 采集运动数据时保持设备固定安装方向

---

## 3. ADC 模拟传感器

### 接线图

| 信号 | ESP32 GPIO | 说明 |
|:---:|:---:|:---|
| 模拟输入 | GPIO **34** | ADC1_CH6，仅输入，0–3.3V |
| GND | GND | 参考地 |

> **注意：** ESP32 ADC2 在 WiFi 开启时不可用，请使用 ADC1 通道（GPIO 32–39）。

---

## 4. OV2640 / OV3660 摄像头

摄像头引脚依开发板型号不同有较大差异，请在 `edge-impulse/ingestion-sdk-platform/sensors/ei_camera.h` 中选择对应的开发板宏：

| 开发板 | 宏定义 |
|---|---|
| ESP-EYE（默认） | `CAMERA_MODEL_ESP_EYE` |
| AI-Thinker ESP-CAM | `CAMERA_MODEL_AI_THINKER` |
| FireBeetle ESP32 | `CAMERA_MODEL_FIREBEETLE_ESP32` |
| M5Stack Camera | `CAMERA_MODEL_M5STACK_WIDE` |

修改宏后重新编译即可。

---

## 5. PSRAM 配置说明

### 为什么部分板子会有编译/运行问题

本固件默认在 `sdkconfig.defaults` 中 **禁用 PSRAM**（`CONFIG_SPIRAM=n`），适用于没有外部 RAM 的普通 ESP32 开发板。

**有 PSRAM 的开发板**（如 ESP-EYE、AI-Thinker ESP-CAM）需要手动启用：

```bash
# 方法 1：通过 menuconfig 图形界面启用
idf.py menuconfig
# 进入：Component config → ESP32-specific → Support for external, SPI-connected RAM → 勾选启用

# 方法 2：直接追加到 sdkconfig.defaults
echo "CONFIG_SPIRAM=y" >> sdkconfig.defaults
echo "CONFIG_SPIRAM_MODE_QUAD=y" >> sdkconfig.defaults
echo "CONFIG_SPIRAM_SPEED_80M=y" >> sdkconfig.defaults
echo "CONFIG_SPIRAM_IGNORE_NOTFOUND=y" >> sdkconfig.defaults

# 然后重新生成 sdkconfig
rm -f sdkconfig && idf.py reconfigure
```

### sdkconfig 与 sdkconfig.defaults 的关系

| 文件 | 说明 |
|---|---|
| `sdkconfig` | **编译器实际使用的配置**，随工程一起分发，直接打开即可编译 |
| `sdkconfig.defaults` | **配置模板参考**，仅在 `sdkconfig` 不存在时自动生效；用于重置配置 |

> 直接使用 Espressif IDE 打开工程文件夹时，`sdkconfig` 已配置好，无需额外操作。
> 若需要重置为默认配置：删除 `sdkconfig`，再通过 IDE 的 SDK Configuration Editor 或命令行 `idf.py reconfigure` 重新生成。

---

## 版本信息

- ESP-IDF: v5.1.1
- Edge Impulse SDK: 参见 `edge-impulse-sdk/` 目录
- 文档更新日期: 2026-05-25
