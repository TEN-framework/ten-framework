# 音频文件播放器扩展 (Audio File Player Extension)

## 简介

这是一个用 Python 编写的 TEN Framework 音频文件播放器扩展。它通过 `start_play` 命令接收音频文件路径，加载 wav/mp3/pcm 等音频文件，自动转换为 16000Hz 采样率的 PCM 格式，并按照 10ms 一帧的间隔发送音频帧。

## 功能特性

- ✅ 支持多种音频格式：WAV、MP3、PCM、OGG、FLAC、M4A
- ✅ 自动转换采样率为 16000Hz
- ✅ 自动转换为单声道
- ✅ 按 10ms 间隔发送音频帧（每帧 160 采样点）
- ✅ 支持循环播放
- ✅ 通过命令控制播放（开始/停止）
- ✅ 异步实现，性能优异

## 支持的命令

### `start_play`

开始播放音频文件。

**命令属性**：

| 属性 | 类型 | 必填 | 默认值 | 说明 |
|------|------|------|--------|------|
| `file_path` | string | ✅ | - | 音频文件路径（绝对或相对路径） |
| `loop_playback` | bool | ❌ | false | 是否循环播放 |

**示例**：

```json
{
  "name": "start_play",
  "property": {
    "file_path": "/path/to/your/audio.wav",
    "loop_playback": false
  }
}
```

### `stop_play`

停止播放音频。

**示例**：

```json
{
  "name": "stop_play"
}
```

## 输出

### 音频帧输出

- **名称**: `pcm_frame`
- **格式**: INTERLEAVE
- **采样率**: 16000Hz
- **位深度**: 16-bit (2 bytes per sample)
- **声道数**: 1 (单声道)
- **每帧采样数**: 160 samples (10ms @ 16kHz)
- **每帧字节数**: 320 bytes

## 使用示例

### 1. 在 graph 中使用

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "audio_player",
      "addon": "audio_file_player_python"
    }
  ]
}
```

### 2. 发送播放命令

通过发送 `start_play` 命令来播放音频：

```json
{
  "name": "start_play",
  "property": {
    "file_path": "/path/to/audio.mp3",
    "loop_playback": true
  }
}
```

### 3. 停止播放

```json
{
  "name": "stop_play"
}
```

## 技术细节

### 音频处理流程

1. **加载音频**: 使用 `pydub` 库加载各种格式的音频文件
2. **格式转换**:
   - 采样率转换为 16000Hz
   - 声道转换为单声道
   - 格式为 16-bit PCM
3. **帧切分**: 按 10ms (160 samples) 切分音频数据
4. **定时发送**: 使用 asyncio 按 10ms 间隔发送音频帧

### 音频参数计算

- 采样率: 16000 Hz
- 帧时长: 10 ms
- 每帧采样数: 16000 × 0.01 = 160 samples
- 字节深度: 2 bytes (16-bit)
- 每帧字节数: 160 × 2 = 320 bytes

## 依赖项

- `ten_runtime_python` >= 0.11.25
- `pydub` >= 0.25.1

### 安装依赖

```bash
pip install pydub
```

**注意**: 对于 MP3 格式支持，还需要安装 `ffmpeg`:

```bash
# Ubuntu/Debian
sudo apt-get install ffmpeg

# macOS
brew install ffmpeg

# Windows
# 下载并安装 ffmpeg，然后添加到 PATH
```

## 许可证

本项目基于 Apache License 2.0 开源。

## 作者

TEN Framework Team

## 更新日志

### v0.11.25 (2025-10-29)

- 初始版本
- 支持 wav/mp3/pcm 等多种音频格式
- 实现 16kHz PCM 转换和 10ms 帧间隔发送
- 支持循环播放和命令控制
