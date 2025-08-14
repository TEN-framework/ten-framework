# ElevenLabs TTS Python Extension

A Text-to-Speech extension for TEN Framework using ElevenLabs API.

## Features

- Real-time text-to-speech synthesis
- WebSocket-based streaming audio
- Support for multiple voice models
- Configurable audio parameters
- Automatic reconnection handling
- Audio dump functionality

## API

Refer to `api` definition in [manifest.json](manifest.json) and default values in [property.json](property.json).

## Development

### Build

Install dependencies:
```bash
pip install -r requirements.txt
```

### Unit test

Run tests using pytest:
```bash
pytest tests/
```

## Configuration

Configure the extension in `property.json`:

```json
{
  "params": {
    "api_key": "your_elevenlabs_api_key",
    "model_id": "eleven_multilingual_v2",
    "voice_id": "pNInz6obpgDQGcFmaJgB",
    "sample_rate": 16000,
    "optimize_streaming_latency": 0,
    "similarity_boost": 0.75,
    "stability": 0.5,
    "style": 0.0,
    "speaker_boost": false
  }
}
```

## Usage

The extension automatically handles WebSocket connections and audio streaming. It supports:

- Real-time text input processing
- Audio data streaming
- Error handling and recovery
- Connection health monitoring
