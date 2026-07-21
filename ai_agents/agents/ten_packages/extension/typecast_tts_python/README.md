# Typecast TTS Extension

Typecast text-to-speech extension for TEN Framework.

The extension uses the official `typecast-python` SDK and Typecast's
`/v1/text-to-speech/stream` endpoint. Typecast streams WAV audio at 32 kHz,
16-bit, mono. The extension strips the initial WAV header and forwards PCM16
mono audio to TEN.

## Configuration

```json
{
  "params": {
    "api_key": "<typecast-api-key>",
    "url": "https://api.typecast.ai",
    "voice_id": "tc_60e5426de8b95f1d3000d7b5",
    "model": "ssfm-v30",
    "output": {
      "audio_format": "wav"
    }
  },
  "chunk_size": 8192
}
```

`output.audio_format` is forced to `wav` because TEN consumes PCM audio frames.
MP3 streaming would require an additional decoder and is intentionally not used.

## Validation

```bash
tman -y install --standalone
./tests/bin/start
```
