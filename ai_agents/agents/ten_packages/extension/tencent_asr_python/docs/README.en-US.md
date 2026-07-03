# Tencent ASR Async Python Extension

A Python extension for Tencent Cloud Automatic Speech Recognition (ASR) service, providing real-time speech-to-text conversion capabilities with full async support.

## Features

- **Full Async Support**: Built with complete asynchronous architecture for high-performance speech recognition
- **Real-time Streaming**: Supports real-time audio streaming with low latency
- **Multiple Finalize Modes**: Configurable finalization strategies (disconnect, mute package, vendor-defined)
- **Audio Dumping**: Optional audio recording for debugging and analysis
- **Keep-alive Support**: Configurable connection keep-alive mechanism
- **Error Handling**: Comprehensive error handling with detailed logging
- **Multi-language Support**: Supports multiple languages and locales
- **Configurable Logging**: Adjustable log levels for debugging

## Configuration

The extension requires the following configuration parameters:

### Required Parameters

- `app_id`: Tencent Cloud ASR application ID
- `secret_key`: Tencent Cloud ASR secret key
- `params`: ASR request parameters (language, audio format, etc.)

### Optional Parameters

- `finalize_mode`: Finalization strategy
  - `disconnect`: Disconnect after finalization
  - `mute_pkg`: Send mute packages
  - `vendor_defined`: Use vendor-defined strategy (default)
- `mute_pkg_duration_ms`: Duration for mute packages (default: 800ms)
- `dump`: Enable audio dumping (default: false)
- `dump_path`: Path for dumped audio files
- `keep_alive_interval`: Keep-alive interval in seconds
- `log_level`: Logging level (default: "INFO")

### Example Configuration

```json
{
  "app_id": "your_app_id",
  "secret_key": "your_secret_key",
  "params": {
    "language": "zh-CN",
    "format": "pcm",
    "sample_rate": 16000
  },
  "finalize_mode": "vendor_defined",
  "dump": false,
  "log_level": "INFO"
}
```

## API

The extension implements the `AsyncASRBaseExtension` interface and provides the following key methods:

### Core Methods

- `on_init()`: Initialize the ASR client and configuration
- `start_connection()`: Establish connection to Tencent ASR service
- `stop_connection()`: Close connection to ASR service
- `send_audio()`: Send audio frames for recognition
- `finalize()`: Finalize the current recognition session

### Event Handlers

- `on_asr_start()`: Called when ASR session starts
- `on_asr_sentence_start()`: Called when a new sentence begins
- `on_asr_sentence_change()`: Called when sentence content changes
- `on_asr_sentence_end()`: Called when a sentence ends
- `on_asr_complete()`: Called when ASR session completes
- `on_asr_fail()`: Called when ASR fails
- `on_asr_error()`: Called when ASR encounters an error

## Dependencies

- `typing_extensions`: For type hints
- `pydantic`: For configuration validation
- `websockets`: For WebSocket communication
- `pytest`: For testing (development dependency)

## Development

### Building

The extension is built as part of the TEN Framework build system. No additional build steps are required.

### Testing

Run the unit tests using:

```bash
pytest tests/
```

The extension includes comprehensive tests for:
- Configuration validation
- Audio processing
- Error handling
- Connection management

## Usage

1. **Installation**: The extension is automatically installed with the TEN Framework
2. **Configuration**: Set up your Tencent Cloud ASR credentials and parameters
3. **Integration**: Use the extension through the TEN Framework ASR interface
4. **Monitoring**: Check logs for debugging and monitoring

## Error Handling

The extension provides detailed error information through:
- Module error codes
- Vendor-specific error details
- Comprehensive logging
- Graceful degradation

## Performance

- **Low Latency**: Optimized for real-time processing
- **High Throughput**: Efficient audio frame processing
- **Memory Efficient**: Minimal memory footprint
- **Connection Reuse**: Maintains persistent connections when possible

## Security

- **Credential Encryption**: Sensitive credentials are encrypted in configuration
- **Secure Communication**: Uses secure WebSocket connections
- **Input Validation**: Comprehensive input validation and sanitization

## Troubleshooting

### Common Issues

1. **Connection Failures**: Check app_id and secret_key configuration
2. **Audio Quality Issues**: Verify audio format and sample rate settings
3. **Performance Problems**: Adjust buffer settings and finalize mode
4. **Logging Issues**: Configure appropriate log levels

### Debug Mode

Enable debug mode by setting `dump: true` in configuration to record audio for analysis.

## License

This extension is part of the TEN Framework and is licensed under the Apache License, Version 2.0.
