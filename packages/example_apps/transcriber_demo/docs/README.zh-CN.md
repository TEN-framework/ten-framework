# Transcriber Demo 应用

## 概述

Transcriber Demo 是一个基于 TEN Framework 实现的实时/离线语音转写演示应用。该应用支持通过 Web 界面的麦克风进行实时语音输入，或者上传/指定离线音频文件进行转写，并实时显示转写结果。

## 特性

- ✅ **实时语音转写**：支持通过麦克风进行实时语音识别
- 📁 **离线文件转写**：支持上传或指定音频文件进行转写
- 🌐 **Web 界面**：提供友好的 Web 控制界面
- 🔄 **实时结果推送**：通过 WebSocket 实时推送转写结果
- 🎵 **多格式支持**：支持 WAV、MP3、FLAC、OGG、M4A 等多种音频格式
- 🔁 **循环播放**：支持音频文件循环播放功能

## 架构说明

该应用基于 TEN Framework 的扩展图（Extension Graph）架构，包含以下核心组件：

### 扩展组件

1. **web_audio_control_go**：Web 服务器扩展
   - 提供 HTTP 服务和 WebSocket 连接
   - 处理音频控制请求
   - 实时推送转写结果到前端

2. **audio_file_player_python**：音频播放器扩展
   - 读取音频文件
   - 将音频数据转换为 PCM 帧流
   - 支持循环播放

3. **azure_asr_python**：Azure 语音识别扩展
   - 接收 PCM 音频帧
   - 调用 Azure Speech Service 进行语音识别
   - 返回识别结果（包括临时结果和最终结果）

### 数据流

```text
用户输入（麦克风/文件）
    ↓
web_audio_control_go / audio_file_player_python
    ↓ (PCM 音频帧)
azure_asr_python
    ↓ (ASR 结果)
web_audio_control_go
    ↓ (WebSocket)
Web 前端显示
```

## 快速开始

### 前置条件

1. **运行环境**：
   - Go 1.16+ （用于 Go 扩展）
   - Python 3.10 （用于 Python 扩展）

2. **Azure Speech Service**：
   - Azure 订阅账号
   - Speech Service 的 API Key 和 Region

### 配置说明

创建 `.env` 文件或设置环境变量：

```bash
# Azure Speech Service 配置
AZURE_STT_KEY=your_azure_speech_api_key
AZURE_STT_REGION=your_azure_region  # 例如：eastus
AZURE_STT_LANGUAGE=en-US            # 语言代码，默认为 en-US
```

支持的语言代码示例：

- `en-US`：美式英语
- `zh-CN`：简体中文
- `ja-JP`：日语
- `ko-KR`：韩语

### 安装依赖

1. **安装 Python 依赖**：

```bash
# 在应用根目录下执行
python3 scripts/install_python_deps.py
```

该脚本会自动：

- 合并所有 Python 扩展的依赖
- 安装到 Python 3.10 环境

2. **构建 Go 应用**：

```bash
# 在应用根目录下执行
go run ten_packages/system/ten_runtime_go/tools/build/main.go
```

### 启动应用

```bash
# 在应用根目录下执行
./bin/start
```

应用默认监听端口（通常是 8002），启动后可以通过浏览器访问。

## 使用方法

### 方式一：实时麦克风转写

1. 在浏览器中打开 `http://localhost:8002/static/microphone.html`
2. 授权浏览器使用麦克风
3. 点击开始录音按钮
4. 开始说话，转写结果将实时显示在页面上

### 方式二：离线文件转写

1. 在浏览器中打开 `http://localhost:8002/static/index.html`
2. 选择方式：
   - **上传文件**：点击"选择文件"按钮上传本地音频文件
   - **服务器路径**：直接输入服务器上的音频文件路径
3. （可选）勾选"Loop Playback"以循环播放
4. 点击"Start Transcription"开始转写
5. 转写结果将实时显示在下方的转写结果区域

### 转写结果说明

- **临时结果**（黄色标记）：识别过程中的临时结果，可能会变化
- **最终结果**（绿色标记）：确认的最终识别结果

## 目录结构

```text
transcriber_demo/
├── main.go                          # 应用入口
├── manifest.json                    # 应用清单
├── property.json                    # 应用属性和图配置
├── bin/
│   └── start                        # 启动脚本
├── scripts/
│   └── install_python_deps.py       # Python 依赖安装脚本
├── docs/                            # 文档目录
│   ├── README.zh-CN.md
│   └── README.en-US.md
└── ten_packages/
    └── extension/
        ├── web_audio_control_go/    # Web 控制扩展
        ├── audio_file_player_python/# 音频播放器扩展
        └── azure_asr_python/        # Azure ASR 扩展
```

## 开发说明

### 修改配置

应用的图配置位于 `property.json` 文件中，可以修改：

- 扩展的连接关系
- 扩展的属性配置
- 日志输出配置

### 添加新扩展

1. 在 `property.json` 的 `nodes` 中添加新的扩展节点
2. 在 `connections` 中定义数据流连接
3. 在 `manifest.json` 的 `dependencies` 中添加依赖声明

### 调试

应用日志输出到：

- 控制台：WARNING 级别及以上
- 文件：`logs/debug.log`（DEBUG 级别及以上）

## 故障排查

### 常见问题

1. **转写结果为空或错误**
   - 检查 Azure Speech Service 配置是否正确
   - 确认 API Key 和 Region 是否有效
   - 检查网络连接

2. **无法上传文件**
   - 检查音频文件格式是否支持
   - 确认文件大小是否超出限制

3. **WebSocket 连接失败**
   - 检查应用是否正常启动
   - 确认端口没有被占用
   - 检查防火墙设置

4. **Python 依赖安装失败**
   - 确认 Python 3.10 已正确安装
   - 尝试使用国内镜像：`python3 scripts/install_python_deps.py --index-url https://pypi.tuna.tsinghua.edu.cn/simple`

## 许可证

此应用是 TEN Framework 项目的一部分，采用 Apache License 2.0 许可证。

## 相关链接

- [TEN Framework 官方文档](https://doc.theten.ai)
- [Azure Speech Service 文档](https://docs.microsoft.com/azure/cognitive-services/speech-service/)
