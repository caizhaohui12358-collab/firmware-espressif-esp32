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

如需使用其他 GPIO，直接修改 **`ei_microphone.h`** 中的宏定义即可：

```c
#define EI_MIC_I2S_SCK  14   // 自定义 SCK 引脚
#define EI_MIC_I2S_WS   15   // 自定义 WS 引脚
#define EI_MIC_I2S_SD   13   // 自定义 SD 引脚
```

### 音量调节

INMP441 采用 24-bit 数字输出，固件以 32-bit I2S 模式读取，分两步转换为 int16：

**第一步（`EI_MIC_GAIN_SHIFT = 16`，固定勿改）**  
INMP441 的 24-bit 音频在 32-bit 帧中位于 [31:8] 位，原始 int32 最大值约 ±2³¹。右移 16 位后恰好映射到 int16 范围（±32767）而不会溢出。**此值不应低于 16**，否则整数溢出会产生"绕回"噪声。

**第二步（`EI_MIC_GAIN_MUL`，可调整）**  
右移后信号较小（INMP441 灵敏度 −26 dBFS），乘以整数倍进行软件增益补偿，并配合饱和截断防止任何溢出：

信号处理链：`raw int32` → `>> 16` → **直流阻断高通（~40 Hz）** → `× MUL（饱和）` → `int16`

直流阻断滤波器会去除直流偏移和次声段漂移（HVAC、机械振动），使静音时波形趋近于零。

| `EI_MIC_GAIN_MUL` | 效果 |
|:---:|:---|
| **4**（默认）| 适合普通对话距离（0.3–0.5 m），静音底噪较低 |
| **8** | 适合安静环境或距离较远（>1 m） |
| **2** | 适合麦克风贴近声源或环境极响 |

修改位置：`ei_microphone.h`
```c
#define EI_MIC_GAIN_SHIFT  16   // 勿改，防溢出
#define EI_MIC_GAIN_MUL     4   // 调整此值改变音量
```

> **注意：** INMP441 是高灵敏度全向麦克风（-26 dBFS @ 94 dB SPL），在安静室内仍会拾取空调、电脑风扇等低频环境声。直流阻断滤波器已处理大部分漂移，但剩余环境噪声属于正常现象。

### EI 平台使用流程

1. 在 Edge Impulse Studio 创建项目，选择 **Audio** 数据类型
2. 烧录固件后，通过 `edge-impulse-daemon` 连接设备
3. 使用 **Data Acquisition** 页面采集音频样本
4. 采样率推荐 **16 kHz**，每条样本 1–2 秒
5. 训练完成后，导出 ESP32 库并替换 `model-parameters/` 目录中的文件

### 常见问题

| 现象 | 可能原因 | 解决方法 |
|---|---|---|
| 无声音时也有波形 | I2S TX 模式未关闭（旧固件） | 已修复：固件现在只启用 `I2S_MODE_RX` |
| 有声音时有爆破声 | int16 溢出绕回（wrap-around）导致 | 已修复：>> 16 保证不溢出，再乘增益系数并饱和截断 |
| 采集到全零数据 | L/R 引脚悬空 | 将 L/R 接 GND（选左声道） |
| 音量太小 | 增益不足 | 将 `EI_MIC_GAIN_SHIFT` 从 14 改为 11 |
| 音量太大 / 仍有爆音 | 增益过强 | 将 `EI_MIC_GAIN_SHIFT` 改为 16 |
| I2S 初始化失败 | 引脚冲突 | 检查所用 GPIO 是否被其他外设占用 |

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
