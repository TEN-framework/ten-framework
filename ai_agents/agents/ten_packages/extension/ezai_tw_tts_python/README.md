# vibevoice_tts_websocket_python

TEN TTS extension for ezai_tw_tts_python.

## Quick start


1. Add the extension to your TEN app manifest and graph:

- Manifest dependency:
  - `../../../ten_packages/extension/ezai_tw_tts_python`
-- Graph node:

```json
{
  "type": "extension",
  "name": "tts",
  "addon": "ezai_tw_tts_python",
  "extension_group": "tts",
  "property": {
    "dump": false,
    "dump_path": "./",
    "params": {
      "speed": 0.8,
      "denoise": false,
      "voice": "",
      "zh_model": "",
    }
  }
}
```

1. Run your TEN app as usual..

## Configuration

- `params.url`: websocket endpoint (default `ws://127.0.0.1:3000/stream`)
- `params.speed`: text-to-speech speed/power (default 0.8)
- `params.denoise`: whether to apply denoising (default false)
- `params.voice`: voice preset key (optional)
- `params.zh_model`: chinese translation model to use (optional)
- `sample_rate`, `channels`, `sample_width`: PCM properties consumed/produced by TTS (defaults: 24000, 1, 2)
- `dump`: write PCM to disk for debugging
- `dump_path`: directory for dump files
