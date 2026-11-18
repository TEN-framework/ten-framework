# Speechmatics TTS Python Extension

This extension provides text-to-speech functionality using Speechmatics TTS API.

## Features

- Low-latency speech synthesis (sub-150ms)
- High-quality, natural-sounding voices
- HTTP REST API integration
- Multiple voice options (UK and US English)
- Support for WAV and MP3 output formats
- Production-grade reliability

## Prerequisites

- Speechmatics API key
- Python 3.8+
- aiohttp package

## Configuration

The extension can be configured through your property.json:

```json
{
  "params": {
    "api_key": "your-api-key-here",
    "voice_id": "sarah",
    "output_format": "wav",
    "sample_rate": 16000,
    "base_url": "https://preview.tts.speechmatics.com"
  }
}
```

### Configuration Options

**Parameters inside `params` object:**
- `api_key` (required): Speechmatics API key
- `voice_id` (required): Voice identifier (sarah, theo, megan, jack)
- `output_format` (optional): Audio format - "wav" or "mp3" (default: "wav")
- `sample_rate` (optional): Audio sample rate in Hz (default: 16000)
- `base_url` (optional): API base URL (default: "https://preview.tts.speechmatics.com")

### Available Voices

| Voice ID | Description |
|----------|-------------|
| `sarah` | English Female (UK) |
| `theo` | English Male (UK) |
| `megan` | English Female (US) |
| `jack` | English Male (US) |

## Getting Started

### 1. Get API Key

Create an API key at the [Speechmatics Portal](https://portal.speechmatics.com/).

### 2. Set Environment Variable

```bash
export SPEECHMATICS_API_KEY=your-api-key-here
```

### 3. Configure Extension

Update your `property.json` with the desired voice and settings.

## API Details

- **Endpoint**: `https://preview.tts.speechmatics.com/generate/{voice_id}`
- **Method**: POST
- **Authentication**: Bearer token
- **Latency**: Sub-150ms
- **Sample Rate**: 16kHz mono (optimized for voice agents)

## Architecture

This extension follows the TEN Framework TTS2 HTTP extension pattern:

- `extension.py`: Main extension class inheriting from `AsyncTTS2HttpExtension`
- `speechmatics_tts.py`: Client implementation with HTTP API integration
- `config.py`: Configuration model with validation
- `addon.py`: Extension addon registration

## License

Apache 2.0

## Contributing

Contributions are welcome! Please submit issues and pull requests to the TEN Framework repository.

## Links

- [Speechmatics TTS Documentation](https://docs.speechmatics.com/text-to-speech/quickstart)
- [Speechmatics Portal](https://portal.speechmatics.com/)
- [TEN Framework](https://github.com/TEN-framework/ten-framework)
