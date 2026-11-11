# Breeze TTS extension for TEN Framework

Stream [Breeze](https://breezeblue.ai) speech synthesis into a TEN voice agent.
Text in, PCM audio out — with Chinese voices, style instructions, and voice
design.

## Features

- Streaming PCM (default 24 kHz, 16-bit mono) straight into TEN's audio pipeline
- Breeze voice library plus designed voices (`voc_...`)
- Emotion / pacing via `instructions`, speed 0.5–2.0
- Interruptible: cancels the in-flight HTTP stream on barge-in
- Optional PCM dump for debugging

## API mapping

| TEN | Breeze |
| --- | --- |
| `text` | `text` |
| `voice_id` | `/v1/text-to-speech/{voice_id}/stream` |
| `api_key` | `xi-api-key` header |
| `instructions` | `instructions` |
| `speed` | `voice_settings.speed` |
| `language` | `language_code` |
| audio | `output_format=pcm` |

## Configuration

`property.json`:

```json
{
  "params": {
    "api_key": "${env:BREEZE_API_KEY}",
    "base_url": "https://api.breeze.blue",
    "voice_id": "voc_u3nxkd5578wc",
    "language": "zh",
    "speed": 1.0,
    "instructions": "",
    "sample_rate": 24000,
    "output_format": "pcm",
    "timeout": 60
  }
}
```

Environment fallback: `BREEZE_API_KEY`, `BREEZE_BASE_URL`.

## Develop

```bash
pip install -r requirements.txt
pytest -q
```

Build inside the TEN build container (`ghcr.io/ten-framework/ten_agent_build`)
with `task build`, same as other TEN extensions.

## Status

v0.1.0 — first cut, validated against the `ten_ai_base` TTS2 interface.
Integration tests against a live TEN graph are pending.
