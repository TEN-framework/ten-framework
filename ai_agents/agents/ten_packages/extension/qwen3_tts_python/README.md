# Qwen3 TTS Extension

This extension provides Text-to-Speech functionality using the Qwen3-TTS model from Alibaba.

## Features

- **Multiple TTS Modes**:
  - **Custom Voice**: Use preset premium speakers with optional instruction control
  - **Voice Clone**: Clone a voice from 3-second reference audio
  - **Voice Design**: Create custom voices using natural language descriptions

- **Multi-language Support**: Chinese, English, Japanese, Korean, German, French, Russian, Portuguese, Spanish, Italian

- **Streaming Output**: Low-latency streaming audio generation with end-to-end synthesis latency as low as 97ms

- **Instruction Control**: Natural language control for prosody and emotion

## Available Models

| Model | Features |
|-------|----------|
| Qwen3-TTS-12Hz-1.7B-VoiceDesign | Natural language voice descriptions |
| Qwen3-TTS-12Hz-1.7B-CustomVoice | 9 premium timbres with instruction control |
| Qwen3-TTS-12Hz-1.7B-Base | 3-second voice clone |
| Qwen3-TTS-12Hz-0.6B-CustomVoice | Lightweight custom voice |
| Qwen3-TTS-12Hz-0.6B-Base | Lightweight voice clone |

## Available Speakers (Custom Voice Mode)

- **Chinese/Dialects**: Vivian, Serena, Uncle_Fu, Dylan, Eric
- **English**: Ryan, Aiden
- **Japanese**: Ono_Anna
- **Korean**: Sohee

## Configuration

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `model` | string | `Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice` | Model name from HuggingFace |
| `language` | string | `English` | Output language |
| `speaker` | string | `Vivian` | Speaker voice for custom_voice mode |
| `instruct` | string | `""` | Instruction for tone/emotion control |
| `mode` | string | `custom_voice` | TTS mode: custom_voice, voice_clone, voice_design |
| `ref_audio_path` | string | `""` | Reference audio path for voice cloning |
| `ref_text` | string | `""` | Reference audio transcript |
| `voice_description` | string | `""` | Voice description for voice_design mode |
| `device` | string | `cuda:0` | Device for inference |
| `dtype` | string | `bfloat16` | Data type (bfloat16 or float16) |
| `attn_implementation` | string | `flash_attention_2` | Attention implementation |
| `sample_rate` | int | `24000` | Output audio sample rate |
| `dump` | bool | `false` | Enable audio dumping for debugging |
| `dump_path` | string | `./` | Path for dumped audio files |

### Example Configurations

#### Custom Voice with Instruction

```json
{
    "params": {
        "model": "Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice",
        "mode": "custom_voice",
        "language": "English",
        "speaker": "Ryan",
        "instruct": "Speak in a warm and friendly tone"
    }
}
```

#### Voice Cloning

```json
{
    "params": {
        "model": "Qwen/Qwen3-TTS-12Hz-1.7B-Base",
        "mode": "voice_clone",
        "language": "English",
        "ref_audio_path": "/path/to/reference.wav",
        "ref_text": "This is the transcript of the reference audio."
    }
}
```

#### Voice Design

```json
{
    "params": {
        "model": "Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign",
        "mode": "voice_design",
        "language": "English",
        "voice_description": "A warm, mature female voice with a slight British accent, speaking at a moderate pace with clear enunciation."
    }
}
```

## Requirements

- Python 3.10+
- CUDA-capable GPU with at least 8GB VRAM (for 1.7B models) or 4GB (for 0.6B models)
- FlashAttention 2 (recommended for optimal performance)

### Installation

```bash
pip install qwen-tts torch numpy pydantic

# Optional: Install FlashAttention 2 for better performance
pip install flash-attn --no-build-isolation
```

## Usage Notes

1. **First Request Latency**: The model is loaded lazily on the first TTS request, which may take a few seconds.

2. **GPU Memory**: The 1.7B models require approximately 6-8GB of GPU memory. Use the 0.6B models for lower memory requirements.

3. **Voice Cloning**: For best results, use 3-10 seconds of clear reference audio with matching transcript.

4. **Instruction Control**: Works with CustomVoice and VoiceDesign models. Base models do not support instructions.

## License

This extension is part of TEN Framework and is licensed under the Apache License, Version 2.0.

The Qwen3-TTS model is subject to its own licensing terms. Please refer to the [Qwen3-TTS repository](https://github.com/QwenLM/Qwen3-TTS) for details.
