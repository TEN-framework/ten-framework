# vibevoice_tts_websocket_python

TEN TTS extension for the VibeVoice-Realtime websocket demo server.

## Quick start

1. Start the VibeVoice realtime server (in the VibeVoice repo):

```bash
python demo/vibevoice_realtime_demo.py --model_path microsoft/VibeVoice-Realtime-0.5B --port 3000
```

2. Add the extension to your TEN app manifest and graph:

- Manifest dependency:
  - `../../../ten_packages/extension/vibevoice_tts_websocket_python`
- Graph node:

```json
{
  "type": "extension",
  "name": "tts",
  "addon": "vibevoice_tts_websocket_python",
  "extension_group": "tts",
  "property": {
    "dump": false,
    "dump_path": "./",
    "params": {
      "url": "ws://127.0.0.1:3000/stream",
      "cfg_scale": 1.5,
      "steps": 5,
      "voice": "",
      "sample_rate": 24000,
      "channels": 1,
      "sample_width": 2
    }
  }
}
```

3. Run your TEN app as usual. Audio will stream from the VibeVoice server.

## Configuration

- `params.url`: websocket endpoint (default `ws://127.0.0.1:3000/stream`)
- `params.cfg_scale`: classifier-free guidance scale
- `params.steps`: inference steps (optional)
- `params.voice`: voice preset key (optional)
- `params.sample_rate`: expected PCM sample rate (default 24000)
- `params.channels`: expected PCM channel count (default 1)
- `params.sample_width`: bytes per sample (default 2 for PCM16)
- `dump`: write PCM to disk for debugging
- `dump_path`: directory for dump files

Notes:
- The server expects full text in the `text` query parameter; the extension buffers input until `text_input_end` is true.
- VibeVoice outputs 24kHz PCM16; keep `sample_rate` at 24000 unless you add resampling elsewhere.
