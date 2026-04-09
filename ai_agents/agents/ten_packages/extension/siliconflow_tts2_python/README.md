# siliconflow_tts2_python

SiliconFlow TTS extension built on TEN's `AsyncTTS2HttpExtension`.

## Notes

- Uses `POST /v1/audio/speech`
- Defaults to `response_format: "mp3"` because SiliconFlow currently returns `audio/mpeg`
- The extension decodes returned MP3 into mono 16-bit PCM before handing audio to TEN

## Required Params

```json
{
  "params": {
    "api_key": "${env:SILICONFLOW_API_KEY}",
    "base_url": "https://api.siliconflow.cn/v1",
    "model": "IndexTeam/IndexTTS-2",
    "voice": "IndexTeam/IndexTTS-2:anna"
  }
}
```

## Optional Params

- `sample_rate`
- `speed`
- `gain`
- `max_tokens`
- `response_format` (`mp3`, `wav` or `pcm`)
