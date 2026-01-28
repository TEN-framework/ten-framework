# camb_tts_python

Camb.ai TTS extension for TEN Framework using the MARS-8 text-to-speech API.

## Features

- MARS-8 model family (mars-8, mars-8-flash, mars-8-instruct)
- 140+ languages supported
- Voice cloning capabilities
- Real-time HTTP streaming
- High-quality 24kHz audio output

## API

Refer to `api` definition in [manifest.json](manifest.json) and default values in [property.json](property.json).

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| api_key | string | (required) | Camb.ai API key |
| voice_id | int32 | 2681 | Voice ID (default: Attic voice) |
| language | string | "en-us" | Language code (BCP-47 format) |
| speech_model | string | "mars-8-flash" | Model selection |
| speed | float64 | 1.0 | Speech speed multiplier |
| format | string | "pcm_s16le" | Output format |
| endpoint | string | (optional) | API endpoint override |

### Available Models

- `mars-8` - Default balanced model
- `mars-8-flash` - Faster inference (recommended)
- `mars-8-instruct` - Supports user instructions

## Development

### Setup

1. Get your API key from [Camb.ai](https://camb.ai)
2. Set environment variable:
   ```bash
   export CAMB_API_KEY=your_key_here
   ```

### Build

Follow the standard TEN Framework extension build process.

### Unit test

Run tests using the standard TEN Framework testing approach.

## Resources

- [Camb.ai API Documentation](https://camb.mintlify.app/)
- [Getting Started](https://camb.mintlify.app/getting-started)
- [API Reference](https://camb.mintlify.app/api-reference/endpoint/create-tts-stream)
