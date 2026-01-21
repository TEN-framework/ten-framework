# NVIDIA Riva TTS Extension - Implementation Details

## Overview

This document describes the implementation of the NVIDIA Riva TTS extension for TEN Framework. The extension provides high-quality, GPU-accelerated text-to-speech synthesis using NVIDIA Riva Speech Skills.

## Architecture

### Component Structure

```
nvidia_riva_tts_python/
├── extension.py          # Main extension class
├── riva_tts.py          # Riva client implementation
├── config.py            # Configuration model
├── addon.py             # Extension registration
├── manifest.json        # Extension metadata
├── property.json        # Default properties
├── requirements.txt     # Python dependencies
├── README.md            # User documentation
└── tests/               # Test suite
    ├── test_config.py
    └── test_extension.py
```

### Class Hierarchy

```
AsyncTTSExtension (base class from ten_ai_base)
    └── NvidiaRivaTTSExtension
            └── uses NvidiaRivaTTSClient
                    └── uses riva.client.SpeechSynthesisService
```

## Implementation Details

### 1. Extension Class (`extension.py`)

The `NvidiaRivaTTSExtension` class inherits from `AsyncTTSExtension` and implements the required abstract methods:

- **`create_config()`**: Parses JSON configuration into `NvidiaRivaTTSConfig`
- **`create_client()`**: Instantiates `NvidiaRivaTTSClient` with configuration
- **`vendor()`**: Returns "nvidia_riva" as the vendor identifier
- **`synthesize_audio_sample_rate()`**: Returns the configured sample rate

### 2. Client Implementation (`riva_tts.py`)

The `NvidiaRivaTTSClient` class handles the actual TTS synthesis:

#### Initialization
- Creates Riva Auth object with server URI and SSL settings
- Initializes `SpeechSynthesisService` for TTS operations
- Validates server connectivity

#### Synthesis Method
```python
async def synthesize(self, text: str, request_id: str) -> AsyncIterator[bytes]
```

**Flow:**
1. Validates input text (non-empty)
2. Calls `tts_service.synthesize_online()` for streaming synthesis
3. Iterates through audio chunks from Riva
4. Converts audio data to PCM bytes
5. Yields audio chunks for streaming playback
6. Handles cancellation requests

**Key Features:**
- Streaming synthesis for low latency
- Cancellation support via `_is_cancelled` flag
- Comprehensive logging at each step
- Error handling with detailed messages

### 3. Configuration (`config.py`)

The `NvidiaRivaTTSConfig` class extends `AsyncTTSConfig`:

**Required Parameters:**
- `server`: Riva server address (host:port)
- `language_code`: Language identifier (e.g., "en-US")
- `voice_name`: Voice identifier (e.g., "English-US.Female-1")

**Optional Parameters:**
- `sample_rate`: Audio sample rate in Hz (default: 16000)
- `use_ssl`: Enable SSL for gRPC (default: false)

**Validation:**
- Ensures all required parameters are present
- Validates parameter types and formats

### 4. Addon Registration (`addon.py`)

Registers the extension with TEN Framework using the `@register_addon_as_extension` decorator.

## Integration with TEN Framework

### TTS Interface Compliance

The extension implements the standard TEN Framework TTS interface defined in `ten_ai_base/api/tts-interface.json`:

- **Input**: Text data via TEN data messages
- **Output**: PCM audio frames via TEN audio frame messages
- **Control**: Start/stop/cancel commands via TEN commands

### Message Flow

```
1. Text Input → Extension receives text data
2. Configuration → Loads voice, language, sample rate
3. Synthesis → Calls Riva API with streaming
4. Audio Output → Yields PCM audio chunks
5. Completion → Signals end of synthesis
```

## NVIDIA Riva Integration

### gRPC API Usage

The extension uses the official `nvidia-riva-client` Python package which provides:

- **Auth**: Authentication and connection management
- **SpeechSynthesisService**: TTS API wrapper
- **AudioEncoding**: Audio format specifications

### Streaming vs Batch

The implementation uses **streaming synthesis** (`synthesize_online`) for:
- Lower latency (first audio chunk arrives quickly)
- Better user experience in real-time applications
- Efficient memory usage

Alternative batch mode (`synthesize`) is available but not used by default.

### Audio Format

- **Encoding**: LINEAR_PCM (16-bit signed integer)
- **Sample Rate**: Configurable (default 16000 Hz)
- **Channels**: Mono
- **Byte Order**: Little-endian

## Error Handling

### Initialization Errors
- Server unreachable → RuntimeError with connection details
- Invalid credentials → Authentication error
- Missing dependencies → Import error

### Runtime Errors
- Empty text → Warning logged, no synthesis
- Synthesis failure → RuntimeError with Riva error message
- Cancellation → Graceful stop, log cancellation

### Logging Strategy

- **INFO**: Initialization, configuration
- **DEBUG**: Synthesis progress, chunk details
- **WARN**: Empty text, unusual conditions
- **ERROR**: Failures, exceptions

## Testing

### Test Coverage

1. **Configuration Tests** (`test_config.py`)
   - Valid configuration creation
   - Missing required parameters
   - Default values
   - Validation logic

2. **Extension Tests** (`test_extension.py`)
   - Extension initialization
   - Config creation from JSON
   - Sample rate retrieval
   - Client creation

3. **Client Tests** (`test_extension.py`)
   - Client initialization with mocked Riva
   - Cancellation handling
   - Empty text handling
   - Synthesis with mocked responses

### Running Tests

```bash
# Install test dependencies
pip install pytest pytest-asyncio

# Run all tests
pytest nvidia_riva_tts_python/tests/ -v

# Run with coverage
pytest nvidia_riva_tts_python/tests/ --cov=nvidia_riva_tts_python
```

## Performance Considerations

### Latency
- **First chunk**: ~100-200ms (depends on text length and server)
- **Streaming**: Continuous audio delivery
- **GPU acceleration**: Significantly faster than CPU-only TTS

### Resource Usage
- **Memory**: Minimal (streaming mode)
- **Network**: gRPC connection to Riva server
- **CPU**: Low (Riva does GPU processing)

### Optimization Tips
1. Use streaming mode for real-time applications
2. Keep Riva server close to application (low network latency)
3. Reuse client connections (handled by extension)
4. Configure appropriate sample rate for use case

## Deployment

### Prerequisites
1. NVIDIA Riva server running (see README.md for setup)
2. Network connectivity to Riva server
3. Python 3.8+ with nvidia-riva-client

### Configuration Example

```json
{
  "params": {
    "server": "riva-server.example.com:50051",
    "language_code": "en-US",
    "voice_name": "English-US.Female-1",
    "sample_rate": 22050,
    "use_ssl": true
  }
}
```

### Environment Variables

```bash
export NVIDIA_RIVA_SERVER=localhost:50051
```

## Future Enhancements

Potential improvements for future versions:

1. **SSML Support**: Full SSML tag support for advanced speech control
2. **Voice Cloning**: Custom voice model support
3. **Multi-language**: Automatic language detection
4. **Caching**: Cache frequently synthesized phrases
5. **Metrics**: Detailed performance metrics and monitoring
6. **Fallback**: Automatic fallback to alternative TTS if Riva unavailable

## References

- [NVIDIA Riva Documentation](https://docs.nvidia.com/deeplearning/riva/user-guide/docs/index.html)
- [Riva Python Client](https://pypi.org/project/nvidia-riva-client/)
- [TEN Framework TTS Interface](https://github.com/TEN-framework/ten-framework)
- [gRPC Python](https://grpc.io/docs/languages/python/)

## License

Apache 2.0 - See LICENSE file in the TEN Framework repository.

## Contributing

Contributions are welcome! Please:
1. Follow the existing code style
2. Add tests for new features
3. Update documentation
4. Submit PR to TEN Framework repository

