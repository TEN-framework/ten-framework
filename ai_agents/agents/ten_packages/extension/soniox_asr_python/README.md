# soniox_asr_python

Soniox ASR (Automatic Speech Recognition) extension for TEN Framework.

## Features

- Real-time speech recognition using Soniox API
- Support for multiple languages
- Configurable audio parameters
- Standardized ASR interface compliance
- Error handling and reconnection support
- Audio dumping for debugging

## Configuration

### Required Parameters

- `api_key`: Your Soniox API key (required)

### Optional Parameters

- `url`: WebSocket URL (default: "wss://stt-rt.soniox.com/transcribe-websocket")
- `model`: ASR model to use (default: "stt-rt-preview")
- `language_hints`: Primary language for recognition (default: ["en"])
- `sample_rate`: Audio sample rate in Hz (default: 16000)
- `drain_holding_until_fin`: Whether to hold final tokens until drain (default: true)
- `dump`: Enable audio dumping for debugging (default: false)
- `dump_path`: Path for audio dump files (default: ".")

## API

Refer to `api` definition in [manifest.json](manifest.json) and default values in [property.json](property.json).

The extension implements the standard ASR interface and outputs `asr_result` data with the following structure:

```json
{
  "id": "unique_result_id",
  "text": "recognized text",
  "final": true,
  "start_ms": 1000,
  "duration_ms": 500,
  "language": "en",
  "words": [
    {
      "word": "hello",
      "start_ms": 1000,
      "duration_ms": 200,
      "stable": true
    }
  ],
  "metadata": {
    "session_id": "session_identifier"
  }
}
```

## Development

### Build

The extension requires Python 3.8+ and the following dependencies:
- pydantic>=2.0.0
- websockets>=11.0.0

### Unit test

Run the tests using:
```bash
cd tests
./bin/start
```

## Migration Notes

This extension has been migrated from the old ASR interface to the new standardized ASR interface. Key changes include:

- Updated to use `AsyncASRBaseExtension` base class
- Standardized output format using `asr_result`
- Modern error handling with `ModuleError`
- Updated runtime imports (`ten_runtime`, `ten_ai_base`)
- Improved configuration management
