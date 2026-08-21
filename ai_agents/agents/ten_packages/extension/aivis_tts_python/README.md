# Aivis TTS Python Extension

Japanese-first realtime text-to-speech for [TEN Framework](https://github.com/TEN-framework/ten-framework) using [Aivis Cloud API](https://api.aivis-project.com/v1/docs).

Fills the Japan regional TTS gap the same way Gradium fills French/EU — a specialist voice API wired as a standard `AsyncTTS2HttpExtension`.

## Features

- HTTP streaming synthesize (`POST /v1/tts/synthesize`)
- WAV → raw PCM via `WavStreamParser` (TEN RTC-ready)
- Configurable sample rate (default 16 kHz mono)
- Optional speaker / style / speaking rate
- API key redaction in logs

## Configuration

```json
{
  "params": {
    "api_key": "${env:AIVIS_API_KEY}",
    "model_uuid": "a59cb814-0083-4369-8542-f51a29e72af7",
    "base_url": "https://api.aivis-project.com",
    "output_sampling_rate": 16000,
    "output_audio_channels": "mono",
    "language": "ja",
    "use_ssml": false
  }
}
```

### Parameters

| Key | Required | Default | Notes |
| --- | :---: | --- | --- |
| `api_key` | ✅ | — | Bearer token from [Aivis Cloud dashboard](https://hub.aivis-project.com/cloud-api/dashboard) |
| `model_uuid` | ✅ | public demo model | AIVM model UUID or limited-access key (`ak_…`) |
| `base_url` | | `https://api.aivis-project.com` | Override for staging |
| `output_sampling_rate` | | `16000` | 8000–48000; TEN graphs usually want 16k |
| `output_audio_channels` | | `mono` | `mono` / `stereo` |
| `language` | | `ja` | Currently Japanese-focused |
| `speaker_uuid` | | — | Optional speaker within the model |
| `style_id` / `style_name` | | — | Mutually exclusive |
| `speaking_rate` | | `1.0` | 0.5–2.0 |
| `use_ssml` | | `false` | Enable SSML / `<break>` tags |

`output_format` is forced to `wav` so the extension can strip the header and emit PCM.

## Architecture

```
LLM text → AivisTTSExtension → POST /v1/tts/synthesize (WAV stream)
                              → WavStreamParser → PCM frames → RTC
```

Pattern mirrors `rime_http_tts` (httpx stream) + `groq_tts_python` (WAV parse).

## Local smoke test

From this workspace (no full TEN runtime required):

```bash
export AIVIS_API_KEY=...
python scripts/smoke_aivis_tts.py
# writes /tmp/aivis_smoke.pcm (s16le mono 16k)
```

## Signup

1. Open https://hub.aivis-project.com/cloud-api/dashboard
2. Create an API key
3. Pick a public model UUID from AivisHub (or use the default in `property.json`)
4. Export `AIVIS_API_KEY`

Pay-as-you-go and a premium unlimited plan are available; free trial credits are offered on signup.

## Why not Supertone?

Supertone API new signups ended 2026-07-23 and the service shuts down 2026-08-31 — unsuitable for a new TEN contribution.
