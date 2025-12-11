# Gradium ASR Extension

## Overview

The Gradium ASR extension provides real-time speech-to-text transcription using the Gradium AI API. It supports WebSocket-based streaming for low-latency transcription.

## Features

- Real-time speech recognition via WebSocket
- Support for multiple regions (US and Europe)
- Voice Activity Detection (VAD) integration
- Configurable audio formats (PCM, WAV, Opus)
- Low-latency streaming transcription
- Interim and final transcription results

## Configuration

### Environment Variables

Set your Gradium API key as an environment variable:

```bash
export GRADIUM_API_KEY=your_api_key_here
```

### Property Configuration

Configure the extension in `property.json`:

```json
{
  "params": {
    "api_key": "${env:GRADIUM_API_KEY|}",
    "region": "us",
    "model_name": "default",
    "input_format": "pcm",
    "sample_rate": 24000,
    "language": ""
  }
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `api_key` | string | - | Gradium API key (required) |
| `region` | string | "us" | API region: "us" or "eu" |
| `model_name` | string | "default" | Gradium ASR model name |
| `input_format` | string | "pcm" | Audio format: "pcm", "wav", or "opus" |
| `sample_rate` | integer | 24000 | Audio sample rate in Hz |
| `language` | string | "" | Language code (optional) |

## Audio Requirements

The Gradium ASR service expects audio with the following specifications:

- **Sample Rate**: 24,000 Hz (24 kHz)
- **Format**: PCM (or WAV/Opus)
- **Bit Depth**: 16-bit signed integer
- **Channels**: Mono (1 channel)
- **Recommended Chunk Size**: 1920 samples per chunk (80ms)

## API Endpoints

### Region URLs

- **US Region**: `wss://us.api.gradium.ai/api/speech/asr`
- **EU Region**: `wss://eu.api.gradium.ai/api/speech/asr`

## Usage Example

### In a TEN App Graph

Add the Gradium ASR extension to your app configuration:

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "gradium_asr",
      "addon": "gradium_asr_python",
      "extension_group": "gradium_asr_group",
      "property": {
        "params": {
          "api_key": "${env:GRADIUM_API_KEY|}",
          "region": "us",
          "model_name": "default"
        }
      }
    }
  ]
}
```

### Connecting Audio Input

Connect your audio source to the Gradium ASR extension:

```json
{
  "connections": [
    {
      "extension_group": "audio_input_group",
      "extension": "audio_input",
      "audio_frame_out": [
        {
          "name": "pcm_frame",
          "dest": [
            {
              "extension_group": "gradium_asr_group",
              "extension": "gradium_asr"
            }
          ]
        }
      ]
    }
  ]
}
```

## Output

The extension outputs ASR results in the standard TEN ASR format:

```json
{
  "text": "transcribed text",
  "final": true,
  "start_ms": 0,
  "duration_ms": 1000,
  "language": "en"
}
```

### Result Fields

- `text`: Transcribed text content
- `final`: Boolean indicating if this is a final (vs interim) result
- `start_ms`: Start time in milliseconds
- `duration_ms`: Duration in milliseconds
- `language`: Detected or configured language

## Error Handling

The extension provides detailed error messages through the standard TEN error interface:

- Connection errors (WebSocket connection failures)
- Authentication errors (invalid API key)
- Transcription errors (processing failures)

## Dependencies

- `websockets>=14.0` - WebSocket client library
- `pydantic>=2.0.0` - Configuration validation
- `ten_runtime_python>=0.11` - TEN runtime
- `ten_ai_base>=0.7` - TEN AI base classes

## License

Same as the TEN framework.

## Support

For issues and questions:
- Gradium API: https://gradium.ai/api_docs.html
- TEN Framework: https://github.com/TEN-framework/TEN-framework
