# murf_tts_python

MURF TTS extension for TEN Framework using a streaming WebSocket backend.

## Features

- Streaming text-to-speech with low-latency audio chunks
- TTFB metrics and audio lifecycle events
- Optional PCM dump for debugging
- Non-fatal handling for known invalid text inputs

## API

Refer to `api` definition in `manifest.json` and defaults in `property.json`.

### Params

Set these under `params` in `property.json`:

- `api_key` (string, required): API key for MURF TTS
- `voiceId` (string): voice identifier (e.g. `Matthew`)
- `multiNativeLocale` (string): locale (e.g. `en-US`)
- `style` (string): voice style (e.g. `Conversation`)
- `rate` (int): speaking rate adjustment
- `pitch` (int): pitch adjustment
- `variation` (int): variation level
- `model` (string): model name (e.g. `FALCON`)
- `sample_rate` (int): audio sample rate

## Authentication

The API key is sent via WebSocket headers (not query params) to avoid
exposure through URL logging.

## Audio header handling

Some responses include a 44-byte header in the first audio chunk.
The extension strips this header when present and logs a warning if the
first chunk is too small to validate.

## Development

### Build

Install dependencies as needed (see `requirements.txt`).

### Unit test

Use the extension tests under `tests/`:

```bash
task test-extension EXTENSION=agents/ten_packages/extension/murf_tts_python
```
