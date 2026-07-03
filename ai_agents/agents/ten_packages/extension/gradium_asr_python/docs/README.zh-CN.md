# Gradium ASR 扩展

## 概述

Gradium ASR 扩展使用 Gradium AI API 提供实时语音转文本转录服务。它支持基于 WebSocket 的流式传输，实现低延迟转录。

## 功能特性

- 通过 WebSocket 实现实时语音识别
- 支持多个区域（美国和欧洲）
- 语音活动检测（VAD）集成
- 可配置的音频格式（PCM、WAV、Opus）
- 低延迟流式转录
- 中间和最终转录结果

## 配置

### 环境变量

将您的 Gradium API 密钥设置为环境变量：

```bash
export GRADIUM_API_KEY=your_api_key_here
```

### 属性配置

在 `property.json` 中配置扩展：

```json
{
  "params": {
    "api_key": "${env:GRADIUM_API_KEY|}",
    "region": "us",
    "model_name": "default",
    "input_format": "pcm",
    "sample_rate": 24000,
    "language": ""
  }
}
```

### 配置参数

| 参数 | 类型 | 默认值 | 描述 |
|------|------|--------|------|
| `api_key` | string | - | Gradium API 密钥（必需）|
| `region` | string | "us" | API 区域："us" 或 "eu" |
| `model_name` | string | "default" | Gradium ASR 模型名称 |
| `input_format` | string | "pcm" | 音频格式："pcm"、"wav" 或 "opus" |
| `sample_rate` | integer | 24000 | 音频采样率（Hz）|
| `language` | string | "" | 语言代码（可选）|

## 音频要求

Gradium ASR 服务期望的音频规格如下：

- **采样率**：24,000 Hz（24 kHz）
- **格式**：PCM（或 WAV/Opus）
- **位深度**：16 位有符号整数
- **声道**：单声道（1 个声道）
- **推荐块大小**：每块 1920 个采样点（80ms）

## API 端点

### 区域 URL

- **美国区域**：`wss://us.api.gradium.ai/api/speech/asr`
- **欧洲区域**：`wss://eu.api.gradium.ai/api/speech/asr`

## 使用示例

### 在 TEN 应用图中

将 Gradium ASR 扩展添加到您的应用配置中：

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "gradium_asr",
      "addon": "gradium_asr_python",
      "extension_group": "gradium_asr_group",
      "property": {
        "params": {
          "api_key": "${env:GRADIUM_API_KEY|}",
          "region": "us",
          "model_name": "default"
        }
      }
    }
  ]
}
```

### 连接音频输入

将您的音频源连接到 Gradium ASR 扩展：

```json
{
  "connections": [
    {
      "extension_group": "audio_input_group",
      "extension": "audio_input",
      "audio_frame_out": [
        {
          "name": "pcm_frame",
          "dest": [
            {
              "extension_group": "gradium_asr_group",
              "extension": "gradium_asr"
            }
          ]
        }
      ]
    }
  ]
}
```

## 输出

扩展以标准 TEN ASR 格式输出结果：

```json
{
  "text": "转录的文本",
  "final": true,
  "start_ms": 0,
  "duration_ms": 1000,
  "language": "zh"
}
```

### 结果字段

- `text`：转录的文本内容
- `final`：布尔值，表示这是最终结果（而非中间结果）
- `start_ms`：开始时间（毫秒）
- `duration_ms`：持续时间（毫秒）
- `language`：检测到的或配置的语言

## 错误处理

扩展通过标准 TEN 错误接口提供详细的错误消息：

- 连接错误（WebSocket 连接失败）
- 身份验证错误（无效的 API 密钥）
- 转录错误（处理失败）

## 依赖项

- `websockets>=14.0` - WebSocket 客户端库
- `pydantic>=2.0.0` - 配置验证
- `ten_runtime_python>=0.11` - TEN 运行时
- `ten_ai_base>=0.7` - TEN AI 基础类

## 许可证

与 TEN 框架相同。

## 支持

如有问题：
- Gradium API：https://gradium.ai/api_docs.html
- TEN 框架：https://github.com/TEN-framework/TEN-framework
