# Smallest AI TTS Extension (Lightning)

Text-to-speech extension for TEN Framework using
[Smallest AI Lightning](https://docs.smallest.ai/waves/documentation/text-to-speech-lightning/overview)
over the SSE streaming endpoint (~100 ms to first audio chunk, 12 languages,
voice cloning).

## Configuration

All settings live under `params` in `property.json`:

```json
{
  "params": {
    "api_key": "${env:SMALLEST_API_KEY|}",
    "model": "lightning_v3.1",
    "voice_id": "magnus",
    "sample_rate": 24000
  }
}
```

| Param | Default | Description |
| --- | --- | --- |
| `api_key` | — | Smallest AI API key (get one at [app.smallest.ai](https://app.smallest.ai)). Required. |
| `base_url` | `https://api.smallest.ai` | API base URL. The streaming path `/waves/v1/tts/live` is appended. |
| `model` | `lightning_v3.1` | `lightning_v3.1` (multilingual, cloning) or `lightning_v3.1_pro` (premium voices, English + Hindi). |
| `voice_id` | `magnus` | Catalog voice or cloned voice (`voice_*`). Pair Pro voices with the Pro model. |
| `sample_rate` | `24000` | Output PCM sample rate in Hz (8000-44100). |
| `speed` | `1.0` | Speech speed (0.5-2.0). |

Any additional keys in `params` (e.g. `language`, `pronunciation_dicts`) are
forwarded verbatim in the request body — see the
[API reference](https://docs.smallest.ai/waves/api-reference/api-reference/text-to-speech/synthesize-speech).

`output_format` is pinned to `pcm` internally: Lightning's raw PCM is signed
16-bit LE mono, which is exactly the TEN `pcm_frame` contract, and skipping
the container header keeps time-to-first-audio low.

## Environment Variables

- `SMALLEST_API_KEY` — Smallest AI API key.

## Testing

```bash
task tts-guarder-test EXTENSION=smallest_tts_python
```

Extension-local tests (mocked vendor, no API key needed):

```bash
cd tests && ./bin/start
```
