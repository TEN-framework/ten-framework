# WebRTC VAD C++ Extension

## 概述

WebRTC VAD (Voice Activity Detection) 语音活动检测扩展，使用 C++ 语言为 TEN Framework 编写。

## 特性

- 基于 WebRTC VAD 算法的实时语音活动检测
- 支持多种采样率（8kHz, 16kHz, 32kHz, 48kHz）
- 支持多种帧长度（10ms, 20ms, 30ms）
- 可调节的检测灵敏度（模式 0-3）
- 低延迟、低资源消耗
- 输出语音检测结果的同时转发原始音频帧

## VAD 模式

扩展支持 4 种灵敏度模式：

- **模式 0**: 质量优先 - 最不激进，误报率低，但可能漏检
- **模式 1**: 低激进 - 平衡模式
- **模式 2**: 中等激进 - 默认模式，推荐用于大多数场景
- **模式 3**: 高激进 - 最激进，检出率高，但可能有更多误报

## 配置

在 `property.json` 中配置 VAD 模式：

```json
{
  "mode": 2
}
```

## 输入输出

### 输入

- **音频帧 (audio_frame)**:
  - 采样率: 8000, 16000, 32000, 或 48000 Hz
  - 位深度: 16-bit (2 bytes per sample)
  - 帧长度: 10ms, 20ms, 或 30ms
  - 声道: 支持单声道和多声道（多声道时使用第一声道）

### 输出

- **音频帧 (audio_frame)**: 转发原始音频帧，并添加以下 VAD 检测结果属性
  - `is_speech` (bool): true 表示检测到语音，false 表示静音/噪音
  - `frame_name` (string): 音频帧名称

## 使用示例

在 TEN 应用的图配置中使用此扩展：

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "webrtc_vad",
      "addon": "webrtc_vad_cpp",
      "property": {
        "mode": 2
      }
    }
  ],
  "connections": [
    {
      "extension": "audio_source",
      "audio_frame": [
        {
          "name": "audio_frame",
          "dest": [
            {
              "extension": "webrtc_vad"
            }
          ]
        }
      ]
    },
    {
      "extension": "webrtc_vad",
      "audio_frame": [
        {
          "name": "audio_frame",
          "dest": [
            {
              "extension": "downstream_processor"
            }
          ]
        }
      ]
    }
  ]
}
```

## 快速开始

### 前置条件

- TEN Framework 0.11.30 或更高版本
- C++ 编译器支持 C++11 或更高

### 安装

按照 TEN Framework 包安装指南进行安装。

## 技术细节

### WebRTC VAD 算法

此扩展使用简化版的 WebRTC VAD 算法实现：

1. **能量计算**: 计算音频帧的 RMS (均方根) 能量
2. **阈值判断**: 根据模式设置不同的能量阈值
3. **平滑处理**: 使用连续帧历史进行状态平滑，减少抖动

### 性能特点

- **低延迟**: 逐帧处理，延迟仅为一帧时长（10-30ms）
- **低资源**: 纯能量计算，无需机器学习模型
- **高效率**: C/C++ 实现，适合实时应用

## 许可证

此包是 TEN Framework 项目的一部分，遵循 Apache License 2.0。
