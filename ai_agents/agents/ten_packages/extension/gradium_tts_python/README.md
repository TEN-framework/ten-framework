# Gradium TTS Extension

Streaming Text-to-Speech integration for [Gradium](https://gradium.ai/) using the websocket API (`wss://<region>.api.gradium.ai/api/speech/tts`). The extension emits 16-bit PCM audio by default (48 kHz) and supports other Gradium PCM formats.

## Configuration

Update `property.json` or override via environment:

- `GRADIUM_API_KEY` – required API key (`x-api-key` header).
- `region` – `us` (default) or `eu` to select the websocket endpoint.
- `model_name` – Gradium TTS model, defaults to `default`.
- `voice_id` – required voice identifier from the Gradium voice library.
- `output_format` – `pcm` (48 kHz), `pcm_16000`, or `pcm_24000`. PCM formats are required by the extension.

Example:

```json
{
  "params": {
    "api_key": "${env:GRADIUM_API_KEY}",
    "region": "us",
    "model_name": "default",
    "voice_id": "YTpq7expH9539ERJ",
    "output_format": "pcm"
  }
}
```
