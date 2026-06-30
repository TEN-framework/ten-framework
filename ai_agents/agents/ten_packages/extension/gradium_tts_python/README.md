# Gradium TTS Extension

A TEN Framework Text-to-Speech extension for Gradium's streaming websocket API.

## Features

- Streaming websocket TTS
- 16-bit mono PCM output
- Configurable sample rates through Gradium PCM output formats
- TTFB metrics
- Per-request PCM dump files

## Configuration

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `params.api_key` | string | Required | Gradium API key sent as `x-api-key` |
| `params.url` | string | `wss://api.gradium.ai/api/speech/tts` | Gradium websocket endpoint |
| `params.model_name` | string | `default` | Gradium model name |
| `params.voice_id` | string | `cLONiZ4hQ8VpQ4Sz` | Gradium voice ID |
| `params.voice` | string | empty | Optional alternate voice selector if `voice_id` is omitted |
| `params.sample_rate` | int | `24000` | PCM sample rate (one of 8000/16000/22050/24000/44100/48000); Gradium only supports PCM, so the wire `output_format` is derived as `pcm_<sample_rate>` |
| `params.json_config` | string (JSON) | empty | Optional Gradium voice-tuning payload as a JSON string, e.g. `'{"speed": 1.1}'` |
| `params.close_ws_on_eos` | bool | `true` | Ask Gradium to close the websocket at EOS |
| `params.retry_for_s` | float | empty | Optional Gradium retry window |
| `params.pronunciation_id` | string | empty | Optional Gradium pronunciation resource |
| `params.<extra_vendor_key>` | scalar/object | Optional | Passed through in the websocket `setup` payload |
| `dump` | bool | `false` | Enable PCM dump output |
| `dump_path` | string | `/tmp` | Dump directory |

Example:

```json
{
  "dump": false,
  "dump_path": "/tmp",
  "params": {
    "api_key": "${env:GRADIUM_API_KEY}",
    "url": "wss://api.gradium.ai/api/speech/tts",
    "model_name": "default",
    "voice_id": "cLONiZ4hQ8VpQ4Sz",
    "sample_rate": 24000
  }
}
```

## Running Tests

```bash
cd gradium_tts_python
tman -y install --standalone
./tests/bin/start
```

Guarder:

```bash
cd /app
task tts-guarder-test EXTENSION=gradium_tts_python
```

## Environment Variables

- `GRADIUM_API_KEY`
