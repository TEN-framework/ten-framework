# StepFun TTS Extension

A TEN Framework extension for text-to-speech synthesis using the [StepFun Realtime Audio API](https://platform.stepfun.com/docs/audio/realtimeaudio).

## Configuration

All parameters are configured through the `params` object in `property.json`.

### Basic configuration

```json
{
    "params": {
        "api_key": "${env:STEPFUN_TTS_KEY|}",
        "base_url": "wss://api.stepfun.com/v1/realtime/audio",
        "model": "step-tts-mini",
        "voice_id": "cixingnansheng"
    }
}
```

### Custom voice and model

```json
{
    "params": {
        "api_key": "${env:STEPFUN_TTS_KEY|}",
        "base_url": "wss://api.stepfun.com/v1/realtime/audio",
        "model": "step-tts-mini",
        "voice_id": "wenrouzhixin"
    }
}
```

## Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `api_key` | string | `""` | StepFun API key. |
| `base_url` | string | `"wss://api.stepfun.com/v1/realtime/audio"` | StepFun realtime audio WebSocket endpoint. |
| `model` | string | `"step-tts-mini"` | TTS model name. |
| `voice_id` | string | `"cixingnansheng"` | Voice identifier for synthesis. |

## Environment Variables

| Variable | Description |
|----------|-------------|
| `STEPFUN_TTS_KEY` | StepFun API key |
