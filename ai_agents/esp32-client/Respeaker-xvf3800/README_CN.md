# ReSpeaker XVF3800 + XIAO ESP32S3 TEN 框架客户端

ReSpeaker XVF3800 与 XIAO ESP32S3 连接 TEN Framework 服务器的语音 AI 客户端，支持实时多模态对话 AI。

中文说明 | [English](README.md)

## 项目简介

本项目使 ReSpeaker XVF3800 开发板（搭载 XIAO ESP32S3）能够连接到 TEN Framework 的 AI 代理服务器,提供:

- 与 AI 代理的实时语音对话(ASR + LLM + TTS)
- XVF3800 远场语音处理和波束成形
- 基于 WebRTC 的音频流传输(通过 Agora RTC)
- 硬件按钮交互(按键通话)
- 低延迟双向音频通信

## 系统架构与交互流程

### 架构图

此图展示了 ReSpeaker XVF3800 开发板（搭载 XIAO ESP32-S3）如何与 TEN Framework 服务器和 Agora RTC 交互：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              ReSpeaker 4-Mic Array + XIAO ESP32-S3 Board                    │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                        硬件层                                        │  │
│  │                                                                      │  │
│  │  ┌─────────────┐                       ┌──────────────────────┐    │  │
│  │  │  XVF3800    │                       │   XIAO ESP32-S3      │    │  │
│  │  │  DSP 芯片   │    I2C (SDA/SCL)      │   (主控 MCU)         │    │  │
│  │  │             │◄─────────────────────►│                      │    │  │
│  │  │ - 按钮      │    GPIO5/GPIO6        │  - WiFi 模块         │    │  │
│  │  │   (SET/     │                       │  - 8MB PSRAM         │    │  │
│  │  │    MUTE)    │                       │  - 双核 240MHz       │    │  │
│  │  │ - 4麦克风   │                       │                      │    │  │
│  │  │   阵列      │                       │  - USB-C 供电        │    │  │
│  │  │ - DSP       │                       │                      │    │  │
│  │  │   处理      │                       │                      │    │  │
│  │  └─────────────┘                       └──────────────────────┘    │  │
│  │                                                │                     │  │
│  │  ┌─────────────┐    I2S (音频数据)           │                     │  │
│  │  │   AIC3104   │◄───────────────────────────►│                     │  │
│  │  │   (编解码器)│    BCLK/WS/DIN/DOUT          │                     │  │
│  │  │             │    GPIO8/7/44/43             │                     │  │
│  │  └─────────────┘                              │                     │  │
│  │         ▲                                     │                     │  │
│  │         │                                     │                     │  │
│  │  ┌──────┴──────┐                       ┌─────▼────────┐           │  │
│  │  │  4麦克风    │                       │   扬声器     │           │  │
│  │  │  阵列       │                       │   (3.5mm)    │           │  │
│  │  │  (XVF3800)  │                       └──────────────┘           │  │
│  │  └─────────────┘                                                  │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                     应用层                                       │  │
│  │                                                                  │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐   │  │
│  │  │ llm_main.c  │  │ ai_agent.c   │  │   audio_proc.c      │   │  │
│  │  │             │  │              │  │                     │   │  │
│  │  │ - WiFi 初始化│  │ - 生成 Token │  │ - I2S 流            │   │  │
│  │  │ - XVF3800   │  │ - 启动/停止  │  │ - AIC3104 配置      │   │  │
│  │  │   按钮监控  │  │   代理       │  │ - 音频管道          │   │  │
│  │  │ - 主循环    │  │ - HTTP API   │  │ - AEC 处理(已禁用)  │   │  │
│  │  └──────┬──────┘  │   客户端     │  └─────────┬───────────┘   │  │
│  │         │         └──────┬───────┘            │               │  │
│  │         │                │                    │               │  │
│  │         └────────────────┴────────────────────┘               │  │
│  │                          │                                    │  │
│  │  ┌───────────────────────▼──────────────────────────┐        │  │
│  │  │              xvf3800.c (按钮驱动)                 │        │  │
│  │  │              - I2C 通信                           │        │  │
│  │  │              - 按钮轮询 (SET/MUTE)                │        │  │
│  │  └───────────────────────┬───────────────────────────┘        │  │
│  │                          │                                    │  │
│  │                    ┌──────▼────────┐                          │  │
│  │                    │  rtc_proc.c   │                          │  │
│  │                    │               │                          │  │
│  │                    │ - 加入/离开   │                          │  │
│  │                    │   频道        │                          │  │
│  │                    │ - RTC 事件    │                          │  │
│  │                    │ - 音频 TX/RX  │                          │  │
│  │                    └───────┬───────┘                          │  │
│  └────────────────────────────┼──────────────────────────────────┘  │
│                                │                                     │
│  ┌─────────────────────────────▼──────────────────────────────────┐ │
│  │                    Agora IoT SDK 层                             │ │
│  │                                                                 │ │
│  │  ┌──────────────────────────────────────────────────────────┐  │ │
│  │  │       Agora RTC API (agora_rtc_api.h)                    │  │ │
│  │  │                                                          │  │ │
│  │  │  - RTC 连接管理                                         │  │ │
│  │  │  - 音频帧推送/拉取                                      │  │ │
│  │  │  - 音频编解码 (G.711 μ-law)                            │  │ │
│  │  │  - 事件回调                                             │  │ │
│  │  └──────────────────────────────────────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
                              │ WiFi / 互联网
                              │
        ┌─────────────────────┴─────────────────────┐
        │                                           │
        │                                           │
┌───────▼────────────┐                   ┌──────────▼────────────┐
│  Agora SD-RTN      │                   │  TEN-Framework        │
│                    │                   │      Server           │
│  ┌──────────────┐  │                   │  (端口: 8080)         │
│  │  SD-RTN™     │  │◄──────────────────┼─►┌─────────────────┐ │
│  │  网络        │  │   音频流          │  │  HTTP API       │ │
│  │              │  │   (G.711 8kHz)    │  │  端点           │ │
│  │ - 全球       │  │                   │  │                 │ │
│  │   路由       │  │                   │  │ - /token/       │ │
│  │ - 低延迟     │  │                   │  │   generate      │ │
│  │   (<300ms)   │  │   ┌─────────┐     │  │ - /start        │ │
│  │ - QoS        │  │   │ESP32 S3 │     │  │ - /stop         │ │
│  │              │  │   │ UID:    │     │  │ - /ping         │ │
│  │ - 加密       │  │   │ 12345   │     │  └─────────────────┘ │
│  │              │  │   └─────────┘     │                       │
│  │              │  │                   │  ┌─────────────────┐ │
│  │              │  │   ┌─────────┐     │  │  Graph 管理器   │ │
│  │              │  │   │TEN Agent│     │  │                 │ │
│  │              │  │   │ (在 RTC │     │  │ - voice_        │ │
│  │              │  │   │ 频道内) │     │  │   assistant     │ │
│  └──────────────┘  │   └─────────┘     │  │ - va_gemini_v2v │ │
│                    │                   │  │                 │ │
└────────────────────┘                   │  └────────┬────────┘ │
                                         │           │          │
                                         │  ┌────────▼────────┐ │
                                         │  │  TEN 扩展       │ │
                                         │  │                 │ │
                                         │  │ ┌─────────────┐ │ │
                                         │  │ │ ASR 扩展    │ │ │
                                         │  │ │ (语音识别)  │ │ │
                                         │  │ │ - Deepgram  │ │ │
                                         │  │ │ - Azure ASR │ │ │
                                         │  │ └─────────────┘ │ │
                                         │  │                 │ │
                                         │  │ ┌─────────────┐ │ │
                                         │  │ │ LLM 扩展    │ │ │
                                         │  │ │ (大语言模型)│ │ │
                                         │  │ │ - OpenAI    │ │ │
                                         │  │ │ - Claude    │ │ │
                                         │  │ └─────────────┘ │ │
                                         │  │                 │ │
                                         │  │ ┌─────────────┐ │ │
                                         │  │ │ TTS 扩展    │ │ │
                                         │  │ │ (语音合成)  │ │ │
                                         │  │ │ - ElevenLabs│ │ │
                                         │  │ │ - Azure TTS │ │ │
                                         │  │ └─────────────┘ │ │
                                         │  │                 │ │
                                         │  │ ┌─────────────┐ │ │
                                         │  │ │ Agora RTC   │ │ │
                                         │  │ │  扩展       │ │ │
                                         │  │ │ - 频道管理  │ │ │
                                         │  │ │ - 音频 I/O  │ │ │
                                         │  │ └─────────────┘ │ │
                                         │  └─────────────────┘ │
                                         └───────────────────────┘

                数据流:
                ─────────►  出站: ESP32 发送音频
                ◄─────────  入站: AI 响应到 ESP32
                ◄────────►  双向: 控制与信令 (HTTP)
```

### 硬件组件

| 组件 | 描述 | 接口 |
|------|------|------|
| **XIAO ESP32-S3** | 主控 MCU，支持 WiFi/BLE | USB-C |
| **XVF3800** | 语音前端 DSP，带 4 麦克风阵列和按钮 | I2C (GPIO5/6) |
| **AIC3104** | 音频编解码器，用于扬声器输出 | I2S (GPIO8/7/44/43) |
| **4麦克风阵列** | 连接到 XVF3800 DSP 的麦克风阵列 | - |
| **扬声器** | 通过 3.5mm 接口输出音频 | 模拟 |

### 关键组件

| 组件 | 描述 | 协议 |
|------|------|------|
| **ReSpeaker 开发板** | 4麦克风阵列 + XVF3800 DSP + AIC3104 编解码器 | I2C/I2S |
| **XIAO ESP32-S3** | 紧凑型 ESP32-S3 MCU，支持 WiFi | - |
| **Agora IoT SDK** | 实时通信 SDK | UDP/RTC |
| **G.711U 编解码器** | 音频压缩 (8kHz, 64kbps) | PCMU |
| **TEN Framework 服务器** | AI 代理编排平台 | HTTP/REST |
| **Graph 管理器** | 管理 AI 工作流图 | - |
| **TEN 扩展** | 模块化 AI 能力 (ASR/LLM/TTS/RTC 等) | - |
| **Agora SD-RTN** | 全球实时网络 | 专有 RTC |

### 通信流程

#### 1. 初始化阶段
```
ESP32 → HTTP POST → TEN Server /token/generate
                    请求: { request_id, uid, channel_name }
                    ↓
TEN Server ← 响应: { appId, token, channel_name }
                    ↓
ESP32 使用 appId 和 token 加入 Agora RTC 频道
```

#### 2. 代理启动阶段
```
用户按下 XVF3800 SET 按钮 (I2C 轮询检测到按键按下)
        ↓
ESP32 → HTTP POST → TEN Server /start
        请求: {
          request_id,
          channel_name: "lili",
          user_uid: 12345,
          graph_name: "voice_assistant",
          properties: {
            agora_rtc: {
              sdk_params: "{\"che.audio.custom_payload_type\":0,\"che.audio.codec_unfallback\":[0,8,9]}"
            }
          }
        }
        ↓
TEN Server 启动 TEN Agent 实例
        ↓
TEN Agent 加入 Agora RTC 频道
        ↓
ESP32 ← HTTP 响应: { code: "0", msg: "success" }
        ↓
开始双向音频流传输
```

#### 3. 实时流传输阶段
```
XVF3800 4麦克风阵列 → DSP 处理 → AIC3104 编解码器 → ESP32
                                                          ↓
                                            G.711U 编码 → Agora RTC
                                                          ↓
                                                    TEN Agent
                                                          ↓
                                            ASR 扩展 (语音识别)
                                                          ↓
                                            LLM 扩展 (AI 处理)
                                                          ↓
                                            TTS 扩展 (语音合成)
                                                          ↓
扬声器 ← AIC3104 ← ESP32 ← G.711U 解码 ← Agora RTC ← TEN Agent
```

#### 4. 代理停止阶段
```
用户按下 XVF3800 MUTE 按钮
        ↓
ESP32 → HTTP POST → TEN Server /stop
        请求: { request_id, channel_name }
        ↓
TEN Server 停止 TEN Agent
        ↓
TEN Agent 离开 RTC 频道
        ↓
ESP32 ← HTTP 响应: { code: "0", msg: "success" }
```

### 音频管道

```
XVF3800 4麦克风阵列
    ↓ 数字音频
XVF3800 DSP (波束成形 + 降噪 + 硬件AEC)
    ↓ I2S (GPIO43)
AIC3104 编解码器
    ↓
I2S 流读取器
    ↓
音频处理 (6KB栈，无软件AEC)
    ↓
RTC 编码器 (G.711U)
    ↓
Agora RTC → TEN Agent → AI 处理 (ASR → LLM → TTS)
    ↓
Agora RTC ← AI 响应
    ↓
RTC 解码器 (G.711U)
    ↓
I2S 流写入器
    ↓ I2S (GPIO44)
AIC3104 编解码器
    ↓ 模拟音频
扬声器 (3.5mm)
```

## 硬件要求

### 主要组件
- **ReSpeaker XVF3800** - 远场语音前端，包含:
  - XMOS XVF3800 语音处理器
  - 2x MEMS 麦克风
  - TI AIC3104 音频编解码器
- **XIAO ESP32S3** - 主控 MCU，支持 WiFi/BLE
- **USB-C 数据线** - 用于供电和编程

### 引脚连接

| 功能 | ESP32S3 GPIO | ReSpeaker 信号 |
|------|-------------|---------------|
| I2C SDA  | GPIO 5      | AIC3104 SDA     |
| I2C SCL  | GPIO 6      | AIC3104 SCL     |
| I2S BCLK | GPIO 8      | 音频 BCLK      |
| I2S WS   | GPIO 7      | 音频 LRCK/WS   |
| I2S DOUT | GPIO 44     | 音频 DOUT      |
| I2S DIN  | GPIO 43     | 音频 DIN       |
| BUTTON   | GPIO 1      | 用户按钮       |

## 软件要求

### 开发环境
- **ESP-IDF**: v5.2.3 commitId c9763f62dd00c887a1a8fafe388db868a7e44069
- **ESP-ADF**: v2.7 commitId 9cf556de500019bb79f3bb84c821fda37668c052
- **Python**: 3.8 或更高版本
- **Git**: 用于克隆仓库

### TEN Framework 服务器
需要运行 TEN Framework 服务器并配置 AI 代理。参见 [TEN Framework](https://github.com/TEN-framework/TEN-Agent) 了解服务器设置。

## 安装步骤

### 1. 安装 ESP-IDF

按照 [ESP-IDF 官方安装指南](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.2.3/esp32s3/get-started/index.html) 操作:

**Linux/macOS:**
```bash
mkdir -p ~/esp
cd ~/esp
git clone -b v5.2.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
. ./export.sh
```

**Windows:**
下载并运行 [ESP-IDF Windows 安装程序](https://dl.espressif.com/dl/esp-idf/)。

### 2. 安装 ESP-ADF

```bash
cd ~/esp
git clone -b v2.7 --recursive https://github.com/espressif/esp-adf.git
export ADF_PATH=~/esp/esp-adf
```

将以下内容添加到您的 shell 配置文件 (`~/.bashrc`, `~/.zshrc` 等):
```bash
export ADF_PATH=~/esp/esp-adf
```

### 3. 复制 ReSpeaker 开发板配置

**关键步骤:** 在编译前必须将 ReSpeaker 引脚配置复制到 ESP-ADF:

```bash
# 将 ReSpeaker 配置复制到 ESP-ADF
cp board_configs/board_pins_config_respeaker.c \
   ~/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

**Windows 系统:**
```powershell
copy board_configs\board_pins_config_respeaker.c ^
     %ADF_PATH%\components\audio_board\esp32_s3_korvo2_v3\board_pins_config.c
```

此步骤配置 ESP-ADF 使用 ReSpeaker 硬件的正确 I2C 和 I2S 引脚。

### 4. 克隆本仓库

```bash
cd ~/projects
git clone <your-repo-url>
cd respeaker-xvf3800-ten-client
```

## 配置

### 1. WiFi 配置

使用 menuconfig 编辑 WiFi 设置:

```bash
idf.py menuconfig
```

导航至: `Agora Demo for ESP32 -> WiFi Configuration`

设置:
- WiFi SSID: 您的 WiFi 网络名称
- WiFi Password: 您的 WiFi 密码

### 2. TEN 服务器配置

在 `main/app_config.h` 中配置 TEN 服务器连接:

```c
#define SERVER_URL "your-ten-server.com"
#define SERVER_PORT 8080
#define AGORA_APP_ID "your-agora-app-id"
```

### 3. 音频配置(可选)

在 `main/audio_proc.c` 中可以调整音频参数:
- 采样率: 16000 Hz(默认)
- 声道数: 1(单声道)
- 位深度: 16 位

## ReSpeaker XVF3800 环境适配详细指南

> ⚠️ **重要提示**: 以下所有适配步骤都是必须的，跳过任何步骤都可能导致项目无法正常运行！

### 必要的适配步骤

#### 步骤 1: 下载 Agora IoT SDK

```bash
cd components
wget https://rte-store.s3.amazonaws.com/agora_iot_sdk.tar
tar -xvf agora_iot_sdk.tar
# 解压后应该有 components/agora_iot_sdk/ 目录，包含 libs 和 include 子目录
```

**验证**: 确认以下文件存在:
- `components/agora_iot_sdk/libs/librtsa.a`
- `components/agora_iot_sdk/libs/libahpl.a`
- `components/agora_iot_sdk/libs/libagora-cjson.a`
- `components/agora_iot_sdk/include/agora_rtc_api.h`

#### 步骤 2: 启用 FreeRTOS 向后兼容性

确保 `sdkconfig.defaults` 文件包含以下配置：

```
CONFIG_FREERTOS_ENABLE_BACKWARD_COMPATIBILITY=y
```

**作用**: 解决 ESP-ADF v2.7 与 ESP-IDF v5.2.3 的 FreeRTOS API 兼容性问题。

如果没有此配置，编译时会遇到以下错误:
- `unknown type name 'xSemaphoreHandle'`
- `'portTICK_RATE_MS' undeclared`

#### 步骤 3: 确认 ReSpeaker 引脚配置已复制

**这是最关键的一步！** 必须将 ReSpeaker 的引脚配置复制到 ESP-ADF:

```bash
# Linux/macOS
cp board_configs/board_pins_config_respeaker.c \
   ~/esp/esp-adf/components/audio_board/lyrat_v4_3/board_pins_config.c

# 或者复制到 esp32_s3_korvo2_v3 目录
cp board_configs/board_pins_config_respeaker.c \
   ~/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

**为什么必须执行这一步？**

| 项目 | ESP32-S3-Korvo-2 默认 | ReSpeaker XVF3800 实际 |
|------|---------------------|---------------------|
| I2C SDA | GPIO 17 | GPIO 5 |
| I2C SCL | GPIO 18 | GPIO 6 |
| I2S BCLK | GPIO 9 | GPIO 8 |
| I2S WS | GPIO 45 | GPIO 7 |
| I2S DOUT | GPIO 8 | GPIO 44 |
| I2S DIN | GPIO 10 | GPIO 43 |

如果不替换引脚配置:
- ❌ AIC3104 音频编解码器无法通过 I2C 初始化
- ❌ I2S 音频数据传输失败
- ❌ 虽然编译成功，但运行时完全没有音频功能

#### 步骤 4: 配置 AI 图形选择

在 `main/app_config.h` 中选择要使用的 AI 图形（只能选择一个）:

```c
// 取消注释您要使用的图形
#define CONFIG_GRAPH_VOICE_ASSISTANT  // ✅ 推荐: 标准语音助手 (ASR + LLM + TTS)
// #define CONFIG_GRAPH_OPENAI         // OpenAI Realtime API（仅音频）
// #define CONFIG_GRAPH_GEMINI         // Gemini 多模态（视频+音频，不支持中文）
```

同时配置 TEN 服务器地址:
```c
#define TENAI_AGENT_URL "http://192.168.1.100:8080"  // 修改为您的服务器地址
```

### ReSpeaker 专用优化说明

本项目已针对 ReSpeaker XVF3800 完成以下优化:

| 配置项 | 值/说明 | 优势 |
|--------|---------|------|
| **硬件 AEC** | 使用 XVF3800 芯片 | 更好的回声消除效果，降低 CPU 占用 |
| **软件 AEC** | 已禁用 (`algo_mask = 0`) | 节省 CPU 和内存 |
| **音频编码** | G.711U (8kHz PCM) | 低带宽、高质量、广泛支持 |
| **SDK 参数** | `custom_payload_type=0`<br>`codec_unfallback=[0,8,9]` | 确保编解码器稳定性 |
| **算法栈大小** | 6KB | 优化后的内存使用（无 AEC 处理） |
| **音频模式** | 纯音频 (`CONFIG_AUDIO_ONLY`) | 降低系统负载 |

### 编译前环境变量设置

每次编译前必须执行:

```bash
export ADF_PATH=~/esp/esp-adf
source ~/esp/esp-idf/export.sh
```

**建议**: 将这些命令添加到您的 shell 配置文件 (`~/.bashrc` 或 `~/.zshrc`):

```bash
# 添加到 ~/.bashrc 或 ~/.zshrc
export ADF_PATH=~/esp/esp-adf
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

然后每次只需运行 `get_idf` 即可。

### 常见问题排查表

| 症状 | 可能原因 | 解决方案 |
|------|---------|---------|
| `AIC3104 init failed` | ⚠️ **引脚配置未复制** | 执行步骤 3，复制 board_pins_config_respeaker.c |
| `agora_iot_sdk not found` | SDK 未下载 | 执行步骤 1，下载并解压 Agora IoT SDK |
| `xSemaphoreHandle undeclared` | FreeRTOS 兼容性问题 | 执行步骤 2，启用向后兼容性 |
| `Stack overflow in algo task` | 栈空间不足 | 已在 audio_proc.c 中修复 (6KB 栈) |
| 编译成功但无音频输出 | I2S 引脚配置错误 | 重新检查步骤 3 是否正确执行 |
| WiFi 无法连接 | WiFi 配置错误 | 检查 `main/common.h` 中的 SSID 和密码 |
| 无法连接 TEN 服务器 | 服务器地址错误或服务器未运行 | 检查 `main/app_config.h` 中的 URL 和端口 |
| 编译时出现 Git 错误 | 子模块未初始化 | `git submodule update --init --recursive` |

### 环境验证清单

**编译前请确认所有项目:**

- [ ] ✅ ESP-IDF v5.2.3 已安装并配置
- [ ] ✅ ESP-ADF v2.7 已安装
- [ ] ✅ 环境变量 `ADF_PATH` 已设置
- [ ] ✅ **board_pins_config_respeaker.c 已复制到 ESP-ADF**（最关键！）
- [ ] ✅ Agora IoT SDK 已下载到 `components/agora_iot_sdk/`
- [ ] ✅ `sdkconfig.defaults` 包含 FreeRTOS 兼容性配置
- [ ] ✅ WiFi SSID 和密码已在 `main/common.h` 中配置
- [ ] ✅ TEN 服务器 URL 已在 `main/app_config.h` 中配置
- [ ] ✅ 已选择 AI 图形（推荐 voice_assistant）
- [ ] ✅ 目标芯片已设置为 ESP32S3

### 技术背景说明

**为什么需要这些适配？**

1. **引脚差异**: ReSpeaker XVF3800 的硬件设计与 ESP32-S3-Korvo-2 完全不同
   - 不同的 I2C 引脚用于控制 AIC3104 音频编解码器
   - 不同的 I2S 引脚用于音频数据传输

2. **硬件 AEC**: XVF3800 芯片内置专业的远场语音处理算法
   - 包括回声消除 (AEC)、波束成形、降噪等
   - 无需 ESP32 运行软件 AEC，节省资源

3. **FreeRTOS API 变更**: ESP-IDF v5.x 对 FreeRTOS API 进行了重大更新
   - 旧的类型名如 `xSemaphoreHandle` 已废弃
   - ESP-ADF v2.7 仍使用旧 API，需要兼容层

## 编译和烧录

### 1. 设置目标芯片

```bash
idf.py set-target esp32s3
```

### 2. 编译项目

```bash
# 完全清理编译(首次编译推荐)
idf.py fullclean
idf.py build
```

### 3. 烧录到设备

通过 USB-C 连接您的 ReSpeaker 开发板并烧录:

**Linux/macOS:**
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

**Windows:**
```bash
idf.py -p COM3 flash monitor
```

将 `/dev/ttyUSB0` 或 `COM3` 替换为您的实际端口。

### 4. 监控串口输出

如果已经烧录:
```bash
idf.py -p <PORT> monitor
```

按 `Ctrl+]` 退出监控。

## 使用说明

### 启动流程

1. 给 ReSpeaker 开发板上电
2. 等待 WiFi 连接(LED 指示灯)
3. 设备自动初始化 AIC3104 编解码器
4. 设备通过 Agora RTC 连接到 TEN 服务器
5. 准备进行语音交互

### 预期的启动日志

```
I (xxx) wifi:connected with YourWiFi
got ip: 192.168.1.100

~~~~~Initializing AIC3104 Codec~~~~
W (xxx) AIC3104_NG: init done: port=0 SDA=5 SCL=6 speed=100000
W (xxx) AIC3104_NG: Found device at address 0x18
W (xxx) AIC3104_NG: probe ok: page reg=0x00
AIC3104 detected, page register = 0x00
~~~~~AIC3104 Codec initialized successfully~~~~

~~~~~agora_rtc_join_channel success~~~~
Press [SET] key to join the Ai Agent ...
```

### 语音交互

1. **按下按钮** (GPIO 1) 开始对话
2. **对着麦克风说话**
3. **AI 通过扬声器回应**
4. **松开按钮**或等待超时

## 项目结构

```
respeaker-xvf3800-ten-client/
├── CMakeLists.txt              # 主构建配置
├── sdkconfig.defaults          # 默认 ESP-IDF 配置
├── sdkconfig.defaults.esp32s3  # ESP32-S3 特定配置
├── partitions.csv              # Flash 分区表
├── dependencies.lock           # 组件依赖
├── board_configs/
│   └── board_pins_config_respeaker.c  # ReSpeaker 引脚映射
├── components/
│   ├── agora_iot_sdk/          # Agora RTC SDK
│   └── esp32-camera/           # 摄像头驱动(可选)
└── main/
    ├── CMakeLists.txt          # 主组件构建配置
    ├── llm_main.c              # 主应用入口
    ├── ai_agent.c/h            # AI 代理协议
    ├── audio_proc.c/h          # 音频管道
    ├── rtc_proc.c/h            # RTC 连接处理
    ├── video_proc.c/h          # 视频处理(可选)
    ├── aic3104_ng.c/h          # AIC3104 编解码器驱动
    ├── xvf3800.c/h             # XVF3800 接口
    └── app_config.h            # 应用配置
```

## 核心功能

### 1. AIC3104 编解码器驱动
TI AIC3104 音频编解码器的自定义 I2C 驱动:
- 使用旧版 I2C API 以兼容 ESP-ADF
- 自动 I2C 总线扫描
- 音量控制支持
- 错误恢复

### 2. XVF3800 集成
远场语音处理:
- 波束成形和回声消除
- 按键通话按钮处理
- 编解码器音频路由

### 3. TEN Framework 协议
与 AI 代理的双向通信:
- 通过 Agora 的 WebRTC 音频流
- 命令和控制消息
- 会话管理

## 故障排除

### 编译错误

**错误: `ADF_PATH not set`**
```bash
# 设置 ADF_PATH 环境变量
export ADF_PATH=~/esp/esp-adf
```

**错误: `audio_board not found`**
- 确保已将 `board_pins_config_respeaker.c` 复制到 ESP-ADF
- 运行 `idf.py fullclean` 并重新编译

### 运行时错误

**错误: `I2C Bus WriteReg Error`**
- 检查 I2C 连接(GPIO 5, 6)
- 验证 AIC3104 电源供应
- 检查 I2C 上拉电阻

**错误: `No I2C devices found`**
- 硬件连接问题
- board_pins_config 中 GPIO 引脚错误
- AIC3104 未上电

**错误: `WiFi connection failed`**
- 验证 menuconfig 中的 SSID 和密码
- 检查 2.4GHz WiFi 可用性(ESP32 不支持 5GHz)

**无音频输出:**
1. 检查扬声器连接
2. 验证 AIC3104 初始化日志
3. 检查 I2S 引脚配置
4. 使用简单音频播放测试

**无音频输入:**
1. 验证麦克风连接
2. 检查 XVF3800 电源和初始化
3. 监控 I2S 输入信号
4. 使用回环测试

### 网络问题

**错误: UDP 缓冲区 `Not enough space`**
- 在 menuconfig 中增加 LWIP 缓冲区大小
- 降低音频比特率
- 检查网络质量

## 高级配置

### 调整音频质量

在 `main/audio_proc.c` 中:
```c
// 更高的采样率以获得更好的质量(更多带宽)
#define AUDIO_SAMPLE_RATE 48000

// 立体声用于空间音频
#define AUDIO_CHANNELS 2
```

### 自定义按钮动作

在 `main/xvf3800.c` 中修改按钮回调:
```c
void button_callback(void) {
    // 自定义动作
}
```

### 日志配置

在 menuconfig 中: `Component config -> Log output`
- 设置日志级别(调试、信息、警告、错误)
- 启用/禁用彩色输出
- 配置 UART 参数

## 性能优化

### 降低延迟
- 使用较低的采样率(语音使用 8000 Hz)
- 最小化音频管道缓冲
- 优化网络设置

### 降低功耗
- 启用 WiFi 省电模式
- 降低 CPU 频率
- 禁用未使用的外设

## 硬件适配说明

本项目从 TEN Framework 的 esp32-client(用于 ESP32-S3-Korvo-2-V3)适配而来。主要变更:

1. **编解码器驱动**: ES8311/ES7210 → AIC3104
2. **I2C 引脚**: GPIO 17/18 → GPIO 5/6
3. **I2S 引脚**: 为 ReSpeaker 布局重新映射
4. **MCLK**: 已禁用(AIC3104 不需要)
5. **开发板初始化**: 直接编解码器初始化,绕过 ESP-ADF 开发板层

详细适配说明请参见 `main/XVF3800_BUTTON_IMPLEMENTATION.md`。

## 已知限制

1. **音量控制**: 软件音量控制未完全实现
2. **MCLK**: 当前已禁用,如需要可以启用
3. **网络缓冲**: 在网络条件不佳时可能显示警告

## 贡献

欢迎贡献！请:
1. Fork 本仓库
2. 创建特性分支
3. 进行修改
4. 彻底测试
5. 提交 pull request

## 许可证

本项目基于 TEN Framework,采用 MIT 许可证。详见各组件的单独许可证。

## 参考资料

- [TEN Framework](https://github.com/TEN-framework/TEN-Agent)
- [ESP-IDF 文档](https://docs.espressif.com/projects/esp-idf/zh_CN/)
- [ESP-ADF 文档](https://docs.espressif.com/projects/esp-adf/zh_CN/)
- [ReSpeaker XVF3800](https://wiki.seeedstudio.com/cn/xvf3800/)
- [TI AIC3104 数据手册](https://www.ti.com/product/zh-cn/TLV320AIC3104)
- [Agora RTC SDK](https://docs.agora.io/cn/)

## 支持

如有问题:
- GitHub Issues: [创建 issue](<your-repo-url>/issues)
- TEN Framework: [TEN Framework 支持](https://github.com/TEN-framework/TEN-Agent)

## 更新日志

### 版本 1.0.0 (2025-01-29)
- 初始版本发布
- ReSpeaker XVF3800 硬件支持
- AIC3104 编解码器驱动
- XVF3800 集成
- TEN Framework 客户端实现
