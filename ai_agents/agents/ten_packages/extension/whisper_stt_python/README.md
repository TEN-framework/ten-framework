# Whisper STT Extension

A TEN Framework extension for speech-to-text (ASR) using OpenAI's Whisper model via the faster-whisper library.

## Features

- **High-Quality Transcription**: Uses OpenAI's Whisper model for accurate speech recognition
- **Optimized Performance**: Powered by faster-whisper (up to 4x faster than openai/whisper)
- **Multiple Model Sizes**: Support for tiny, base, small, medium, large models
- **Multi-Language Support**: Transcribe in 99+ languages
- **Translation**: Translate speech to English
- **CPU & GPU Support**: Run on CPU or GPU with configurable compute types
- **VAD Filtering**: Built-in voice activity detection to filter silence
- **Auto-Reconnection**: Automatic reconnection with exponential backoff
- **Audio Dumping**: Debug support for saving audio streams

## Installation

### Requirements

- Python 3.8+
- TEN Framework
- faster-whisper library

### GPU Support (Optional)

For GPU acceleration, install CUDA libraries:
- CUDA 12.x
- cuDNN 9.x

See [faster-whisper documentation](https://github.com/SYSTRAN/faster-whisper) for detailed GPU setup.

## Configuration

### Basic Configuration

```json
{
  "params": {
    "model": "base",
    "device": "cpu",
    "compute_type": "int8",
    "language": "en",
    "task": "transcribe",
    "sample_rate": 16000
  }
}
```

### Parameters

#### Model Selection
- `model`: Model size to use
  - `tiny`, `tiny.en`: Fastest, lowest accuracy (~1GB RAM)
  - `base`, `base.en`: Good balance (~1GB RAM)
  - `small`, `small.en`: Better accuracy (~2GB RAM)
  - `medium`, `medium.en`: High accuracy (~5GB RAM)
  - `large-v1`, `large-v2`, `large-v3`: Best accuracy (~10GB RAM)

#### Device & Compute
- `device`: Execution device
  - `cpu`: Run on CPU
  - `cuda`: Run on NVIDIA GPU
- `compute_type`: Precision mode
  - CPU: `int8`, `int8_float16`, `float32`
  - GPU: `int8`, `int8_float16`, `float16`

#### Language & Task
- `language`: Source language code (e.g., `en`, `zh`, `ja`, `fr`, `de`)
  - Use `auto` for automatic language detection
- `task`: Processing task
  - `transcribe`: Transcribe in source language
  - `translate`: Translate to English

#### Audio Settings
- `sample_rate`: Input audio sample rate (default: 16000 Hz)

### Advanced Configuration

```json
{
  "dump": true,
  "dump_path": "/tmp/whisper_audio",
  "finalize_mode": "disconnect",
  "silence_duration_ms": 1000,
  "params": {
    "model": "medium",
    "device": "cuda",
    "compute_type": "float16",
    "language": "auto",
    "task": "transcribe",
    "sample_rate": 16000
  }
}
```

#### Debug Options
- `dump`: Enable audio dumping for debugging (default: false)
- `dump_path`: Directory to save audio dumps (default: "/tmp")

#### Finalize Options
- `finalize_mode`: How to handle VAD end-of-speech
  - `disconnect`: Process remaining audio immediately
  - `silence`: Add silence padding before processing
- `silence_duration_ms`: Silence duration in milliseconds (default: 1000)

## Usage Examples

### English Transcription (CPU)
```json
{
  "params": {
    "model": "base",
    "device": "cpu",
    "compute_type": "int8",
    "language": "en",
    "task": "transcribe"
  }
}
```

### Multi-Language with Auto-Detection (GPU)
```json
{
  "params": {
    "model": "large-v3",
    "device": "cuda",
    "compute_type": "float16",
    "language": "auto",
    "task": "transcribe"
  }
}
```

### Translation to English
```json
{
  "params": {
    "model": "medium",
    "device": "cpu",
    "compute_type": "int8",
    "language": "zh",
    "task": "translate"
  }
}
```

## Performance

### Model Comparison

| Model | Speed | Accuracy | VRAM (GPU) | RAM (CPU) |
|-------|-------|----------|------------|-----------|
| tiny | Fastest | Basic | ~1GB | ~1GB |
| base | Fast | Good | ~1GB | ~1GB |
| small | Medium | Better | ~2GB | ~2GB |
| medium | Slower | High | ~5GB | ~5GB |
| large-v3 | Slowest | Best | ~10GB | ~10GB |

### Compute Type Comparison

| Compute Type | Speed | Quality | Device |
|--------------|-------|---------|--------|
| int8 | Fastest | Good | CPU/GPU |
| int8_float16 | Fast | Better | GPU |
| float16 | Medium | Best | GPU |
| float32 | Slowest | Best | CPU |

## Supported Languages

Whisper supports 99+ languages including:
- English (en)
- Chinese (zh)
- Spanish (es)
- French (fr)
- German (de)
- Japanese (ja)
- Korean (ko)
- Russian (ru)
- Arabic (ar)
- Hindi (hi)
- Portuguese (pt)
- Italian (it)
- And many more...

## Architecture

The extension uses:
- **faster-whisper**: Optimized Whisper implementation using CTranslate2
- **VAD Filtering**: Silero VAD model for silence detection
- **Buffer Strategy**: Keep mode with 10MB limit for timestamp accuracy
- **Reconnection Manager**: Automatic reconnection with exponential backoff

## Troubleshooting

### GPU Not Working
- Verify CUDA and cuDNN are installed correctly
- Check CUDA version compatibility with faster-whisper
- Try `device: "cpu"` to test without GPU

### Low Accuracy
- Try a larger model (e.g., `medium` or `large-v3`)
- Specify the correct language instead of `auto`
- Ensure audio quality is good (16kHz, mono, clear speech)

### High Latency
- Use a smaller model (e.g., `tiny` or `base`)
- Enable GPU acceleration
- Use `int8` compute type for faster processing

### Out of Memory
- Use a smaller model
- Switch to CPU with `int8` compute type
- Reduce buffer size in configuration

## License

This extension is part of the TEN Framework and is licensed under the Apache License 2.0.

## Credits

- OpenAI Whisper: https://github.com/openai/whisper
- faster-whisper: https://github.com/SYSTRAN/faster-whisper
- TEN Framework: https://github.com/TEN-framework/ten-framework
