# StepFun TTS Python Extension

这是 TEN Framework 的 StepFun Text-to-Speech 扩展实现。

## 功能特性

- 支持 StepFun TTS API 的所有主要功能
- 支持多种音色和语言
- 支持情感和语速调节
- 支持音频格式选择（mp3, wav, flac, opus）
- 支持采样率配置
- 完整的错误处理和重试机制
- 支持音频转储功能

## 配置参数

### 必需参数

- `api_key`: StepFun API 密钥

### 可选参数

- `base_url`: API 基础 URL（默认：https://api.stepfun.com/v1）
- `model`: 模型名称（默认：step-tts-mini）
- `voice`: 音色名称（默认：cixingnansheng）
- `response_format`: 音频格式（默认：mp3）
- `speed`: 语速（0.5-2.0，默认：1.0）
- `volume`: 音量（0.1-2.0，默认：1.0）
- `sample_rate`: 采样率（8000/16000/22050/24000，默认：24000）
- `voice_label`: 音色标签
  - `language`: 语言（粤语、四川话、日语）
  - `emotion`: 情感（高兴、非常高兴、生气、非常生气、悲伤、撒娇）
  - `style`: 语速（慢速、极慢、快速、极快）

## 使用示例

### 基本配置

```json
{
  "params": {
    "api_key": "your_stepfun_api_key",
    "model": "step-tts-mini",
    "voice": "cixingnansheng",
    "response_format": "mp3",
    "speed": 1.0,
    "volume": 1.0,
    "sample_rate": 24000
  }
}
```

### 高级配置（带音色标签）

```json
{
  "params": {
    "api_key": "your_stepfun_api_key",
    "model": "step-tts-vivid",
    "voice": "cixingnansheng",
    "response_format": "wav",
    "speed": 1.2,
    "volume": 1.5,
    "sample_rate": 24000,
    "voice_label": {
      "language": "粤语",
      "emotion": "高兴",
      "style": "慢速"
    }
  }
}
```

## 支持的模型

- `step-tts-mini`: 轻量级模型
- `step-tts-vivid`: 高质量模型

## 支持的音色

支持官方音色和开发者自生成音色，具体音色列表请参考 StepFun API 文档。

## 支持的音频格式

- `mp3`: MP3 格式（默认）
- `wav`: WAV 格式
- `flac`: FLAC 格式
- `opus`: Opus 格式

## 错误处理

扩展包含完整的错误处理机制：

- 网络错误自动重试（最多3次）
- API 密钥错误检测
- 服务不可用错误处理
- 详细的错误日志记录

## 依赖项

- `openai>=1.0.0`: OpenAI 客户端库
- `pydantic>=2.0.0`: 数据验证库

## 开发

### 运行测试

```bash
python test_stepfun_tts.py
```

### 代码结构

- `config.py`: 配置类定义
- `stepfun_tts.py`: StepFun TTS 客户端实现
- `extension.py`: TEN Framework 扩展实现
- `addon.py`: 扩展注册
- `manifest.json`: 扩展元数据
- `property.json`: 默认配置
- `requirements.txt`: Python 依赖

## 许可证

Apache License 2.0




