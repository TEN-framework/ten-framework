# Deepgram WebSocket ASR Extension

Direct WebSocket integration with Deepgram's Speech-to-Text API supporting both Nova (v1) and Flux (v2) models.

## Features

- **Dual API Support**: Automatically detects and configures for Deepgram v1 (Nova) or v2 (Flux) endpoints
- **SDK-Free**: Uses `aiohttp` WebSocket client directly for better control and fewer dependencies
- **Flux Turn Detection**: Native support for Flux model's built-in turn detection via `EndOfTurn` events
- **Configurable Parameters**: Full control over Flux-specific parameters (`eot_threshold`, `eot_timeout_ms`, `eager_eot_threshold`)
- **Automatic Reconnection**: Handles connection drops and network issues gracefully

## Installation

### Dependencies

```bash
pip3 install aiohttp pydantic
```

### Add to Manifest

In your `tenapp/manifest.json`:

```json
{
  "dependencies": [
    {
      "path": "../../../ten_packages/extension/deepgram_ws_asr_python"
    }
  ]
}
```

After updating manifest:
```bash
cd tenapp && tman install
```

## Configuration

### Deepgram v1 (Nova Models)

For Nova-2, Nova-3, and other v1 models:

```json
{
  "type": "extension",
  "name": "stt",
  "addon": "deepgram_ws_asr_python",
  "extension_group": "stt",
  "property": {
    "params": {
      "api_key": "${env:DEEPGRAM_API_KEY}",
      "url": "wss://api.deepgram.com/v1/listen",
      "model": "nova-3",
      "language": "en-US"
    }
  }
}
```

**v1 Supported Parameters:**
- `api_key` (required): Your Deepgram API key
- `url` (optional): WebSocket endpoint (default: `wss://api.deepgram.com/v1/listen`)
- `model` (optional): Model name (default: `nova-2`)
- `language` (optional): Language code (default: `en-US`)
- `interim_results` (optional): Enable interim transcripts (default: true)
- `punctuate` (optional): Enable punctuation (default: true)
- `channels` (optional): Number of audio channels (default: 1)
- `keywords` (optional): Array of keywords to boost

### Deepgram v2 (Flux Models)

For Flux models with turn detection:

```json
{
  "type": "extension",
  "name": "stt",
  "addon": "deepgram_ws_asr_python",
  "extension_group": "stt",
  "property": {
    "params": {
      "api_key": "${env:DEEPGRAM_API_KEY}",
      "url": "wss://api.deepgram.com/v2/listen",
      "model": "flux-general-en",
      "language": "en-US",
      "eot_threshold": 0.7,
      "eot_timeout_ms": 3000,
      "eager_eot_threshold": 0.85
    }
  }
}
```

**v2 Flux-Specific Parameters:**
- `api_key` (required): Your Deepgram API key
- `url` (required): Must contain `/v2/` for v2 API
- `model` (required): Flux model name (e.g., `flux-general-en`, `flux-medical-en`)
- `language` (required): Language code
- `eot_threshold` (optional): End-of-turn threshold (0.0-1.0, default: 0.5)
- `eot_timeout_ms` (optional): Silence duration before turn ends in milliseconds (default: 2000)
- `eager_eot_threshold` (optional): Threshold for quick turn detection (0.0-1.0)

**⚠️ Important:** v2 API does NOT support v1 parameters like `interim_results`, `punctuate`, or `channels`. The extension automatically filters these out.

## API Version Detection

The extension automatically detects which API version to use based on:

1. **URL contains `/v2/`** → Uses v2 API (Flux)
2. **Model starts with `flux`** → Uses v2 API (Flux)
3. **Otherwise** → Uses v1 API (Nova)

## Turn Detection with Flux

Flux models provide built-in turn detection through `TurnInfo` events:

### Event Types

- **`Update`**: Interim transcript during speech
- **`EndOfTurn`**: User finished speaking (detected via silence)
- **`EagerEndOfTurn`**: Quick turn detection when confident (if `eager_eot_threshold` configured)
- **`TurnResumed`**: User resumed speaking after pause

### Timing Optimization

**Fast Response (1.5s silence):**
```json
{
  "eot_threshold": 0.7,
  "eot_timeout_ms": 1500
}
```
- **Pro**: Faster perceived response (~2-3s total latency)
- **Con**: May cut off slow speakers

**Balanced (3s silence - default):**
```json
{
  "eot_threshold": 0.7,
  "eot_timeout_ms": 3000
}
```
- **Pro**: Allows natural pauses mid-sentence
- **Con**: Higher perceived latency (~4-5s total)

**With Eager Detection:**
```json
{
  "eot_threshold": 0.7,
  "eot_timeout_ms": 3000,
  "eager_eot_threshold": 0.85
}
```
- **Pro**: Fast response when confident, fallback to longer timeout
- **Con**: More complex tuning required

## Message Formats

### v1 (Nova) Response
```json
{
  "type": "Results",
  "channel_index": [0, 1],
  "duration": 2.5,
  "start": 0.0,
  "is_final": true,
  "channel": {
    "alternatives": [
      {
        "transcript": "Hello, how are you?",
        "confidence": 0.95
      }
    ]
  }
}
```

### v2 (Flux) Response
```json
{
  "type": "TurnInfo",
  "transcript": "Hello, how are you?",
  "event": "EndOfTurn",
  "metadata": {
    "confidence": 0.95,
    "duration_ms": 2500
  }
}
```

## Output

The extension sends transcripts via TEN Framework `Data` messages with name `asr_result`:

```python
{
  "is_final": True,  # or False for interim results
  "text": "transcribed text",
  "stream_id": 0,
  "data_type": "transcribe",
  "text_ts": 1234567890  # Unix timestamp in milliseconds
}
```

## Example Graphs

### Basic Voice Assistant (Nova-3)
```json
{
  "nodes": [
    {
      "addon": "deepgram_ws_asr_python",
      "name": "stt",
      "property": {
        "params": {
          "api_key": "${env:DEEPGRAM_API_KEY}",
          "model": "nova-3"
        }
      }
    }
  ],
  "connections": [
    {
      "extension": "stt",
      "audio_frame": [
        {
          "name": "pcm_frame",
          "source": [{"extension": "agora_rtc"}]
        }
      ]
    },
    {
      "extension": "main_control",
      "data": [
        {
          "name": "asr_result",
          "source": [{"extension": "stt"}]
        }
      ]
    }
  ]
}
```

### Advanced with Flux Turn Detection
```json
{
  "nodes": [
    {
      "addon": "deepgram_ws_asr_python",
      "name": "stt",
      "property": {
        "params": {
          "api_key": "${env:DEEPGRAM_API_KEY}",
          "url": "wss://api.deepgram.com/v2/listen",
          "model": "flux-general-en",
          "language": "en-US",
          "eot_threshold": 0.7,
          "eot_timeout_ms": 1500
        }
      }
    }
  ],
  "connections": [
    {
      "extension": "stt",
      "audio_frame": [
        {
          "name": "pcm_frame",
          "source": [{"extension": "agora_rtc"}]
        }
      ]
    },
    {
      "extension": "main_control",
      "data": [
        {
          "name": "asr_result",
          "source": [{"extension": "stt"}]
        }
      ]
    }
  ]
}
```

## Troubleshooting

### Connection Issues

**Symptom**: Worker process dies after 60 seconds
**Cause**: Extension not in `manifest.json`
**Solution**:
```bash
# 1. Add to manifest.json dependencies
# 2. Run tman install
cd tenapp && tman install

# 3. Verify symlink exists
ls -la tenapp/ten_packages/extension/ | grep deepgram_ws_asr_python
```

### API Key Not Found

**Symptom**: Logs show `Environment variable DEEPGRAM_API_KEY is not found`
**Solution**:
```bash
# Option 1: Set in .env file
echo "DEEPGRAM_API_KEY=your_key_here" >> .env

# Option 2: Hardcode for testing
# In property.json: "api_key": "actual_key_value"
```

### v2 API HTTP 400 Error

**Symptom**: WebSocket connection fails with 400 Bad Request
**Cause**: Sending v1 parameters to v2 endpoint
**Solution**: Remove v1-only parameters (`interim_results`, `punctuate`, `channels`) from v2 config

### No Transcripts Received

**Check these in order:**
```bash
# 1. Verify API key is valid
curl "https://api.deepgram.com/v1/listen" \
  -H "Authorization: Token YOUR_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"url": "https://static.deepgram.com/examples/Bueller-Life-moves-pretty-fast.wav"}'

# 2. Check audio is flowing
# Look for: [DEEPGRAM-AUDIO] Sent frame #XXX in logs

# 3. Verify WebSocket connected
# Look for: [DEEPGRAM-WS] WebSocket connected successfully

# 4. Check for error messages
tail -100 /tmp/task_run.log | grep -E "ERROR|Traceback"
```

## Performance Tuning

### For Low Latency (Real-time Conversation)
```json
{
  "model": "flux-general-en",
  "eot_threshold": 0.75,
  "eot_timeout_ms": 1000,
  "eager_eot_threshold": 0.9
}
```

### For Accuracy (Presentations, Long Sentences)
```json
{
  "model": "flux-general-en",
  "eot_threshold": 0.6,
  "eot_timeout_ms": 4000
}
```

### For Balanced Performance (Default)
```json
{
  "model": "flux-general-en",
  "eot_threshold": 0.7,
  "eot_timeout_ms": 3000
}
```

## Comparison: v1 (Nova) vs v2 (Flux)

| Feature | v1 (Nova) | v2 (Flux) |
|---------|-----------|-----------|
| **Turn Detection** | External VAD required | Built-in |
| **Latency** | ~500-1000ms | ~260ms + EOT timeout |
| **Interim Results** | ✅ Yes | ✅ Yes (as Updates) |
| **Message Format** | Results object | TurnInfo events |
| **Parameters** | `interim_results`, `punctuate`, `channels` | `eot_*` thresholds |
| **Use Case** | General transcription | Real-time conversations |

## Testing

### Start Session
```bash
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{
    "graph_name": "your_graph_with_deepgram",
    "channel_name": "test_channel",
    "remote_stream_id": 123
  }'
```

### Monitor Logs
```bash
# Watch for Deepgram activity
tail -f /tmp/task_run.log | grep -i "DEEPGRAM\|FLUX"

# Look for:
# [DEEPGRAM-WS] Using v2 API for Flux
# [DEEPGRAM-WS] WebSocket connected successfully
# [DEEPGRAM-FLUX-TRANSCRIPT] Text: '...'
# [DEEPGRAM-FLUX] EndOfTurn event received
```

## References

- [Deepgram API Documentation](https://developers.deepgram.com/)
- [Flux Model Guide](https://developers.deepgram.com/docs/flux)
- [TEN Framework Documentation](https://doc.theten.ai)
- Implementation Guide: `ai/AI_working_with_ten.md`
- Setup Guide: `ai/deepgram/SETUP_VOICE_ASSISTANT_ADVANCED.md`

## Support

For issues or questions:
1. Check logs: `/tmp/task_run.log`
2. Verify configuration in `property.json`
3. Ensure manifest.json includes this extension
4. Run `tman install` after manifest changes

## License

Part of TEN Framework project.
