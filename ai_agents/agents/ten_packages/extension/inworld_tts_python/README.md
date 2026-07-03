# Inworld TTS Extension

A TEN Framework extension for text-to-speech synthesis using Inworld's TTS 1.5 API.

## Overview

Inworld TTS provides ultra-realistic, context-aware speech synthesis with support for:
- Multiple high-quality voices
- 15 languages
- Low latency (<200ms for Max, ~120ms for Mini)
- Voice cloning capabilities
- Audio markups for emotion and non-verbals

## Models

| Model | Description | Latency | Price |
|-------|-------------|---------|-------|
| `inworld-tts-1.5-max` | Flagship model with rich, expressive speech | <200ms | $10/1M chars |
| `inworld-tts-1.5-mini` | Cost-efficient with ultra-low latency | ~120ms | $5/1M chars |

## Configuration

### Environment Variables

Set your Inworld API key:

```bash
export INWORLD_API_KEY=your_base64_encoded_api_key
```

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `api_key` | string | `${env:INWORLD_API_KEY}` | Inworld API key (Base64-encoded) |
| `model` | string | `inworld-tts-1.5-max` | Model ID |
| `voice` | string | `Ashley` | Voice ID |
| `sample_rate` | int | `24000` | Audio sample rate in Hz |
| `temperature` | float | `1.1` | Controls randomness (0.6-1.1 recommended) |
| `speaking_rate` | float | `1.0` | Playback speed (0.5-1.5) |
| `text_normalization` | string | `ON` | Expand numbers/dates (ON) or read as written (OFF) |

### Example Configuration

```json
{
  "params": {
    "api_key": "${env:INWORLD_API_KEY}",
    "model": "inworld-tts-1.5-max",
    "voice": "Ashley",
    "sample_rate": 24000,
    "temperature": 1.1,
    "speaking_rate": 1.0,
    "text_normalization": "ON"
  }
}
```

## Supported Languages

Inworld TTS 1.5 supports 15 languages including:
- English
- Spanish
- French
- German
- Italian
- Portuguese
- Chinese
- Japanese
- Korean
- Dutch
- Polish
- Russian
- Hindi
- And more

## Audio Markups

Inworld TTS supports inline audio markups for controlling emotion, emphasis, and natural sounds:

**Emotions:**
- `[happy]`, `[sad]`, `[angry]`, `[whisper]`

**Non-verbals:**
- `[cough]`, `[sigh]`, `[breathe]`, `[clear_throat]`

Example:
```
[happy] Hello! How are you today? [sigh] I've been waiting for you.
```

## API Documentation

- [Inworld TTS Documentation](https://docs.inworld.ai/docs/tts/tts)
- [API Reference](https://docs.inworld.ai/docs/tts-api/reference/)
- [Voice List](https://inworld.ai/tts)

## License

This extension is part of the TEN Framework, licensed under the Apache License 2.0.
