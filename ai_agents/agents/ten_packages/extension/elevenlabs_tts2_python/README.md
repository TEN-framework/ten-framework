# ElevenLabs TTS Python Extension

A Text-to-Speech extension for TEN Framework using ElevenLabs API with unified WebSocket connection management.

## Features

- Real-time text-to-speech synthesis
- WebSocket-based streaming audio
- Unified connection management with automatic reconnection
- Immediate flush response for audio control
- Support for multiple voice models
- Configurable audio parameters
- Concurrency-safe implementation
- Audio dump functionality

## Architecture

This extension uses a unified connection management pattern inspired by ByteDance TTS implementation:

- **Unified Connection Loop**: Single main loop manages all WebSocket lifecycle
- **Automatic Reconnection**: Built-in reconnection using `async for websockets.connect()`
- **Request-based Reconnection**: External components can request reconnection
- **Immediate Flush Response**: Flush requests immediately disconnect and re-establish
- **Concurrency Safety**: Lock mechanisms prevent race conditions

For detailed implementation information, see [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md).

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
    "key": "your_elevenlabs_api_key",
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

The extension automatically handles WebSocket connections and audio streaming with the following capabilities:

- Real-time text input processing
- Audio data streaming with immediate flush response
- Unified error handling and recovery
- Connection health monitoring
- Automatic reconnection on network issues
- Concurrency-safe operations

### Key Methods

```python
# Initialize and start connection
client = ElevenLabsTTS2(config, ten_env, error_callback)
await client.start_connection()

# Send text for synthesis
await client.text_input_queue.put(text_input)

# Get synthesized audio
audio_data = await client.get_synthesized_audio()

# Flush current audio (immediate stop)
await client.handle_flush()

# Request reconnection
await client.request_reconnect()

# Close connection
await client.close_connection()
```

## Documentation

- [Implementation Guide](IMPLEMENTATION_GUIDE.md) - Detailed architecture and usage information
- [API Reference](manifest.json) - Complete API specification
- [Configuration](property.json) - Default configuration values
