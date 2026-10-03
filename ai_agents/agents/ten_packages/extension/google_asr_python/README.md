# Google Cloud Speech-to-Text ASR Extension

A TEN Framework extension for real-time speech recognition using [Google Cloud Speech-to-Text V2 API](https://cloud.google.com/speech-to-text/v2/docs).

## Configuration

All parameters are configured through the `params` object in `property.json`.

### Basic configuration

```json
{
    "params": {
        "project_id": "${env:GOOGLE_ASR_PROJECT_ID|}",
        "language": "en-US",
        "model": "long",
        "sample_rate": 16000,
        "channels": 1,
        "encoding": "LINEAR16",
        "interim_results": true
    }
}
```

### Authentication

Google Cloud authentication is handled via [Application Default Credentials (ADC)](https://cloud.google.com/docs/authentication/application-default-credentials). Two options are supported:

**Option 1 — Credentials file path:**

```json
{
    "params": {
        "project_id": "${env:GOOGLE_ASR_PROJECT_ID|}",
        "adc_credentials_path": "${env:GOOGLE_APPLICATION_CREDENTIALS_PATH|}"
    }
}
```

**Option 2 — Credentials JSON string:**

```json
{
    "params": {
        "project_id": "${env:GOOGLE_ASR_PROJECT_ID|}",
        "adc_credentials_string": "${env:GOOGLE_APPLICATION_CREDENTIALS_STRING|}"
    }
}
```

### Multilingual recognition

Pass multiple language codes as a comma-separated string:

```json
{
    "params": {
        "language": "en-US,zh-CN,ja-JP",
        "model": "long"
    }
}
```

### Speaker diarization

```json
{
    "params": {
        "language": "en-US",
        "model": "long",
        "enable_speaker_diarization": true,
        "diarization_speaker_count": 2
    }
}
```

### Audio dump (debugging)

```json
{
    "params": {
        "language": "en-US",
        "dump": true,
        "dump_path": "./dump/"
    }
}
```

## Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `project_id` | string | `""` | Google Cloud Project ID. Retrieved from ADC if omitted. |
| `location` | string | `"global"` | Google Cloud location for the recognizer. |
| `adc_credentials_path` | string | `""` | Path to ADC credentials JSON file. |
| `adc_credentials_string` | string | `""` | ADC credentials as a JSON string. |
| `language` | string | `"en-US"` | Primary language code, or comma-separated list for multilingual. |
| `model` | string | `"long"` | Recognition model: `"long"`, `"short"`, `"chirp_2"`. |
| `sample_rate` | int | `16000` | Audio sample rate in Hz. |
| `channels` | int | `1` | Number of audio channels. |
| `encoding` | string | `"LINEAR16"` | Audio encoding. Supports `LINEAR16`, `MULAW`, `ALAW`, `FLAC`, `auto`. |
| `interim_results` | bool | `true` | Emit interim (partial) recognition results. |
| `enable_automatic_punctuation` | bool | `true` | Add punctuation to recognition results. |
| `enable_word_time_offsets` | bool | `true` | Include word-level timestamps. |
| `enable_speaker_diarization` | bool | `false` | Enable speaker diarization. |
| `diarization_speaker_count` | int | `0` | Number of speakers (0 = auto detect). |
| `profanity_filter` | bool | `false` | Filter profanity from results. |
| `max_retry_attempts` | int | `3` | Maximum reconnection attempts on failure. |
| `retry_delay` | float | `1.0` | Seconds between reconnection attempts. |
| `stream_max_duration` | int | `270` | Max stream duration in seconds before reconnect. |
| `dump` | bool | `false` | Dump incoming audio to a PCM file for debugging. |
| `dump_path` | string | `"."` | Directory path for audio dump files. |

## Environment Variables

| Variable | Description |
|----------|-------------|
| `GOOGLE_ASR_PROJECT_ID` | Google Cloud project ID |
| `GOOGLE_APPLICATION_CREDENTIALS_PATH` | Path to the service account credentials JSON file |
| `GOOGLE_APPLICATION_CREDENTIALS_STRING` | Service account credentials as a JSON string |
