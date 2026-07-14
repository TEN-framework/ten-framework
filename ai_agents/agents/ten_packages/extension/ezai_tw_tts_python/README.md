# ezai_tw_tts_python

TEN TTS extension for EZAI TW TTS — Traditional-Chinese text-to-speech via
the EZAI Matcha HTTP API.

## Features

- Traditional-Chinese text normalization (WeTextProcessing + OpenCC s2t)
- Automatic sentence segmentation for low-latency streaming
- WAV (PCM24) → PCM16 conversion
- Configurable voice, speed, denoising, and zh_model

## Quick start

1. Add the extension to your TEN app manifest and graph:

   - Manifest dependency:
     - `../../../ten_packages/extension/ezai_tw_tts_python`
   - Graph node:

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
      "url": "https://matcha.ezai-k8s.freeddns.org/tts",
      "api_key": "",
      "speed": 0.8,
      "denoise": false,
      "voice": "IU_IUF1003",
      "zh_model": "twllama"
    }
  }
}
```

2. Run your TEN app as usual.

## Configuration

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `params.url` | string | `https://matcha.ezai-k8s.freeddns.org/tts` | EZAI TTS endpoint |
| `params.api_key` | string | `""` | Bearer token for the API |
| `params.voice` | string | `IU_IUF1003` | Voice preset key |
| `params.speed` | float | `0.8` | Speech speed |
| `params.denoise` | bool | `false` | Apply denoising |
| `params.zh_model` | string | `twllama` | Chinese translation model |
| `params.sample_rate` | int | `24000` | PCM sample rate |
| `params.channels` | int | `1` | PCM channels |
| `params.sample_width` | int | `2` | Bytes per sample (PCM_16) |
| `dump` | bool | `false` | Write PCM to disk for debugging |
| `dump_path` | string | `""` | Directory for dump files |

## API

Refer to `api` definition in [manifest.json](manifest.json) and default values
in [property.json](property.json).
