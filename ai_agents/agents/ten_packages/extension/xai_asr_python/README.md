# xAI ASR Extension

A TEN Framework extension that provides streaming Speech-to-Text (STT / ASR)
capabilities using xAI's WebSocket speech API.

## Features

- Real-time ASR over WebSocket
- Raw PCM / mu-law / A-law input
- Partial and final transcript handling
- Explicit finalize via `audio.done`
- Reconnect support with bounded retry attempts
- Audio dump support for debugging

## Configuration

All configuration is supplied through the `params` object.

```json
{
  "dump": false,
  "dump_path": "/tmp",
  "finalize_timeout_ms": 2000,
  "params": {
    "api_key": "${env:XAI_API_KEY}",
    "base_url": "wss://api.x.ai/v1/stt",
    "sample_rate": 16000,
    "encoding": "pcm",
    "interim_results": true,
    "endpointing": 300,
    "language": "en",
    "diarize": false,
    "multichannel": false,
    "channels": 1
  }
}
```

## Key Properties

- `params.api_key`: xAI API key
- `params.base_url`: WebSocket endpoint
- `params.sample_rate`: input sample rate
- `params.encoding`: `pcm`, `mulaw`, or `alaw`
- `params.language`: formatting language code
- `params.interim_results`: emit partial transcripts
- `params.endpointing`: silence duration before utterance finalization
- `dump`: enable PCM dump output
- `dump_path`: dump directory
- `finalize_timeout_ms`: wait time for `transcript.done`

## Running Tests

```bash
cd xai_asr_python
tman -y install --standalone
./tests/bin/start
```

## Environment Variables

- `XAI_API_KEY` - Your xAI API key
