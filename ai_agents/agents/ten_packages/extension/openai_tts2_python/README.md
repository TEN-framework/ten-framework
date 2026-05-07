# openai_tts2_python

OpenAI TTS Extension for TEN Framework - provides text-to-speech synthesis using OpenAI's HTTP API.

## Features

- Streaming TTS synthesis using OpenAI's audio API
- Support for multiple models and voices
- Configurable speech speed
- Optional audio dumping for debugging
- Fixed 24kHz PCM audio output
- Automatic API key authentication

## Configuration

### Top-level Properties

- `dump` (bool): Enable audio dumping for debugging (default: false)
- `dump_path` (string): Path to save dumped audio files (default: extension directory + "openai_tts_in.pcm")

### TTS Parameters (under `params`)

All OpenAI TTS-specific settings are configured within the `params` object:

- `api_key` (string, required): OpenAI API key
- `model` (string, required): TTS model to use (e.g., "gpt-4o-mini-tts")
- `voice` (string, required): Voice identifier (e.g., "coral", "alloy", "echo", "fable", "onyx", "nova", "shimmer")
- `speed` (float, optional): Speech speed (0.25 to 4.0, default: 1.0)
- `instructions` (string, optional): Additional instructions for the TTS model
- `enable_context` (bool, optional): Enable context-aware passthrough fields for custom TTS servers (default: `false`)
- `context_max_history` (int, optional): Number of previous completed turns to include in `context.context_text` on the first request of each turn (default: `0`)

When `enable_context` is `true`, every outbound request includes:

- `context_id` (string): Always present, uses the current `request_id`
- `is_start` (bool): Present and `true` only on the first request of the same `request_id`
- `is_end` (bool): Present and `true` only on the last request where `text_input_end=true`

On the first request of each turn, the extension also sends a `context` object:

- `context.context_id`: Same as the current `request_id`
- `context.context_type`: Fixed to `conversation_history`
- `context.context_text`: A JSON string built from the `context` extension's `fetch` history, bounded to the latest `context_max_history` turn IDs. Each entry includes `role`, OpenAI-style `content`, `interrupted`, `start_time`, `end_time`, and `turn_id`.

### Example Configuration

```json
{
  "dump": false,
  "dump_path": "/tmp/openai_tts_dump",
  "params": {
    "api_key": "sk-...",
    "model": "gpt-4o-mini-tts",
    "voice": "coral",
    "speed": 1.0,
    "instructions": "",
    "enable_context": false,
    "context_max_history": 0
  }
}
```

## Architecture

This extension follows the `AsyncTTS2HttpExtension` pattern:

- **Extension**: `OpenAITTSExtension` - Inherits from `AsyncTTS2HttpExtension`
- **Config**: `OpenAITTSConfig` - Extends `AsyncTTS2HttpConfig`, stores configuration with `params` as a dict
- **Client**: `OpenAITTSClient` - Extends `AsyncTTS2HttpClient`, handles OpenAI API communication

## API Reference

Refer to `api` definition in [manifest.json](manifest.json) and default values in [property.json](property.json).
