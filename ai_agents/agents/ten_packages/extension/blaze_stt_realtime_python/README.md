# Blaze Realtime STT Extension for TEN Framework

Blaze realtime (streaming) Speech-to-Text (STT) extension for [TEN Framework](https://github.com/TEN-framework/ten-framework).

Unlike the batch [`blaze_stt_python`](../blaze_stt_python) extension (which uploads a
full audio file to `POST /v1/stt/execute`), this extension streams audio over a
WebSocket to `/v1/stt/realtime` and receives interim (`partial`) and stable
(`final`) transcripts as the audio is spoken.

## Installation

```bash
pip install -r requirements.txt
```

Or install dependencies directly:

```bash
pip install websockets pydantic
```

## Configuration

### Environment Variables

```bash
export BLAZE_STT_API_URL="http://localhost:8000"   # http(s) base; converted to ws(s)
export BLAZE_STT_API_KEY="your-api-token-here"     # sent in the init message
```

### Property.json (TEN Framework)

```json
{
    "params": {
        "api_url": "${env:BLAZE_STT_API_URL}",
        "api_key": "${env:BLAZE_STT_API_KEY}",
        "language": "vi",
        "model": "stt-stream-1.5",
        "timeout": 3600,
        "enable_log": false
    }
}
```

## WebSocket Protocol (`/v1/stt/realtime`)

1. Connect to `ws(s)://<host>/v1/stt/realtime`
2. Send a JSON init message: `{ "token", "language", "model", "enable_log" }`
3. Receive `{ "type": "ready" }`
4. Stream binary PCM audio chunks (**16 kHz, mono, 16-bit little-endian**)
5. Receive transcript messages:
   - `{ "type": "partial", "text": "..." }` — interim result
   - `{ "type": "final",   "text": "..." }` — stable result
   - `{ "type": "error",   "text": "..." }` — error
6. Close the connection to signal end-of-audio.

## Architecture

The package is split into two layers:

- **`BlazeSTTRealtimeExtension`** (`extension.py`) — the TEN ASR extension.
  It subclasses `ten_ai_base.asr.AsyncASRBaseExtension`, is registered as an
  addon (`addon.py`), and is driven by the TEN runtime: it consumes incoming
  `AudioFrame`s, emits transcripts as `asr_result` data, and reports failures
  (invalid/missing API key, connection or server errors) as `error` data via
  `send_asr_error`. This is what runs inside a TEN graph; you do not instantiate
  it directly.
- **`BlazeRealtimeClient`** (`blaze_stt_realtime.py`) — a standalone async
  websocket client with no TEN dependency. The extension uses its
  `build_ws_url` helper; you can also use it directly for scripts/tests.

## Usage

### Inside a TEN graph

Add the extension to your graph and configure it via `property.json` (see
[Configuration](#propertyjson-ten-framework)). The runtime loads the addon by
its name `blaze_stt_realtime_python`. Transcripts arrive as `asr_result` data
and errors as `error` data on the extension's output.

### Standalone client (scripts / tests)

```python
import asyncio
from blaze_stt_realtime_python.blaze_stt_realtime import BlazeRealtimeClient

client = BlazeRealtimeClient(config={
    "api_url": "http://localhost:8000",
    "api_key": "your-api-token",
    "language": "vi",
    "model": "stt-stream-1.5",
})

async def mic_chunks():
    # Yield binary PCM chunks (16 kHz, mono, 16-bit) as they arrive.
    ...

async def main():
    async for event in client.transcribe_stream(mic_chunks()):
        if event["type"] in ("partial", "final"):
            print(event["type"], event["text"])

asyncio.run(main())
```

To transcribe a complete PCM buffer in one call:

```python
result = await client.transcribe(audio_data=pcm_bytes, language="vi")
print(result["transcription"])
```

## API Reference

### BlazeSTTRealtimeExtension (TEN ASR extension)

Implements `AsyncASRBaseExtension`. Key overrides:
- `start_connection()` — validate the API key, open the websocket, complete the
  `ready` handshake, and start the receive loop. Surfaces a FATAL `error` on
  any failure.
- `send_audio(frame, session_id)` — forward a PCM `AudioFrame` over the socket
- `finalize(session_id)` — report end-of-turn (`asr_finalize_end`)
- `is_connected()` / `stop_connection()` — connection lifecycle

### BlazeRealtimeClient (standalone client)

- `transcribe_stream(audio_chunks, language, model, enable_log, drain_timeout)` —
  async generator yielding transcript events from a (sync or async) iterable of PCM chunks
- `transcribe(audio_data, language, model, enable_log, chunk_size, chunk_interval, drain_timeout)` —
  convenience wrapper that streams a complete PCM buffer and accumulates the transcript

## Audio Format

- Encoding: `pcm_s16le` (16-bit little-endian)
- Sample rate: `16000` Hz
- Channels: `1` (mono)

## License

This extension is provided as-is for use with the TEN Framework and Blaze services.
