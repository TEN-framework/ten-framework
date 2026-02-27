# TEN Framework ESP32-Client 在 ReSpeaker XVF3800 上的适配指南

本文档详细记录了将 TEN Framework 的 esp32-client 从 ESP32-S3-Korvo-2 V3 开发板适配到 ReSpeaker XVF3800 开发板的完整过程。

## 目录

- [硬件差异](#硬件差异)
- [核心问题](#核心问题)
- [完整适配步骤](#完整适配步骤)
- [文件修改清单](#文件修改清单)
- [引脚配置对照表](#引脚配置对照表)
- [编译和烧录](#编译和烧录)
- [常见问题](#常见问题)
- [验证结果](#验证结果)

---

## 硬件差异

### 原始硬件：ESP32-S3-Korvo-2 V3
- **Codec 芯片**：ES8311 (DAC) + ES7210 (ADC)
- **I2C 引脚**：SDA=GPIO17, SCL=GPIO18
- **I2S 引脚**：BCLK=GPIO9, WS=GPIO45, DOUT=GPIO8, DIN=GPIO10, MCLK=GPIO16

### 目标硬件：ReSpeaker XVF3800
- **Codec 芯片**：TI AIC3104
- **I2C 引脚**：SDA=GPIO5, SCL=GPIO6
- **I2S 引脚**：BCLK=GPIO8, WS=GPIO7, DOUT=GPIO44, DIN=GPIO43, MCLK=禁用

---

## 核心问题

### 问题 1：Codec 芯片不兼容
**现象**：
```
E (3502) I2C_BUS: I2C Bus WriteReg Error
```

**原因**：
- ESP-ADF 默认只支持 ES8311/ES7210
- 没有 AIC3104 的驱动支持
- 代码尝试用 ES8311 驱动访问 AIC3104 芯片

### 问题 2：I2C 驱动冲突
**现象**：
```
E (1002) i2c: CONFLICT! driver_ng is not allowed to be used with this old driver
```

**原因**：
- ESP-IDF 5.x 有新旧两套 I2C API
- ReSpeaker 示例项目使用新 API (`i2c_master_bus_handle_t`)
- ESP-ADF 使用旧 API (`i2c_driver_install`)
- 两者不能同时存在

### 问题 3：引脚配置不匹配
**原因**：
- ReSpeaker XVF3800 的引脚与 Korvo-2 V3 完全不同
- 需要修改 ESP-ADF 的 Board 层配置

---

## 完整适配步骤

### 步骤 1：添加 AIC3104 驱动（使用旧 I2C API）

#### 1.1 创建 `main/aic3104_ng.h`

```c
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"

// AIC3104 I2C address
#define AIC3104_ADDR 0x18

// Registers (page 0)
#define AIC3104_PAGE_CTRL        0x00
#define AIC3104_LEFT_DAC_VOLUME  0x2B
#define AIC3104_RIGHT_DAC_VOLUME 0x2C
#define AIC3104_HPLOUT_LEVEL     0x33
#define AIC3104_HPROUT_LEVEL     0x41
#define AIC3104_LEFT_LOP_LEVEL   0x56
#define AIC3104_RIGHT_LOP_LEVEL  0x5D

typedef struct {
    i2c_port_t i2c_port;
    int sda_gpio;
    int scl_gpio;
    uint32_t speed_hz;
} aic3104_ng_t;

// init I2C bus (using legacy driver)
esp_err_t aic3104_ng_init(aic3104_ng_t *ctx, int i2c_port, int sda_gpio, int scl_gpio, uint32_t speed_hz);

// low-level reg rw
esp_err_t aic3104_ng_write(aic3104_ng_t *ctx, uint8_t reg, uint8_t val);
esp_err_t aic3104_ng_read(aic3104_ng_t *ctx, uint8_t reg, uint8_t *val);

// quick sanity test: write page 0 and read it back
esp_err_t aic3104_ng_probe(aic3104_ng_t *ctx, uint8_t *page_val_out);

// apply the same minimal setup as your Arduino example
esp_err_t aic3104_ng_setup_default(aic3104_ng_t *ctx);
```

#### 1.2 创建 `main/aic3104_ng.c`

```c
#include "aic3104_ng.h"
#include "esp_log.h"

static const char *TAG = "AIC3104_NG";

// I2C bus scanner to detect devices
esp_err_t aic3104_i2c_scan(i2c_port_t i2c_port)
{
    ESP_LOGW(TAG, "Scanning I2C bus...");
    int found = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGW(TAG, "Found device at address 0x%02X", addr);
            found++;
        }
    }

    if (found == 0) {
        ESP_LOGE(TAG, "No I2C devices found!");
        return ESP_FAIL;
    }

    ESP_LOGW(TAG, "Found %d I2C device(s)", found);
    return ESP_OK;
}

esp_err_t aic3104_ng_init(aic3104_ng_t *ctx, int i2c_port, int sda_gpio, int scl_gpio, uint32_t speed_hz)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    ctx->i2c_port = i2c_port;
    ctx->sda_gpio = sda_gpio;
    ctx->scl_gpio = scl_gpio;
    ctx->speed_hz = speed_hz ? speed_hz : 100000;

    // Try to delete existing I2C driver (may already be initialized by ESP-ADF)
    i2c_driver_delete(i2c_port);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = ctx->speed_hz,
    };

    esp_err_t ret = i2c_param_config(i2c_port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGW(TAG, "init done: port=%d SDA=%d SCL=%d speed=%lu", i2c_port, sda_gpio, scl_gpio, (unsigned long)ctx->speed_hz);

    // Scan I2C bus to detect devices
    aic3104_i2c_scan(i2c_port);

    return ESP_OK;
}

esp_err_t aic3104_ng_write(aic3104_ng_t *ctx, uint8_t reg, uint8_t val)
{
    if (!ctx) return ESP_ERR_INVALID_STATE;

    uint8_t buf[2] = { reg, val };
    return i2c_master_write_to_device(ctx->i2c_port, AIC3104_ADDR, buf, sizeof(buf), pdMS_TO_TICKS(50));
}

esp_err_t aic3104_ng_read(aic3104_ng_t *ctx, uint8_t reg, uint8_t *val)
{
    if (!ctx || !val) return ESP_ERR_INVALID_ARG;

    return i2c_master_write_read_device(ctx->i2c_port, AIC3104_ADDR, &reg, 1, val, 1, pdMS_TO_TICKS(50));
}

esp_err_t aic3104_ng_probe(aic3104_ng_t *ctx, uint8_t *page_val_out)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    ESP_LOGW(TAG, "probe: write page 0");
    esp_err_t ret = aic3104_ng_write(ctx, AIC3104_PAGE_CTRL, 0x00);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write page failed: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t v = 0xFF;
    ret = aic3104_ng_read(ctx, AIC3104_PAGE_CTRL, &v);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read page failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGW(TAG, "probe ok: page reg=0x%02X", v);
    if (page_val_out) *page_val_out = v;
    return ESP_OK;
}

esp_err_t aic3104_ng_setup_default(aic3104_ng_t *ctx)
{
    if (!ctx) return ESP_ERR_INVALID_ARG;

    esp_err_t ret;

    // page 0
    ret = aic3104_ng_write(ctx, AIC3104_PAGE_CTRL, 0x00);
    if (ret != ESP_OK) return ret;

    // 0dB DAC
    ret = aic3104_ng_write(ctx, AIC3104_LEFT_DAC_VOLUME, 0x00);
    if (ret != ESP_OK) return ret;
    ret = aic3104_ng_write(ctx, AIC3104_RIGHT_DAC_VOLUME, 0x00);
    if (ret != ESP_OK) return ret;

    // outputs to 0dB, unmuted, powered up
    ret = aic3104_ng_write(ctx, AIC3104_HPLOUT_LEVEL, 0x0D);
    if (ret != ESP_OK) return ret;
    ret = aic3104_ng_write(ctx, AIC3104_HPROUT_LEVEL, 0x0D);
    if (ret != ESP_OK) return ret;

    ret = aic3104_ng_write(ctx, AIC3104_LEFT_LOP_LEVEL, 0x0B);
    if (ret != ESP_OK) return ret;
    ret = aic3104_ng_write(ctx, AIC3104_RIGHT_LOP_LEVEL, 0x0B);
    if (ret != ESP_OK) return ret;

    ESP_LOGW(TAG, "default setup applied");
    return ESP_OK;
}
```

**关键点**：
- ✅ 使用旧 I2C API (`driver/i2c.h`) 而不是新 API (`driver/i2c_master.h`)
- ✅ 在初始化前调用 `i2c_driver_delete()` 删除可能存在的旧驱动
- ✅ 添加 I2C 总线扫描功能，用于调试硬件连接

---

### 步骤 2：修改 `main/CMakeLists.txt`

**原始内容**：
```cmake
idf_component_register(SRCS llm_main.c ai_agent.c rtc_proc.c audio_proc.c video_proc.c
					REQUIRES esp32-camera audio_hal audio_pipeline audio_stream audio_board esp_peripherals esp-adf-libs
							 input_key_service esp_wifi nvs_flash agora_iot_sdk mbedtls)
```

**修改后**：
```cmake
idf_component_register(SRCS llm_main.c ai_agent.c rtc_proc.c audio_proc.c video_proc.c aic3104_ng.c
					REQUIRES esp32-camera audio_hal audio_pipeline audio_stream audio_board esp_peripherals esp-adf-libs
							 input_key_service esp_wifi nvs_flash agora_iot_sdk mbedtls)
```

**改动**：
- ✅ 在 `SRCS` 列表中添加 `aic3104_ng.c`

---

### 步骤 3：修改 `main/llm_main.c`

#### 3.1 添加头文件

在文件开头添加：
```c
#include "aic3104_ng.h"
```

完整的 include 部分应该是：
```c
#include "ai_agent.h"
#include "audio_proc.h"
#include "common.h"
#include "rtc_proc.h"
#include "aic3104_ng.h"  // 新增

#ifndef CONFIG_AUDIO_ONLY
#include "video_proc.h"
#endif
```

#### 3.2 在 `app_main()` 中添加 AIC3104 初始化

在 WiFi 连接成功后，`ai_agent_generate()` 之前添加：

```c
int app_main(void)
{
  // ... NVS 初始化、WiFi 初始化等 ...

  // Wait until WiFi is connected
  while (!g_app.b_wifi_connected) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }

  // ========== 新增：初始化 AIC3104 Codec ==========
  printf("~~~~~Initializing AIC3104 Codec~~~~\r\n");
  aic3104_ng_t aic = {0};
  ESP_ERROR_CHECK(aic3104_ng_init(&aic, I2C_NUM_0, GPIO_NUM_5, GPIO_NUM_6, 100000));

  uint8_t page = 0xFF;
  esp_err_t probe_ret = aic3104_ng_probe(&aic, &page);
  if (probe_ret == ESP_OK) {
    printf("AIC3104 detected, page register = 0x%02X\n", page);
    ESP_ERROR_CHECK(aic3104_ng_setup_default(&aic));
    printf("~~~~~AIC3104 Codec initialized successfully~~~~\r\n");
  } else {
    printf("WARNING: AIC3104 probe failed with error: %s\n", esp_err_to_name(probe_ret));
    printf("Please check:\n");
    printf("  1. Hardware connections (SDA=GPIO5, SCL=GPIO6)\n");
    printf("  2. AIC3104 power supply\n");
    printf("  3. I2C pull-up resistors\n");
    printf("Continuing without AIC3104...\n");
  }
  // ========== 新增部分结束 ==========

  ai_agent_generate();

  // ... 其余代码 ...
}
```

**关键点**：
- ✅ 使用 GPIO_NUM_5 (SDA) 和 GPIO_NUM_6 (SCL)
- ✅ I2C 速度设置为 100kHz
- ✅ 添加错误处理，probe 失败不会导致程序崩溃
- ✅ 打印详细的调试信息

---

### 步骤 4：修改 `main/audio_proc.c`

#### 4.1 注释掉 `audio_board_init()` 调用

**原始 `setup_audio()` 函数**：
```c
void setup_audio(void)
{
  board_handle = audio_board_init();
  audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
  audio_hal_set_volume(board_handle->audio_hal, 85);
}
```

**修改后**：
```c
void setup_audio(void)
{
  // AIC3104 is initialized directly in llm_main.c, no need for audio_board_init()
  // board_handle = audio_board_init();
  // audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
  // audio_hal_set_volume(board_handle->audio_hal, 85);

  printf("setup_audio: using AIC3104 direct initialization\n");
}
```

#### 4.2 修改音量控制函数

**原始函数**：
```c
int audio_get_volume(void)
{
  int volume = 0;
  audio_hal_get_volume(board_handle->audio_hal, &volume);
  return volume;
}

void audio_set_volume(int volume)
{
  audio_hal_set_volume(board_handle->audio_hal, volume);
}
```

**修改后**：
```c
int audio_get_volume(void)
{
  // Volume control should be implemented via AIC3104 driver if needed
  // For now, return a fixed value
  return 85;
}

void audio_set_volume(int volume)
{
  // Volume control should be implemented via AIC3104 driver if needed
  printf("audio_set_volume: volume=%d (AIC3104 volume control not yet implemented)\n", volume);
}
```

**说明**：
- 因为不再使用 ESP-ADF 的 `audio_hal`，音量控制暂时禁用
- 如果需要音量控制，可以通过直接写 AIC3104 寄存器实现

---

### 步骤 5：修改 ESP-ADF Board 引脚配置

这是**最关键**的一步！需要修改 ESP-ADF 框架中的 Board 层配置。

#### 5.1 文件位置

**Windows**：
```
D:\Espressif\frameworks\esp-adf\components\audio_board\esp32_s3_korvo2_v3\board_pins_config.c
```

**Linux/Mac**：
```
~/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

或者根据你的 `$ADF_PATH` 环境变量：
```
$ADF_PATH/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

#### 5.2 修改 I2C 引脚配置

找到 `get_i2c_pins()` 函数，修改为：

```c
esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config)
{
    // 注释掉原来的 Korvo-2 配置
    // AUDIO_NULL_CHECK(TAG, i2c_config, return ESP_FAIL);
    // if (port == I2C_NUM_0 || port == I2C_NUM_1) {
    //     i2c_config->sda_io_num = GPIO_NUM_17;
    //     i2c_config->scl_io_num = GPIO_NUM_18;
    // } else {
    //     i2c_config->sda_io_num = -1;
    //     i2c_config->scl_io_num = -1;
    //     ESP_LOGE(TAG, "i2c port %d is not supported", port);
    //     return ESP_FAIL;
    // }

    // ReSpeaker XVF3800 I2C 配置
    i2c_config->sda_io_num = GPIO_NUM_5;   // ReSpeaker I2C SDA
    i2c_config->scl_io_num = GPIO_NUM_6;   // ReSpeaker I2C SCL
    return ESP_OK;
}
```

#### 5.3 修改 I2S 引脚配置

找到 `get_i2s_pins()` 函数，修改为：

```c
esp_err_t get_i2s_pins(int port, board_i2s_pin_t *i2s_config)
{
    // 注释掉原来的 Korvo-2 配置
    // AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    // if (port == 0) {
    //     i2s_config->bck_io_num = GPIO_NUM_9;
    //     i2s_config->ws_io_num = GPIO_NUM_45;
    //     i2s_config->data_out_num = GPIO_NUM_8;
    //     i2s_config->data_in_num = GPIO_NUM_10;
    //     i2s_config->mck_io_num = GPIO_NUM_16;
    // } else if (port == 1) {
    //     i2s_config->bck_io_num = -1;
    //     i2s_config->ws_io_num = -1;
    //     i2s_config->data_out_num = -1;
    //     i2s_config->data_in_num = -1;
    //     i2s_config->mck_io_num = -1;
    // } else {
    //     memset(i2s_config, -1, sizeof(board_i2s_pin_t));
    //     ESP_LOGE(TAG, "i2s port %d is not supported", port);
    //     return ESP_FAIL;
    // }

    // ReSpeaker XVF3800 I2S 配置
    i2s_config->bck_io_num   = GPIO_NUM_8;   // BCLK
    i2s_config->ws_io_num    = GPIO_NUM_7;   // WS/LRCK
    i2s_config->data_out_num = GPIO_NUM_44;  // DOUT
    i2s_config->data_in_num  = GPIO_NUM_43;  // DIN
    i2s_config->mck_io_num   = -1;           // 禁用 MCLK（先保证稳定）
    return ESP_OK;
}
```

**⚠️ 重要说明**：
- 这个文件在 ESP-ADF 框架目录中，不在项目目录
- 修改后会影响所有使用这个 Board 配置的项目
- 建议备份原文件：`cp board_pins_config.c board_pins_config.c.backup`

---

## 文件修改清单

### 新增文件
1. ✅ `main/aic3104_ng.h` - AIC3104 驱动头文件
2. ✅ `main/aic3104_ng.c` - AIC3104 驱动实现

### 修改的项目文件
1. ✅ `main/CMakeLists.txt` - 添加 `aic3104_ng.c` 到编译列表
2. ✅ `main/llm_main.c` - 添加 AIC3104 初始化代码
3. ✅ `main/audio_proc.c` - 注释掉 `audio_board_init()` 调用

### 修改的 ESP-ADF 框架文件
1. ✅ `$ADF_PATH/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c` - 修改 I2C 和 I2S 引脚配置

---

## 引脚配置对照表

### I2C 引脚（用于控制 AIC3104）

| 信号 | Korvo-2 V3 | ReSpeaker XVF3800 | 修改位置 |
|------|-----------|-------------------|---------|
| SDA  | GPIO 17   | **GPIO 5**        | board_pins_config.c, llm_main.c |
| SCL  | GPIO 18   | **GPIO 6**        | board_pins_config.c, llm_main.c |

### I2S 引脚（音频数据传输）

| 信号 | Korvo-2 V3 | ReSpeaker XVF3800 | 修改位置 |
|------|-----------|-------------------|---------|
| BCLK | GPIO 9    | **GPIO 8**        | board_pins_config.c |
| WS/LRCK | GPIO 45 | **GPIO 7**       | board_pins_config.c |
| DOUT | GPIO 8    | **GPIO 44**       | board_pins_config.c |
| DIN  | GPIO 10   | **GPIO 43**       | board_pins_config.c |
| MCLK | GPIO 16   | **-1 (禁用)**     | board_pins_config.c |

### Codec 芯片

| 项目 | Korvo-2 V3 | ReSpeaker XVF3800 |
|------|-----------|-------------------|
| DAC  | ES8311    | **AIC3104**       |
| ADC  | ES7210    | **AIC3104**       |
| I2C 地址 | 0x18 (ES8311) | **0x18 (AIC3104)** |

---

## 编译和烧录

### 环境要求

- **ESP-IDF**: v5.2.3
- **ESP-ADF**: v2.7
- **Python**: 3.x
- **操作系统**: Windows 或 Linux/Mac

### 编译步骤

#### Windows

```bash
# 1. 设置环境变量（如果还没设置）
setx ADF_PATH "D:\Espressif\frameworks\esp-adf"

# 2. 打开 ESP-IDF 5.2 PowerShell（重启后生效）

# 3. 进入项目目录
cd D:\path\to\ten-framework-main 2\ai_agents\esp32-client

# 4. 设置目标芯片
idf.py set-target esp32s3

# 5. 配置项目（可选）
idf.py menuconfig
# 在 menuconfig 中配置 WiFi SSID 和密码：
# Agora Demo for ESP32 -> WiFi SSID/Password

# 6. 完全清理（推荐）
idf.py fullclean

# 7. 编译
idf.py build

# 8. 烧录并监控
idf.py -p COM<X> flash monitor
# 替换 <X> 为实际的 COM 端口号，如 COM3
```

#### Linux/Mac

```bash
# 1. 导出环境变量
export ADF_PATH=~/esp/esp-adf

# 2. 激活 IDF 环境
. $HOME/esp/esp-idf/export.sh

# 3. 进入项目目录
cd ~/path/to/ten-framework-main 2/ai_agents/esp32-client

# 4. 设置目标芯片
idf.py set-target esp32s3

# 5. 配置项目（可选）
idf.py menuconfig

# 6. 完全清理（推荐）
idf.py fullclean

# 7. 编译
idf.py build

# 8. 烧录并监控
idf.py -p /dev/ttyUSB0 flash monitor
```

### 常用命令

```bash
# 只编译不烧录
idf.py build

# 只烧录不编译
idf.py -p COM<X> flash

# 只监控串口输出
idf.py -p COM<X> monitor

# 擦除整个 Flash
idf.py -p COM<X> erase-flash

# 查看项目配置
idf.py menuconfig
```

---

## 常见问题

### Q1: 编译时出现 `i2c driver install error`

**原因**：I2C 驱动冲突，新旧 API 同时使用

**解决方案**：
- 确保 `aic3104_ng.c` 使用的是 `driver/i2c.h`（旧 API）
- 确保没有使用 `driver/i2c_master.h`（新 API）
- 检查代码中是否调用了 `i2c_driver_delete()` 删除旧驱动

### Q2: 运行时 I2C 超时 `ESP_ERR_TIMEOUT`

**可能原因**：
1. 硬件连接问题
2. 引脚配置错误
3. I2C 地址错误
4. 没有上拉电阻

**调试步骤**：
1. 检查日志中的 I2C 扫描结果：
   ```
   W (xxxx) AIC3104_NG: Scanning I2C bus...
   W (xxxx) AIC3104_NG: Found device at address 0x??
   ```
2. 如果没有检测到设备，检查硬件连接
3. 如果检测到设备但地址不是 0x18，修改 `aic3104_ng.h` 中的 `AIC3104_ADDR`

### Q3: 编译错误 `audio_board_init` 未定义

**原因**：`audio_proc.c` 中没有正确注释掉相关代码

**解决方案**：
确保 `setup_audio()` 函数中所有调用 `board_handle` 的代码都已注释

### Q4: 音频没有声音

**可能原因**：
1. AIC3104 初始化失败
2. I2S 引脚配置错误
3. 音频数据路径问题

**调试步骤**：
1. 检查日志确认 AIC3104 初始化成功：
   ```
   AIC3104 detected, page register = 0x00
   ~~~~~AIC3104 Codec initialized successfully~~~~
   ```
2. 确认 `board_pins_config.c` 中的 I2S 引脚配置正确
3. 使用示波器检查 I2S 信号线是否有波形

### Q5: 网络缓冲区错误 `Not enough space`

**现象**：
```
[7479.598][err] -1/12:Not enough space
[7480.643][err][][netc_send_udp_data:185] 28 calls suppressed
```

**原因**：这是运行时的网络问题，不影响硬件初始化

**解决方案**：
1. 增加 LWIP 缓冲区大小（`idf.py menuconfig`）
2. 检查网络质量
3. 降低音频码率

### Q6: 修改 `board_pins_config.c` 后仍然报错

**原因**：修改的文件路径不对或没有重新编译 ESP-ADF

**解决方案**：
1. 确认修改的是正确的文件（使用 `echo $ADF_PATH` 或 `echo %ADF_PATH%` 查看路径）
2. 运行 `idf.py fullclean` 完全清理
3. 重新编译 `idf.py build`

---

## 验证结果

### 成功的启动日志

如果一切配置正确，应该看到类似以下日志：

```
I (xxxx) wifi:connected with YourWiFi, aid = 1, channel 6, BW20, bssid = xx:xx:xx:xx:xx:xx
got ip: 10.103.4.61

~~~~~Initializing AIC3104 Codec~~~~
E (2342) i2c: i2c_driver_delete(457): i2c driver install error
W (2349) AIC3104_NG: init done: port=0 SDA=5 SCL=6 speed=100000
W (2355) AIC3104_NG: Scanning I2C bus...
W (2360) AIC3104_NG: Found device at address 0x18
W (2365) AIC3104_NG: Found 1 I2C device(s)
W (2370) AIC3104_NG: probe: write page 0
W (2375) AIC3104_NG: probe ok: page reg=0x00
AIC3104 detected, page register = 0x00
W (2380) AIC3104_NG: default setup applied
~~~~~AIC3104 Codec initialized successfully~~~~

I (xxxx) AUDIO_PIPELINE: Pipeline started
~~~~~agora_rtc_join_channel success~~~~
Agora: Press [SET] key to join the Ai Agent ...
```

### 关键成功标志

- ✅ `WiFi connected` - WiFi 连接成功
- ✅ `got ip: xxx.xxx.xxx.xxx` - 获取到 IP 地址
- ✅ `Found device at address 0x18` - 检测到 AIC3104 芯片
- ✅ `AIC3104 detected, page register = 0x00` - AIC3104 探测成功
- ✅ `AIC3104 Codec initialized successfully` - Codec 初始化成功
- ✅ `agora_rtc_join_channel success` - RTC 加入频道成功

---

## 总结

### 适配的核心要点

1. **Codec 驱动替换**：从 ES8311/ES7210 替换为 AIC3104
2. **I2C API 兼容**：使用旧 I2C API 避免驱动冲突
3. **引脚重新映射**：修改 Board 层配置适配新硬件
4. **跳过 Board 初始化**：直接初始化 AIC3104，不依赖 ESP-ADF Board 层

### 修改的文件统计

- **新增文件**: 2 个
- **修改项目文件**: 3 个
- **修改框架文件**: 1 个（ESP-ADF）

### 已知限制

1. **音量控制**：暂时禁用，需要通过 AIC3104 寄存器实现
2. **MCLK**：禁用状态，如果需要可以启用并配置引脚
3. **网络缓冲区**：运行时可能出现缓冲区不足警告

### 参考资源

- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.2.3/esp32s3/)
- [ESP-ADF 编程指南](https://docs.espressif.com/projects/esp-adf/zh_CN/latest/)
- [TI AIC3104 数据手册](https://www.ti.com/product/TLV320AIC3104)
- [ReSpeaker 参考项目](https://github.com/zhannn668/esp32-client-respeaker)

---

## 许可证

本适配指南基于 TEN Framework 的 MIT 许可证。

---

**文档版本**: 1.0
**创建日期**: 2025-01-05
**适用硬件**: ReSpeaker XVF3800 + ESP32-S3
**适用软件**: TEN Framework esp32-client + ESP-IDF 5.2.3 + ESP-ADF 2.7
