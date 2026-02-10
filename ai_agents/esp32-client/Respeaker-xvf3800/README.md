# ReSpeaker XVF3800 + XIAO ESP32S3 TEN Framework Client

A voice AI client that connects ReSpeaker XVF3800 with XIAO ESP32S3 to TEN Framework server for real-time multimodal conversational AI.

[中文说明](README_CN.md) | English

## Overview

This project enables the ReSpeaker XVF3800 development board (equipped with XIAO ESP32S3) to connect to TEN Framework's AI agent server, providing:

- Real-time voice conversation with AI agents (ASR + LLM + TTS)
- XVF3800 far-field voice processing with beamforming
- WebRTC-based audio streaming via Agora RTC
- Hardware button interaction (push-to-talk)
- Low-latency bidirectional audio communication

## System Architecture & Interaction Flow

### Architecture Diagram

This diagram shows how the ReSpeaker XVF3800 board with XIAO ESP32-S3 interacts with TEN-Framework server and Agora RTC:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              ReSpeaker 4-Mic Array + XIAO ESP32-S3 Board                    │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │                        Hardware Layer                                │  │
│  │                                                                      │  │
│  │  ┌─────────────┐                       ┌──────────────────────┐    │  │
│  │  │  XVF3800    │                       │   XIAO ESP32-S3      │    │  │
│  │  │  DSP Chip   │    I2C (SDA/SCL)      │   (Main MCU)         │    │  │
│  │  │             │◄─────────────────────►│                      │    │  │
│  │  │ - Buttons   │    GPIO5/GPIO6        │  - WiFi Module       │    │  │
│  │  │   (SET/     │                       │  - 8MB PSRAM         │    │  │
│  │  │    MUTE)    │                       │  - Dual-core 240MHz  │    │  │
│  │  │ - 4-Mic     │                       │                      │    │  │
│  │  │   Array     │                       │  - USB-C Power       │    │  │
│  │  │ - DSP       │                       │                      │    │  │
│  │  │   Processing│                       │                      │    │  │
│  │  └─────────────┘                       └──────────────────────┘    │  │
│  │                                                │                     │  │
│  │  ┌─────────────┐    I2S (Audio Data)         │                     │  │
│  │  │   AIC3104   │◄───────────────────────────►│                     │  │
│  │  │   (Codec)   │    BCLK/WS/DIN/DOUT          │                     │  │
│  │  │             │    GPIO8/7/44/43             │                     │  │
│  │  └─────────────┘                              │                     │  │
│  │         ▲                                     │                     │  │
│  │         │                                     │                     │  │
│  │  ┌──────┴──────┐                       ┌─────▼────────┐           │  │
│  │  │  4-Mic      │                       │   Speaker    │           │  │
│  │  │  Array      │                       │   (3.5mm)    │           │  │
│  │  │  (XVF3800)  │                       └──────────────┘           │  │
│  │  └─────────────┘                                                  │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                     Application Layer                            │  │
│  │                                                                  │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐   │  │
│  │  │ llm_main.c  │  │ ai_agent.c   │  │   audio_proc.c      │   │  │
│  │  │             │  │              │  │                     │   │  │
│  │  │ - WiFi Init │  │ - Generate   │  │ - I2S Streams       │   │  │
│  │  │ - XVF3800   │  │   Token      │  │ - AIC3104 Config    │   │  │
│  │  │   Button    │  │ - Start/Stop │  │ - Audio Pipeline    │   │  │
│  │  │   Monitor   │  │   Agent      │  │ - AEC (Disabled)    │   │  │
│  │  │ - Main Loop │  │ - HTTP API   │  └─────────┬───────────┘   │  │
│  │  └──────┬──────┘  │   Client     │            │               │  │
│  │         │         └──────┬───────┘            │               │  │
│  │         │                │                    │               │  │
│  │         └────────────────┴────────────────────┘               │  │
│  │                          │                                    │  │
│  │  ┌───────────────────────▼──────────────────────────┐        │  │
│  │  │              xvf3800.c (Button Driver)            │        │  │
│  │  │              - I2C Communication                  │        │  │
│  │  │              - Button Polling (SET/MUTE)          │        │  │
│  │  └───────────────────────┬───────────────────────────┘        │  │
│  │                          │                                    │  │
│  │                    ┌──────▼────────┐                          │  │
│  │                    │  rtc_proc.c   │                          │  │
│  │                    │               │                          │  │
│  │                    │ - Join/Leave  │                          │  │
│  │                    │   Channel     │                          │  │
│  │                    │ - RTC Events  │                          │  │
│  │                    │ - Audio TX/RX │                          │  │
│  │                    └───────┬───────┘                          │  │
│  └────────────────────────────┼──────────────────────────────────┘  │
│                                │                                     │
│  ┌─────────────────────────────▼──────────────────────────────────┐ │
│  │                    Agora IoT SDK Layer                          │ │
│  │                                                                 │ │
│  │  ┌──────────────────────────────────────────────────────────┐  │ │
│  │  │       Agora RTC API (agora_rtc_api.h)                    │  │ │
│  │  │                                                          │  │ │
│  │  │  - RTC Connection Management                            │  │ │
│  │  │  - Audio Frame Push/Pull                                │  │ │
│  │  │  - Audio Encode/Decode (G.711 μ-law)                    │  │ │
│  │  │  - Event Callbacks                                      │  │ │
│  │  └──────────────────────────────────────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
                              │ WiFi / Internet
                              │
        ┌─────────────────────┴─────────────────────┐
        │                                           │
        │                                           │
┌───────▼────────────┐                   ┌──────────▼────────────┐
│  Agora SD-RTN      │                   │  TEN-Framework        │
│                    │                   │      Server           │
│  ┌──────────────┐  │                   │  (Port: 8080)         │
│  │  SD-RTN™     │  │◄──────────────────┼─►┌─────────────────┐ │
│  │  Network     │  │   Audio Stream    │  │  HTTP API       │ │
│  │              │  │   (G.711 8kHz)    │  │  Endpoints      │ │
│  │ - Global     │  │                   │  │                 │ │
│  │   Routing    │  │                   │  │ - /token/       │ │
│  │ - Low        │  │                   │  │   generate      │ │
│  │   Latency    │  │   ┌─────────┐     │  │ - /start        │ │
│  │   (<300ms)   │  │   │ESP32 S3 │     │  │ - /stop         │ │
│  │ - QoS        │  │   │ UID:    │     │  │ - /ping         │ │
│  │              │  │   │ 12345   │     │  └─────────────────┘ │
│  │ - Encryption │  │   └─────────┘     │                       │
│  │              │  │                   │  ┌─────────────────┐ │
│  │              │  │   ┌─────────┐     │  │  Graph Manager  │ │
│  │              │  │   │TEN Agent│     │  │                 │ │
│  │              │  │   │ (In RTC │     │  │ - voice_        │ │
│  │              │  │   │ Channel)│     │  │   assistant     │ │
│  └──────────────┘  │   └─────────┘     │  │ - va_gemini_v2v │ │
│                    │                   │  │                 │ │
└────────────────────┘                   │  └────────┬────────┘ │
                                         │           │          │
                                         │  ┌────────▼────────┐ │
                                         │  │  TEN Extensions │ │
                                         │  │                 │ │
                                         │  │ ┌─────────────┐ │ │
                                         │  │ │ ASR Ext.    │ │ │
                                         │  │ │ (Speech     │ │ │
                                         │  │ │  Recognition│ │ │
                                         │  │ │ - Deepgram  │ │ │
                                         │  │ │ - Azure ASR │ │ │
                                         │  │ └─────────────┘ │ │
                                         │  │                 │ │
                                         │  │ ┌─────────────┐ │ │
                                         │  │ │ LLM Ext.    │ │ │
                                         │  │ │ (Language   │ │ │
                                         │  │ │  Model)     │ │ │
                                         │  │ │ - OpenAI    │ │ │
                                         │  │ │ - Claude    │ │ │
                                         │  │ └─────────────┘ │ │
                                         │  │                 │ │
                                         │  │ ┌─────────────┐ │ │
                                         │  │ │ TTS Ext.    │ │ │
                                         │  │ │ (Text-to-   │ │ │
                                         │  │ │  Speech)    │ │ │
                                         │  │ │ - ElevenLabs│ │ │
                                         │  │ │ - Azure TTS │ │ │
                                         │  │ └─────────────┘ │ │
                                         │  │                 │ │
                                         │  │ ┌─────────────┐ │ │
                                         │  │ │ Agora RTC   │ │ │
                                         │  │ │  Extension  │ │ │
                                         │  │ │ - Channel   │ │ │
                                         │  │ │   Management│ │ │
                                         │  │ │ - Audio I/O │ │ │
                                         │  │ └─────────────┘ │ │
                                         │  └─────────────────┘ │
                                         └───────────────────────┘

                Data Flow:
                ─────────►  Outbound: Audio from ESP32
                ◄─────────  Inbound: AI Response to ESP32
                ◄────────►  Bidirectional: Control & Signaling (HTTP)
```

### Hardware Components

| Component | Description | Interface |
|-----------|-------------|-----------|
| **XIAO ESP32-S3** | Main microcontroller with WiFi/BLE | USB-C |
| **XVF3800** | Voice front-end DSP with 4-mic array and buttons | I2C (GPIO5/6) |
| **AIC3104** | Audio codec for speaker output | I2S (GPIO8/7/44/43) |
| **4-Mic Array** | Microphone array connected to XVF3800 DSP | - |
| **Speaker** | Audio output via 3.5mm jack | Analog |

### Key Components

| Component | Description | Protocol |
|-----------|-------------|----------|
| **ReSpeaker Board** | 4-mic array with XVF3800 DSP and AIC3104 codec | I2C/I2S |
| **XIAO ESP32-S3** | Compact ESP32-S3 MCU with WiFi | - |
| **Agora IoT SDK** | Real-time communication SDK | UDP/RTC |
| **G.711U Codec** | Audio compression (8kHz, 64kbps) | PCMU |
| **TEN Framework Server** | AI agent orchestration platform | HTTP/REST |
| **Graph Manager** | Manages AI workflow graphs | - |
| **TEN Extensions** | Modular AI capabilities (ASR/LLM/TTS/RTC, etc.) | - |
| **Agora SD-RTN** | Global real-time network | Proprietary RTC |

### Communication Flow

#### 1. Initialization Phase
```
ESP32 → HTTP POST → TEN Server /token/generate
                    Request: { request_id, uid, channel_name }
                    ↓
TEN Server ← Response: { appId, token, channel_name }
                    ↓
ESP32 joins Agora RTC channel with appId and token
```

#### 2. Agent Start Phase
```
User presses XVF3800 SET button (I2C polling detects button press)
        ↓
ESP32 → HTTP POST → TEN Server /start
        Request: {
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
TEN Server starts TEN Agent instance
        ↓
TEN Agent joins Agora RTC channel
        ↓
ESP32 ← HTTP Response: { code: "0", msg: "success" }
        ↓
Bidirectional audio streaming begins
```

#### 3. Real-time Streaming Phase
```
XVF3800 4-Mic Array → DSP Processing → AIC3104 Codec → ESP32
                                                          ↓
                                            G.711U Encode → Agora RTC
                                                          ↓
                                                    TEN Agent
                                                          ↓
                                            ASR Extension (Speech Recognition)
                                                          ↓
                                            LLM Extension (AI Processing)
                                                          ↓
                                            TTS Extension (Voice Synthesis)
                                                          ↓
Speaker ← AIC3104 ← ESP32 ← G.711U Decode ← Agora RTC ← TEN Agent
```

#### 4. Agent Stop Phase
```
User presses XVF3800 MUTE button
        ↓
ESP32 → HTTP POST → TEN Server /stop
        Request: { request_id, channel_name }
        ↓
TEN Server stops TEN Agent
        ↓
TEN Agent leaves RTC channel
        ↓
ESP32 ← HTTP Response: { code: "0", msg: "success" }
```

### Audio Pipeline

```
XVF3800 4-Mic Array
    ↓ Digital Audio
XVF3800 DSP (Beamforming + Noise Reduction + Hardware AEC)
    ↓ I2S (GPIO43)
AIC3104 Codec
    ↓
I2S Stream Reader
    ↓
Audio Processing (6KB stack, no software AEC)
    ↓
RTC Encoder (G.711U)
    ↓
Agora RTC → TEN Agent → AI Processing (ASR → LLM → TTS)
    ↓
Agora RTC ← AI Response
    ↓
RTC Decoder (G.711U)
    ↓
I2S Stream Writer
    ↓ I2S (GPIO44)
AIC3104 Codec
    ↓ Analog Audio
Speaker (3.5mm)
```

## Hardware Requirements

### Main Components
- **ReSpeaker XVF3800** - Far-field voice front-end with:
  - XMOS XVF3800 voice processor
  - 2x MEMS microphones
  - TI AIC3104 audio codec
- **XIAO ESP32S3** - Main MCU with WiFi/BLE connectivity
- **USB-C Cable** - For power and programming

### Pin Connections

| Function | ESP32S3 GPIO | ReSpeaker Signal |
|----------|-------------|------------------|
| I2C SDA  | GPIO 5      | AIC3104 SDA     |
| I2C SCL  | GPIO 6      | AIC3104 SCL     |
| I2S BCLK | GPIO 8      | Audio BCLK      |
| I2S WS   | GPIO 7      | Audio LRCK/WS   |
| I2S DOUT | GPIO 44     | Audio DOUT      |
| I2S DIN  | GPIO 43     | Audio DIN       |
| BUTTON   | GPIO 1      | User Button     |

## Software Requirements

### Development Environment
- **ESP-IDF**: v5.2.3 commitId c9763f62dd00c887a1a8fafe388db868a7e44069
- **ESP-ADF**: v2.7 commitId 9cf556de500019bb79f3bb84c821fda37668c052
- **Python**: 3.8 or later
- **Git**: For cloning repositories

### TEN Framework Server
You need a running TEN Framework server with AI agent capabilities. See [TEN Framework](https://github.com/TEN-framework/TEN-Agent) for server setup.

## Installation

### 1. Install ESP-IDF

Follow the [official ESP-IDF installation guide](https://docs.espressif.com/projects/esp-idf/en/v5.2.3/esp32s3/get-started/index.html):

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
Download and run the [ESP-IDF Windows Installer](https://dl.espressif.com/dl/esp-idf/).

### 2. Install ESP-ADF

```bash
cd ~/esp
git clone -b v2.7 --recursive https://github.com/espressif/esp-adf.git
export ADF_PATH=~/esp/esp-adf
```

Add to your shell profile (`~/.bashrc`, `~/.zshrc`, or equivalent):
```bash
export ADF_PATH=~/esp/esp-adf
```

### 3. Copy ReSpeaker Board Configuration

**CRITICAL STEP:** You must copy the ReSpeaker pin configuration to ESP-ADF before building:

```bash
# Copy ReSpeaker configuration to ESP-ADF
cp board_configs/board_pins_config_respeaker.c \
   ~/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

**For Windows:**
```powershell
copy board_configs\board_pins_config_respeaker.c ^
     %ADF_PATH%\components\audio_board\esp32_s3_korvo2_v3\board_pins_config.c
```

This step configures ESP-ADF to use the correct I2C and I2S pins for ReSpeaker hardware.

### 4. Clone This Repository

```bash
cd ~/projects
git clone <your-repo-url>
cd respeaker-xvf3800-ten-client
```

## Configuration

### 1. WiFi Configuration

Edit the WiFi settings using menuconfig:

```bash
idf.py menuconfig
```

Navigate to: `Agora Demo for ESP32 -> WiFi Configuration`

Set:
- WiFi SSID: Your WiFi network name
- WiFi Password: Your WiFi password

### 2. TEN Server Configuration

In `main/app_config.h`, configure the TEN server connection:

```c
#define SERVER_URL "your-ten-server.com"
#define SERVER_PORT 8080
#define AGORA_APP_ID "your-agora-app-id"
```

### 3. Audio Configuration (Optional)

In `main/audio_proc.c`, you can adjust audio parameters:
- Sample rate: 16000 Hz (default)
- Channels: 1 (mono)
- Bits per sample: 16

## ReSpeaker XVF3800 Environment Adaptation Guide

> ⚠️ **CRITICAL**: All adaptation steps below are mandatory. Skipping any step will prevent the project from working correctly!

### Required Adaptation Steps

#### Step 1: Download Agora IoT SDK

```bash
cd components
wget https://rte-store.s3.amazonaws.com/agora_iot_sdk.tar
tar -xvf agora_iot_sdk.tar
# After extraction, you should have components/agora_iot_sdk/ with libs and include subdirectories
```

**Verification**: Confirm these files exist:
- `components/agora_iot_sdk/libs/librtsa.a`
- `components/agora_iot_sdk/libs/libahpl.a`
- `components/agora_iot_sdk/libs/libagora-cjson.a`
- `components/agora_iot_sdk/include/agora_rtc_api.h`

#### Step 2: Enable FreeRTOS Backward Compatibility

Ensure `sdkconfig.defaults` contains the following configuration:

```
CONFIG_FREERTOS_ENABLE_BACKWARD_COMPATIBILITY=y
```

**Purpose**: Resolves FreeRTOS API compatibility issues between ESP-ADF v2.7 and ESP-IDF v5.2.3.

Without this configuration, you'll encounter compilation errors like:
- `unknown type name 'xSemaphoreHandle'`
- `'portTICK_RATE_MS' undeclared`

#### Step 3: Copy ReSpeaker Pin Configuration to ESP-ADF

**THIS IS THE MOST CRITICAL STEP! Failure to execute this will cause AIC3104 initialization failure due to incorrect I2C/I2S pin configuration!**

```bash
# Linux/macOS
cp board_configs/board_pins_config_respeaker.c \
   ~/esp/esp-adf/components/audio_board/lyrat_v4_3/board_pins_config.c

# Or copy to esp32_s3_korvo2_v3 directory
cp board_configs/board_pins_config_respeaker.c \
   ~/esp/esp-adf/components/audio_board/esp32_s3_korvo2_v3/board_pins_config.c
```

**Windows:**
```powershell
copy board_configs\board_pins_config_respeaker.c ^
     %ADF_PATH%\components\audio_board\lyrat_v4_3\board_pins_config.c
```

**Why is this step absolutely necessary?**

| Item | ESP32-S3-Korvo-2 Default | ReSpeaker XVF3800 Actual |
|------|-------------------------|-------------------------|
| I2C SDA | GPIO 17 | GPIO 5 |
| I2C SCL | GPIO 18 | GPIO 6 |
| I2S BCLK | GPIO 9 | GPIO 8 |
| I2S WS | GPIO 45 | GPIO 7 |
| I2S DOUT | GPIO 8 | GPIO 44 |
| I2S DIN | GPIO 10 | GPIO 43 |

If you don't replace the pin configuration:
- ❌ AIC3104 audio codec cannot initialize via I2C
- ❌ I2S audio data transmission fails
- ❌ Although compilation succeeds, there will be NO audio functionality at runtime

#### Step 4: Configure AI Graph Selection

In `main/app_config.h`, select which AI graph to use (only one):

```c
// Uncomment the graph you want to use
#define CONFIG_GRAPH_VOICE_ASSISTANT  // ✅ Recommended: Standard voice assistant (ASR + LLM + TTS)
// #define CONFIG_GRAPH_OPENAI         // OpenAI Realtime API (audio only)
// #define CONFIG_GRAPH_GEMINI         // Gemini multimodal (video + audio, no Chinese support)
```

Also configure the TEN server address:
```c
#define TENAI_AGENT_URL "http://192.168.1.100:8080"  // Change to your server address
```

### ReSpeaker-Specific Optimizations

This project includes the following optimizations for ReSpeaker XVF3800:

| Configuration | Value/Description | Benefit |
|--------------|------------------|---------|
| **Hardware AEC** | Using XVF3800 chip | Better echo cancellation, reduced CPU usage |
| **Software AEC** | Disabled (`algo_mask = 0`) | Saves CPU and memory |
| **Audio Codec** | G.711U (8kHz PCM) | Low bandwidth, high quality, widely supported |
| **SDK Parameters** | `custom_payload_type=0`<br>`codec_unfallback=[0,8,9]` | Ensures codec stability |
| **Algorithm Stack** | 6KB | Optimized memory usage (no AEC processing) |
| **Audio Mode** | Audio-only (`CONFIG_AUDIO_ONLY`) | Reduced system load |

### Environment Variables Setup

Must execute before every build:

```bash
export ADF_PATH=~/esp/esp-adf
source ~/esp/esp-idf/export.sh
```

**Recommendation**: Add these to your shell profile (`~/.bashrc` or `~/.zshrc`):

```bash
# Add to ~/.bashrc or ~/.zshrc
export ADF_PATH=~/esp/esp-adf
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

Then you only need to run `get_idf` each time.

### Troubleshooting Table

| Symptom | Possible Cause | Solution |
|---------|---------------|----------|
| `AIC3104 init failed` | ⚠️ **Pin config not copied** | Execute Step 3, copy board_pins_config_respeaker.c |
| `agora_iot_sdk not found` | SDK not downloaded | Execute Step 1, download and extract Agora IoT SDK |
| `xSemaphoreHandle undeclared` | FreeRTOS compatibility issue | Execute Step 2, enable backward compatibility |
| `Stack overflow in algo task` | Insufficient stack space | Already fixed in audio_proc.c (6KB stack) |
| Build succeeds but no audio output | I2S pin configuration incorrect | Re-verify Step 3 was executed correctly |
| WiFi connection fails | WiFi configuration incorrect | Check SSID and password in `main/common.h` |
| Cannot connect to TEN server | Server address wrong or not running | Check URL and port in `main/app_config.h` |
| Git errors during compilation | Submodules not initialized | `git submodule update --init --recursive` |

### Pre-Build Verification Checklist

**Confirm all items before building:**

- [ ] ✅ ESP-IDF v5.2.3 installed and configured
- [ ] ✅ ESP-ADF v2.7 installed
- [ ] ✅ Environment variable `ADF_PATH` is set
- [ ] ✅ **board_pins_config_respeaker.c copied to ESP-ADF** (Most critical!)
- [ ] ✅ Agora IoT SDK downloaded to `components/agora_iot_sdk/`
- [ ] ✅ `sdkconfig.defaults` contains FreeRTOS compatibility config
- [ ] ✅ WiFi SSID and password configured in `main/common.h`
- [ ] ✅ TEN server URL configured in `main/app_config.h`
- [ ] ✅ AI graph selected (recommended: voice_assistant)
- [ ] ✅ Target chip set to ESP32S3

### Technical Background

**Why are these adaptations necessary?**

1. **Pin Differences**: ReSpeaker XVF3800 hardware design differs completely from ESP32-S3-Korvo-2
   - Different I2C pins for controlling AIC3104 audio codec
   - Different I2S pins for audio data transmission

2. **Hardware AEC**: XVF3800 chip has built-in professional far-field voice processing
   - Includes echo cancellation (AEC), beamforming, noise reduction, etc.
   - No need for ESP32 to run software AEC, saving resources

3. **FreeRTOS API Changes**: ESP-IDF v5.x made major updates to FreeRTOS APIs
   - Old type names like `xSemaphoreHandle` are deprecated
   - ESP-ADF v2.7 still uses old APIs, requiring compatibility layer

## Building and Flashing

### 1. Set Target Chip

```bash
idf.py set-target esp32s3
```

### 2. Build the Project

```bash
# Full clean build (recommended for first build)
idf.py fullclean
idf.py build
```

### 3. Flash to Device

Connect your ReSpeaker board via USB-C and flash:

**Linux/macOS:**
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

**Windows:**
```bash
idf.py -p COM3 flash monitor
```

Replace `/dev/ttyUSB0` or `COM3` with your actual port.

### 4. Monitor Serial Output

If already flashed:
```bash
idf.py -p <PORT> monitor
```

Press `Ctrl+]` to exit the monitor.

## Usage

### Startup Sequence

1. Power on the ReSpeaker board
2. Wait for WiFi connection (LED indicator)
3. Device automatically initializes AIC3104 codec
4. Device connects to TEN server via Agora RTC
5. Ready for voice interaction

### Expected Startup Logs

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

### Voice Interaction

1. **Push Button** (GPIO 1) to start conversation
2. **Speak** into the microphones
3. **AI responds** through the speaker
4. **Release button** or wait for timeout

## Project Structure

```
respeaker-xvf3800-ten-client/
├── CMakeLists.txt              # Main build configuration
├── sdkconfig.defaults          # Default ESP-IDF configuration
├── sdkconfig.defaults.esp32s3  # ESP32-S3 specific config
├── partitions.csv              # Flash partition table
├── dependencies.lock           # Component dependencies
├── board_configs/
│   └── board_pins_config_respeaker.c  # ReSpeaker pin mapping
├── components/
│   ├── agora_iot_sdk/          # Agora RTC SDK
│   └── esp32-camera/           # Camera driver (optional)
└── main/
    ├── CMakeLists.txt          # Main component build config
    ├── llm_main.c              # Main application entry
    ├── ai_agent.c/h            # AI agent protocol
    ├── audio_proc.c/h          # Audio pipeline
    ├── rtc_proc.c/h            # RTC connection handling
    ├── video_proc.c/h          # Video processing (optional)
    ├── aic3104_ng.c/h          # AIC3104 codec driver
    ├── xvf3800.c/h             # XVF3800 interface
    └── app_config.h            # Application configuration
```

## Key Features

### 1. AIC3104 Codec Driver
Custom I2C driver for TI AIC3104 audio codec:
- Legacy I2C API for ESP-ADF compatibility
- Automatic I2C bus scanning
- Volume control support
- Error recovery

### 2. XVF3800 Integration
Far-field voice processing:
- Beamforming and echo cancellation
- Push-to-talk button handling
- Audio routing to/from codec

### 3. TEN Framework Protocol
Bidirectional communication with AI agents:
- WebRTC audio streaming via Agora
- Command and control messages
- Session management

## Troubleshooting

### Build Errors

**Error: `ADF_PATH not set`**
```bash
# Set ADF_PATH environment variable
export ADF_PATH=~/esp/esp-adf
```

**Error: `audio_board not found`**
- Ensure you copied `board_pins_config_respeaker.c` to ESP-ADF
- Run `idf.py fullclean` and rebuild

### Runtime Errors

**Error: `I2C Bus WriteReg Error`**
- Check I2C connections (GPIO 5, 6)
- Verify AIC3104 power supply
- Check for I2C pull-up resistors

**Error: `No I2C devices found`**
- Hardware connection issue
- Wrong GPIO pins in board_pins_config
- AIC3104 not powered on

**Error: `WiFi connection failed`**
- Verify SSID and password in menuconfig
- Check 2.4GHz WiFi availability (ESP32 doesn't support 5GHz)

**No audio output:**
1. Check speaker connections
2. Verify AIC3104 initialization logs
3. Check I2S pin configuration
4. Test with simple audio playback

**No audio input:**
1. Verify microphone connections
2. Check XVF3800 power and initialization
3. Monitor I2S input signals
4. Test with loopback

### Network Issues

**Error: `Not enough space` in UDP buffer**
- Increase LWIP buffer size in menuconfig
- Reduce audio bitrate
- Check network quality

## Advanced Configuration

### Adjusting Audio Quality

In `main/audio_proc.c`:
```c
// Higher sample rate for better quality (more bandwidth)
#define AUDIO_SAMPLE_RATE 48000

// Stereo for spatial audio
#define AUDIO_CHANNELS 2
```

### Custom Button Actions

In `main/xvf3800.c`, modify button callback:
```c
void button_callback(void) {
    // Custom action
}
```

### Logging Configuration

In menuconfig: `Component config -> Log output`
- Set log level (Debug, Info, Warning, Error)
- Enable/disable color output
- Configure UART parameters

## Performance Optimization

### Reduce Latency
- Use lower sample rate (8000 Hz for voice)
- Minimize buffering in audio pipeline
- Optimize network settings

### Reduce Power Consumption
- Enable WiFi power save mode
- Lower CPU frequency
- Disable unused peripherals

## Hardware Adaptation Notes

This project is adapted from TEN Framework's esp32-client for ESP32-S3-Korvo-2-V3. Key changes:

1. **Codec Driver**: ES8311/ES7210 → AIC3104
2. **I2C Pins**: GPIO 17/18 → GPIO 5/6
3. **I2S Pins**: Remapped for ReSpeaker layout
4. **MCLK**: Disabled (not required by AIC3104)
5. **Board Init**: Direct codec initialization, bypassing ESP-ADF board layer

For detailed adaptation notes, see `main/XVF3800_BUTTON_IMPLEMENTATION.md`.

## Known Limitations

1. **Volume Control**: Software volume control not fully implemented
2. **MCLK**: Currently disabled, can be enabled if needed
3. **Network Buffering**: May show warnings under poor network conditions

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is based on TEN Framework, licensed under MIT License. See individual component licenses for details.

## References

- [TEN Framework](https://github.com/TEN-framework/TEN-Agent)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
- [ESP-ADF Documentation](https://docs.espressif.com/projects/esp-adf/)
- [ReSpeaker XVF3800](https://wiki.seeedstudio.com/xvf3800/)
- [TI AIC3104 Datasheet](https://www.ti.com/product/TLV320AIC3104)
- [Agora RTC SDK](https://docs.agora.io/)

## Support

For issues and questions:
- GitHub Issues: [Create an issue](<your-repo-url>/issues)
- TEN Framework: [TEN Framework Support](https://github.com/TEN-framework/TEN-Agent)

## Changelog

### Version 1.0.0 (2025-01-29)
- Initial release
- ReSpeaker XVF3800 hardware support
- AIC3104 codec driver
- XVF3800 integration
- TEN Framework client implementation
