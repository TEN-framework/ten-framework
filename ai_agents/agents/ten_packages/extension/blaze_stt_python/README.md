# Blaze STT Extension

TEN ASR extension for [Blaze](https://blaze.vn) realtime Speech-to-Text
(model `stt-stream-1.5`).

## Features

- Streams `pcm_frame` audio to Blaze over one WebSocket
- Interim (`final: false`) and final (`final: true`) `asr_result` output
- Server-side end-of-speech detection, with `start_ms` / `duration_ms` mapped
  to the session's audio timeline
- Bounded reconnect with backoff; audio is buffered while reconnecting
- Connect-delay metrics and optional input audio dump

## Protocol

Endpoint: `wss://api.blaze.vn/v1/stt/realtime` (derived from `base_url`).

1. Send `{"token", "language", "model", ["topic"], ["context"]}`.
2. Receive `successful-authentication`, then `ready`. A bad token gets
   `failed-authentication` and close code 1008, reported as a fatal error.
3. Stream binary PCM16 mono at 16 kHz.
4. Receive `{"type": "partial", "text"}` and
   `{"type": "final", "text", "start_ms", "end_ms"}`. Errors arrive as
   `{"type": "error", "code", "text"}` and are reported as non-fatal.

Blaze has no flush message. On `asr_finalize` the extension sends
`finalize_silence_ms` of silence so Blaze closes the current utterance, waits
up to `finalize_timeout_ms` for its final result, then sends exactly one
`asr_finalize_end`. When no speech is pending it ends immediately.

## Configuration

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `params.base_url` | string | `https://api.blaze.vn` | Blaze base URL (`http(s)` is converted to `ws(s)`) |
| `params.api_key` | string | Required | Blaze API token |
| `params.language` | string | `vi` | Recognition language, e.g. `vi`, `en` |
| `params.model` | string | `stt-stream-1.5` | Realtime STT model |
| `params.sample_rate` | int | `16000` | Input sample rate; Blaze realtime accepts 16000 only |
| `params.topic` | string | empty | Optional domain hint |
| `params.context` | string | empty | Optional free-form context |
| `params.result_language_default` | string | empty | Overrides `asr_result.language` |
| `finalize_silence_ms` | int | `800` | Silence sent on `asr_finalize` |
| `finalize_timeout_ms` | int | `2000` | Wait for the final result after `asr_finalize` |
| `dump` | bool | `false` | Dump incoming PCM |
| `dump_path` | string | `/tmp` | Directory for the dump file |

`asr_result.language` is normalized: `vi` → `vi-VN`, `en` → `en-US`.

## Environment Variables

- `BLAZE_STT_API_URL` - Blaze base URL
- `BLAZE_STT_API_KEY` - Blaze API token (the same token works for TTS)

## Running Tests

Inside the `ten_agent_dev` container:

```bash
cd /app
task test-extension EXTENSION=agents/ten_packages/extension/blaze_stt_python
task asr-guarder-test EXTENSION=blaze_stt_python CONFIG_DIR=tests/configs
```

Guarder configs: `property_en.json`, `property_vi.json`,
`property_invalid.json`, `property_dump.json`. There is no `property_zh.json`,
so the guarder's Chinese multi-language case is expected to fail.
