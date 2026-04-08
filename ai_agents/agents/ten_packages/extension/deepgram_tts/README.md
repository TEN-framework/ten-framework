# Deepgram TTS Extension

A TEN Framework extension that provides Text-to-Speech (TTS) capabilities using Deepgram's Aura streaming API.

## Features

- Real-time streaming TTS via WebSocket
- Multiple voice models (Aura-2 series)
- Configurable sample rates (8000, 16000, 24000, 48000 Hz)
- Linear16 PCM audio output
- TTFB (Time to First Byte) metrics reporting
- Audio dump capability for debugging

## Configuration

### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `params.api_key` | string | Required | Deepgram API key |
| `params.model` | string | `aura-2-thalia-en` | Voice model to use |
| `params.encoding` | string | `linear16` | Audio encoding format |
| `params.sample_rate` | int | `24000` | Output sample rate in Hz |
| `params.base_url` | string | `wss://api.deepgram.com/v1/speak` | WebSocket endpoint |
| `params.<deepgram_query_param>` | scalar | Optional | Additional Deepgram websocket query parameters passed through to the vendor |
| `dump` | bool | `false` | Enable audio dumping |
| `dump_path` | string | `/tmp` | Path for audio dump files |

### Example Configuration

```json
{
  "params": {
    "api_key": "${env:DEEPGRAM_API_KEY}",
    "model": "aura-2-thalia-en",
    "encoding": "linear16",
    "sample_rate": 24000,
    "container": "none"
  },
  "dump": false,
  "dump_path": "/tmp"
}
```

Known extension-owned keys such as `api_key`, `base_url`, `model`, `encoding`,
and `sample_rate` are normalized onto the config object. Any remaining scalar
keys under `params` are appended to the Deepgram websocket query string.

## Available Voice Models

Deepgram Aura-2 voices:
- `aura-2-thalia-en` - Female, English (default)
- `aura-2-luna-en` - Female, English
- `aura-2-stella-en` - Female, English
- `aura-2-athena-en` - Female, English
- `aura-2-hera-en` - Female, English
- `aura-2-orion-en` - Male, English
- `aura-2-arcas-en` - Male, English
- `aura-2-perseus-en` - Male, English
- `aura-2-angus-en` - Male, English
- `aura-2-orpheus-en` - Male, English
- `aura-2-helios-en` - Male, English
- `aura-2-zeus-en` - Male, English

## Supported Sample Rates

- 8000 Hz
- 16000 Hz
- 24000 Hz (recommended)
- 48000 Hz

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
cd deepgram_tts
tman -y install --standalone
./tests/bin/start
```

## Environment Variables

- `DEEPGRAM_API_KEY` - Your Deepgram API key

## License

Apache License, Version 2.0
