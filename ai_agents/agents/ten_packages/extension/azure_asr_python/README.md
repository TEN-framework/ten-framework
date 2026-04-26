# Azure Speech-to-Text ASR Extension

A TEN Framework extension for real-time speech recognition using [Azure Cognitive Services Speech SDK](https://learn.microsoft.com/en-us/azure/ai-services/speech-service/speech-to-text).

## Configuration

All parameters are configured through the `params` object in `property.json`.

### Basic configuration

```json
{
    "params": {
        "key": "${env:AZURE_STT_KEY|}",
        "region": "${env:AZURE_STT_REGION|}"
    }
}
```

### Language and recognition settings

```json
{
    "params": {
        "key": "${env:AZURE_STT_KEY|}",
        "region": "${env:AZURE_STT_REGION|}",
        "language": "en-US",
        "sample_rate": 16000
    }
}
```

### Multilingual recognition

Pass multiple language codes as a comma-separated string:

```json
{
    "params": {
        "key": "${env:AZURE_STT_KEY|}",
        "region": "${env:AZURE_STT_REGION|}",
        "language": "en-US,zh-CN,ja-JP"
    }
}
```

### Hotword boosting

Provide hotwords to improve recognition of domain-specific terms. Use the `|boost` suffix to set the boost value (default boost is applied without a suffix):

```json
{
    "params": {
        "key": "${env:AZURE_STT_KEY|}",
        "region": "${env:AZURE_STT_REGION|}",
        "hotwords": ["TEN Framework|10", "tman|8", "extension"]
    }
}
```

### Stream finalization mode

Controls how the extension finalizes an utterance:

```json
{
    "params": {
        "key": "${env:AZURE_STT_KEY|}",
        "region": "${env:AZURE_STT_REGION|}",
        "finalize_mode": "mute_pkg",
        "mute_pkg_duration_ms": 800
    }
}
```

| `finalize_mode` | Behavior |
|-----------------|----------|
| `"mute_pkg"` (default) | Finalize after `mute_pkg_duration_ms` of silence in the audio stream. |
| `"disconnect"` | Finalize on stream disconnect. |

### Audio dump (debugging)

```json
{
    "params": {
        "key": "${env:AZURE_STT_KEY|}",
        "region": "${env:AZURE_STT_REGION|}",
        "dump": true,
        "dump_path": "./dump/"
    }
}
```

## Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `key` | string | `""` | Azure Speech service subscription key. |
| `region` | string | `""` | Azure region, e.g. `"eastus"`, `"westeurope"`. |
| `language` | string | `"en-US"` | Primary language code, or comma-separated list for multilingual. |
| `sample_rate` | int | `16000` | Audio sample rate in Hz. |
| `finalize_mode` | string | `"mute_pkg"` | Utterance finalization strategy: `"mute_pkg"` or `"disconnect"`. |
| `mute_pkg_duration_ms` | int | `800` | Silence duration (ms) to trigger finalization in `mute_pkg` mode. |
| `hotwords` | list | `[]` | List of hotwords for phrase-list boosting. Use `"word\|boost"` format to set boost value. |
| `dump` | bool | `false` | Dump incoming audio to a PCM file for debugging. |
| `dump_path` | string | `"."` | Directory path for audio dump files. |

## Environment Variables

| Variable | Description |
|----------|-------------|
| `AZURE_STT_KEY` | Azure Speech service subscription key |
| `AZURE_STT_REGION` | Azure region identifier (e.g. `eastus`) |
