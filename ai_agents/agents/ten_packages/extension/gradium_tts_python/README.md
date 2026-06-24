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
| `params.base_url` | string | `wss://api.gradium.ai/api/speech/tts` | Gradium websocket endpoint |
| `params.model_name` | string | `default` | Gradium model name |
| `params.voice_id` | string | `cLONiZ4hQ8VpQ4Sz` | Gradium voice ID |
| `params.voice` | string | empty | Optional alternate voice selector if `voice_id` is omitted |
| `params.sample_rate` | int | `24000` | Desired PCM sample rate; normalized to `output_format` |
| `params.output_format` | string | derived | Optional explicit Gradium output format such as `pcm_16000` |
| `params.json_config` | object/string | empty | Optional Gradium voice tuning payload |
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
    "base_url": "wss://api.gradium.ai/api/speech/tts",
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
