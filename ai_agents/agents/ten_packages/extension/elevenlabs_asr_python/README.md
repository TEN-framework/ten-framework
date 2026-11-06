# ElevenLabs ASR Python Extension

ElevenLabs Speech Recognition Extension based on the ElevenLabs Scribe Realtime v2
speech-to-text WebSocket API.

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
| `model_id` | string | `scribe_v2_realtime` | ElevenLabs realtime model ID |
| `language_code` | string | auto-detected | Optional ISO-639-1/3 language code |
| `include_timestamps` | bool | `false` | Whether to include delayed word timestamps; disable for lowest turn latency |
| `include_language_detection` | bool | `false` | Include detected language in delayed timestamp events |
| `commit_strategy` | string | `vad` | Commit strategy |
| `vad_threshold` | float | `0.4` | VAD sensitivity threshold |
| `vad_silence_threshold_secs` | float | `0.35` | Silence required to commit a turn |
| `min_speech_duration_ms` | int | `100` | Minimum speech duration for VAD |
| `min_silence_duration_ms` | int | `100` | Minimum silence duration for VAD |
| `keyterms` | string[] | unset | Up to 50 terms, each at most 20 characters |
| `keepalive_interval_ms` | int | unset | Optional server keepalive interval from 500 to 10000 ms |
| `enable_logging` | bool | `true` | Whether to enable logging |

### Top-level Properties

| Property | Type | Default Value | Description |
|----------|------|---------------|-------------|
| `dump` | bool | `false` | Save sent PCM audio for debugging |
| `dump_path` | string | `/tmp` | Directory for the audio dump |

The extension accepts the Scribe Realtime v2 query options for VAD, language
selection, keyterms, no-verbatim mode, background filtering, and keepalives.
TEN audio input is signed PCM16 mono, so `ulaw_8000` is rejected unless an
upstream extension converts the audio first.

## Configuration Examples

### Complete Configuration

```json
{
    "dump": false,
    "dump_path": "/tmp",
    "params": {
        "api_key": "your_elevenlabs_api_key_here",
        "ws_url": "wss://api.elevenlabs.io/v1/speech-to-text/realtime",
        "sample_rate": 16000,
        "audio_format": "pcm_16000",
        "model_id": "scribe_v2_realtime",
        "language_code": "en",
        "include_timestamps": false,
        "commit_strategy": "vad",
        "vad_threshold": 0.4,
        "vad_silence_threshold_secs": 0.35,
        "min_speech_duration_ms": 100,
        "min_silence_duration_ms": 100,
        "enable_logging": true
    }
}
```
