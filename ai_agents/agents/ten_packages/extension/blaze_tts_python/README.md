# Blaze TTS Extension

TEN TTS extension for [Blaze](https://blaze.vn) realtime Text-to-Speech
(model `2.0-realtime`).

## Features

- Streams raw PCM16 mono over one reusable WebSocket
- Connects and authenticates at startup, so the first reply has no
  connection delay
- Each `tts_text_input` fragment is synthesized as soon as it arrives
- TTFB metrics, interrupt (flush) support, and per-request audio dumps

## Protocol

Endpoint: `wss://api.blaze.vn/v1/tts/realtime` (derived from `base_url`).

1. Receive `successful-connection`.
2. Send `{"token", "strategy": "request"}`; receive
   `successful-authentication`. A bad token gets `failed-authentication` and
   close code 1008, reported as a fatal error.
3. Per fragment, send a query (`query`, `language`, `speaker_id`, `model`,
   `audio_format: pcm`, `sample_rate`, ...) and receive `processing-request`,
   `started-byte-stream`, binary PCM chunks, then `finished-byte-stream`.
   `failed-request`, `bad-request` and `internal-error` end the fragment with
   a non-fatal error and the socket is reopened.

Blaze has no cancel message, so an interrupt closes the socket to drop unread
audio; the next request reconnects.

## Configuration

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `params.base_url` | string | `https://api.blaze.vn` | Blaze base URL (`http(s)` is converted to `ws(s)`) |
| `params.api_key` | string | Required | Blaze API token |
| `params.speaker_id` | string | `HN-Nu-CSKH-HuongGiang` | Blaze voice ID; list voices with `GET /v1/voices` |
| `params.language` | string | `vi` | Language code |
| `params.model` | string | `2.0-realtime` | Realtime TTS model |
| `params.audio_speed` | float | `1.0` | Speed multiplier |
| `params.audio_quality` | int | `64` | Audio quality |
| `params.normalization` | string | `basic` | Text normalization (`no`, `basic`, `advanced`) |
| `params.sample_rate` | int | `24000` | Output sample rate (8000, 16000, 22050, 24000, 44100, 48000) |
| `params.<blaze_param>` | scalar | Optional | Other keys are added to each query |
| `dump` | bool | `false` | Dump synthesized audio per request |
| `dump_path` | string | `/tmp` | Directory for dump files |

## Environment Variables

- `BLAZE_TTS_API_URL` - Blaze base URL
- `BLAZE_TTS_API_KEY` - Blaze API token (the same token works for STT)
- `BLAZE_TTS_SPEAKER_ID` - Optional voice ID override

## Running Tests

Inside the `ten_agent_dev` container:

```bash
cd /app
task test-extension EXTENSION=agents/ten_packages/extension/blaze_tts_python
task tts-guarder-test EXTENSION=blaze_tts_python CONFIG_DIR=tests/configs
```
