# Groq TTS Python Extension

This extension provides text-to-speech functionality using the Groq TTS API.

## Features

- Streaming audio synthesis using Groq's TTS API
- Supports multiple voices and models
- Configurable sample rate and speed
- Built-in retry mechanism for robust connections
- WAV stream parsing to extract raw PCM audio

## Configuration

The extension can be configured through your property.json:

```json
{
  "params": {
    "api_key": "your-groq-api-key",
    "model": "playai-tts",
    "voice": "Arista-PlayAI",
    "response_format": "wav",
    "sample_rate": 16000,
    "speed": 1.0
  }
}
```

### Configuration Options

**Top-level properties:**
- `dump` (optional): Enable audio dumping for debugging (default: false)
- `dump_path` (optional): Path for audio dumps (default: extension directory + "groq_tts_in.pcm")

**Parameters inside `params` object:**
- `api_key` (required): Your Groq API key
- `voice` (required): Voice identifier to use for synthesis
- `model` (optional): TTS model to use (default: "playai-tts")
- `response_format` (optional): Audio format (default: "wav")
- `sample_rate` (optional): Audio sample rate in Hz (default: 16000)
- `speed` (optional): Speech speed multiplier (default: 1.0)

## Architecture

This extension follows the `AsyncTTS2HttpExtension` pattern:

- `extension.py`: Main extension class inheriting from `AsyncTTS2HttpExtension`
- `groq_tts.py`: Client implementation (`GroqTTSClient`) with streaming support
- `config.py`: Configuration model extending `AsyncTTS2HttpConfig`
- `wav_stream_parser.py`: Utility for parsing WAV streams
- `utils.py`: Retry logic and utility functions

The configuration uses a `params` dict to encapsulate all TTS-specific parameters, keeping the top level clean with only framework-related properties (`dump`, `dump_path`).
