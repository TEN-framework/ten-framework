# ElevenLabs ASR Python Extension

ElevenLabs Speech Recognition Extension based on ElevenLabs real-time speech-to-text API.

## Configuration Parameters

### Required Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `api_key` | string | ElevenLabs API key, **must be provided** |

### Optional Parameters

| Parameter | Type | Default Value | Description |
|-----------|------|---------------|-------------|
| `ws_url` | string | `wss://api.elevenlabs.io/v1/speech-to-text/realtime` | WebSocket endpoint URL |
| `sample_rate` | int | `16000` | Audio sample rate |
| `audio_format` | string | `pcm_16000` | Audio format |
| `model_id` | string | `scribe_v2_realtime` | ElevenLabs model ID |
| `language_code` | string | `en` | Language code |
| `include_timestamps` | bool | `true` | Whether to include timestamps |
| `commit_strategy` | string | `manual` | Commit strategy |
| `enable_logging` | bool | `true` | Whether to enable logging |

## Configuration Examples

### Complete Configuration

```json
{
    "params": {
        "api_key": "your_elevenlabs_api_key_here",
        "ws_url": "wss://api.elevenlabs.io/v1/speech-to-text/realtime",
        "sample_rate": 16000,
        "audio_format": "pcm_16000",
        "model_id": "scribe_v2_realtime",
        "language_code": "en",
        "include_timestamps": true,
        "commit_strategy": "manual",
        "enable_logging": true
    }
}
```