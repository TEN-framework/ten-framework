# xAI TTS Extension

A TEN Framework extension that provides Text-to-Speech (TTS) capabilities using
xAI's streaming voice API.

## Features

- Real-time streaming TTS via WebSocket
- Five xAI voices (`eve`, `ara`, `rex`, `sal`, `leo`)
- Configurable codecs and sample rates
- PCM streaming output for realtime voice graphs
- TTFB (Time to First Byte) metrics reporting
- Audio dump capability for debugging

## Configuration

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `params.api_key` | string | Required | xAI API key |
| `params.voice_id` | string | `eve` | Voice to use |
| `params.language` | string | `en` | BCP-47 language code |
| `params.codec` | string | `pcm` | Output codec |
| `params.sample_rate` | int | `24000` | Output sample rate in Hz |
| `params.base_url` | string | `wss://api.x.ai/v1/tts` | WebSocket endpoint |
| `params.<xai_query_param>` | scalar | Optional | Additional xAI websocket query parameters passed through to the vendor |
| `dump` | bool | `false` | Enable audio dumping |
| `dump_path` | string | `/tmp` | Path for audio dump files |

### Example Configuration

```json
{
  "params": {
    "api_key": "${env:XAI_API_KEY}",
    "voice_id": "eve",
    "language": "en",
    "codec": "pcm",
    "sample_rate": 24000,
    "optimize_streaming_latency": 0
  },
  "dump": false,
  "dump_path": "/tmp"
}
```

Known extension-owned keys such as `api_key`, `base_url`, `voice_id`, `language`,
`codec`, and `sample_rate` are normalized onto the config object. Any remaining
scalar keys under `params` are appended to the xAI websocket query string.

## Voices

- `eve` - energetic, upbeat
- `ara` - warm, friendly
- `rex` - confident, clear
- `sal` - smooth, balanced
- `leo` - authoritative, strong

## API Interface

This extension implements the standard TEN TTS interface:

### Input Data
- `tts_text_input` - Text to synthesize
- `tts_flush` - Flush pending audio

### Output Data
- `tts_audio_start` - Audio generation started
- `tts_audio_end` - Audio generation completed
- `metrics` - Performance metrics (TTFB, duration)
- `error` - Error information

### Output Audio
- `pcm_frame` - PCM audio data (16-bit, mono)

## Running Tests

```bash
cd xai_tts_python
tman -y install --standalone
./tests/bin/start
```

## Environment Variables

- `XAI_API_KEY` - Your xAI API key

## License

Apache License, Version 2.0
