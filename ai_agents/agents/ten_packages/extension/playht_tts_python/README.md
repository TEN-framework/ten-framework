# playht_tts_python

PlayHT TTS Extension for TEN Framework - provides text-to-speech synthesis using PlayHT's HTTP API.

## Features

- Streaming TTS synthesis using PlayHT's audio API
- Support for multiple voice engines (PlayDialog, Play3.0, etc.)
- Support for multiple languages
- Configurable voice parameters (speed, temperature, guidance)
- Optional audio dumping for debugging
- Configurable sample rate (default: 16kHz PCM)
- Automatic API authentication

## Configuration

### Top-level Properties

- `dump` (bool): Enable audio dumping for debugging (default: false)
- `dump_path` (string): Path to save dumped audio files (default: extension directory + "playht_tts_in.pcm")

### TTS Parameters (under `params`)

All PlayHT TTS-specific settings are configured within the `params` object:

#### Required Parameters

- `api_key` (string, required): PlayHT API key
- `user_id` (string, required): PlayHT user ID

#### Optional Parameters

- `voice_engine` (string): Voice engine to use (e.g., "PlayDialog", "Play3.0")
- `protocol` (string): Protocol to use for TTS request (default: "ws" for WebSocket)
- `voice` (string): Voice ID or voice URL
- `language` (string): Language code (e.g., "ENGLISH", "SPANISH", "FRENCH")
- `format` (string): Audio format (default: "FORMAT_PCM")
- `sample_rate` (int): Sample rate in Hz (default: 16000)
- `speed` (float): Speech speed multiplier
- `seed` (int): Random seed for voice generation
- `temperature` (float): Temperature for voice generation
- `voice_guidance` (float): Voice guidance strength
- `style_guidance` (float): Style guidance strength
- `text_guidance` (float): Text guidance strength

### Example Configuration

```json
{
  "dump": false,
  "dump_path": "/tmp/playht_tts_dump",
  "params": {
    "api_key": "your-api-key-here",
    "user_id": "your-user-id-here",
    "voice_engine": "PlayDialog",
    "voice": "s3://voice-cloning-zero-shot/.../manifest.json",
    "protocol": "ws",
    "sample_rate": 16000,
    "speed": 1.0,
    "language": "ENGLISH"
  }
}
```

## Architecture

This extension follows the `AsyncTTS2HttpExtension` pattern:

- **Extension**: `PlayHTTTSExtension` - Inherits from `AsyncTTS2HttpExtension`
- **Config**: `PlayHTTTSConfig` - Extends `AsyncTTS2HttpConfig`, stores configuration with `params` as a dict
- **Client**: `PlayHTTTSClient` - Extends `AsyncTTS2HttpClient`, handles PlayHT API communication

## Voice Engines

PlayHT supports multiple voice engines:

- **PlayDialog**: Optimized for conversational AI
- **Play3.0**: Latest generation voice engine
- **PlayHT2.0**: Previous generation with wide voice selection

Refer to [PlayHT documentation](https://docs.play.ht/) for more details on available voice engines and voices.

## API Reference

Refer to `api` definition in [manifest.json](manifest.json) and default values in [property.json](property.json).
