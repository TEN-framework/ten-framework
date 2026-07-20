# Smallest AI ASR Extension (Pulse)

Real-time speech-to-text extension for TEN Framework using
[Smallest AI Pulse](https://docs.smallest.ai/waves/documentation/speech-to-text-pulse/overview)
over the live WebSocket API (64 ms time-to-first-transcript, 38 languages
with auto-detect).

## Configuration

All settings live under `params` in `property.json`:

```json
{
  "params": {
    "api_key": "${env:SMALLEST_API_KEY|}",
    "model": "pulse",
    "language": "en",
    "sample_rate": 16000
  }
}
```

| Param | Default | Description |
| --- | --- | --- |
| `api_key` | — | Smallest AI API key (get one at [app.smallest.ai](https://app.smallest.ai)). Required. |
| `url` | `wss://api.smallest.ai/waves/v1/stt/live` | WebSocket endpoint. |
| `model` | `pulse` | Streaming model. Only `pulse` supports the live endpoint. |
| `language` | `en` | ISO language code (e.g. `en`, `hi`, `es`). |
| `sample_rate` | `16000` | Input PCM sample rate in Hz. |

Any additional keys in `params` (e.g. `word_timestamps`, `eou_timeout`,
`punctuate`) are forwarded verbatim as query parameters to the live
endpoint — see the
[feature docs](https://docs.smallest.ai/waves/documentation/speech-to-text-pulse/overview)
for the full list.

## Environment Variables

- `SMALLEST_API_KEY` — Smallest AI API key.

## Behavior

- Audio is streamed as raw PCM16 binary frames.
- Interim transcripts are sent with `final=false`; Pulse finalizes segments
  automatically and on explicit `asr_finalize` signals (mapped to Pulse's
  `{"type": "finalize"}` control message).
- Transient WebSocket failures are retried with exponential backoff
  (5 attempts) before a fatal error is reported.

## Testing

```bash
task asr-guarder-test EXTENSION=smallest_asr_python
```

Extension-local tests (mocked vendor, no API key needed):

```bash
cd tests && ./bin/start
```
