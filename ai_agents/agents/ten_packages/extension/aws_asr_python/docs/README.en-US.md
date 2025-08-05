# AWS ASR Python Extension

A Python extension for AWS Automatic Speech Recognition (ASR) service, providing real-time speech-to-text conversion capabilities with full async support using AWS Transcribe streaming API.

## Features

- **Full Async Support**: Built with complete asynchronous architecture for high-performance speech recognition
- **Real-time Streaming**: Supports real-time audio streaming with low latency using AWS Transcribe streaming API
- **AWS Transcribe API**: Uses AWS Transcribe streaming transcription API for enterprise-grade performance
- **Multiple Audio Formats**: Supports PCM16 audio format
- **Audio Dumping**: Optional audio recording for debugging and analysis
- **Configurable Logging**: Adjustable log levels for debugging
- **Error Handling**: Comprehensive error handling with detailed logging
- **Multi-language Support**: Supports multiple languages through AWS Transcribe
- **Reconnection Management**: Automatic reconnection mechanism for service stability
- **Session Management**: Supports session ID and audio timeline management

## Configuration

The extension requires the following configuration parameters:

### Required Parameters

- `params`: AWS Transcribe configuration parameters including authentication and transcription settings

### Optional Parameters

- `dump`: Enable audio dumping (default: false)
- `dump_path`: Path for dumped audio files (default: "aws_asr_in.pcm")
- `log_level`: Logging level (default: "INFO")
- `finalize_mode`: Finalization mode, either "disconnect" or "mute_pkg" (default: "disconnect")
- `mute_pkg_duration_ms`: Mute package duration in milliseconds (default: 800)

### AWS Transcribe Configuration Parameters

- `region`: AWS region, e.g. 'us-west-2'
- `access_key_id`: AWS access key ID
- `secret_access_key`: AWS secret access key
- `language_code`: Language code, e.g. 'en-US', 'zh-CN'
- `media_sample_rate_hz`: Audio sample rate (Hz), e.g. 16000
- `media_encoding`: Audio encoding format, e.g. 'pcm'
- `vocabulary_name`: Custom vocabulary name (optional)
- `session_id`: Session ID (optional)
- `vocab_filter_method`: Vocabulary filter method (optional)
- `vocab_filter_name`: Vocabulary filter name (optional)
- `show_speaker_label`: Whether to show speaker labels (optional)
- `enable_channel_identification`: Whether to enable channel identification (optional)
- `number_of_channels`: Number of channels (optional)
- `enable_partial_results_stabilization`: Whether to enable partial results stabilization (optional)
- `partial_results_stability`: Partial results stability setting (optional)
- `language_model_name`: Language model name (optional)

### Example Configuration

```json
{
  "params": {
    "region": "us-west-2",
    "access_key_id": "your_aws_access_key_id",
    "secret_access_key": "your_aws_secret_access_key",
    "language_code": "en-US",
    "media_sample_rate_hz": 16000,
    "media_encoding": "pcm",
    "vocabulary_name": "custom-vocabulary",
    "show_speaker_label": true,
    "enable_partial_results_stabilization": true,
    "partial_results_stability": "HIGH"
  },
  "dump": false,
  "log_level": "INFO",
  "finalize_mode": "disconnect",
  "mute_pkg_duration_ms": 800
}
```

## API

The extension implements the `AsyncASRBaseExtension` interface and provides the following key methods:

### Core Methods

- `on_init()`: Initialize the AWS ASR client and configuration
- `start_connection()`: Establish connection to AWS Transcribe service
- `stop_connection()`: Close connection to ASR service
- `send_audio()`: Send audio frames for recognition
- `finalize()`: Finalize the current recognition session
- `is_connected()`: Check connection status

### Event Handlers

- `on_asr_start()`: Called when ASR session starts
- `on_asr_delta()`: Called when transcription delta is received
- `on_asr_completed()`: Called when transcription is completed
- `on_asr_committed()`: Called when audio buffer is committed
- `on_asr_server_error()`: Called when server error occurs
- `on_asr_client_error()`: Called when client error occurs

### Internal Methods

- `_handle_transcript_event()`: Handle transcript events
- `_disconnect_aws()`: Disconnect from AWS
- `_reconnect_aws()`: Reconnect to AWS
- `_handle_finalize_disconnect()`: Handle disconnect finalization
- `_handle_finalize_mute_pkg()`: Handle mute package finalization

## Dependencies

- `typing_extensions`: For type hints
- `pydantic`: For configuration validation and data models
- `amazon-transcribe`: AWS Transcribe Python client library
- `pytest`: For testing (development dependency)

## Development

### Building

The extension is built as part of the TEN Framework build system. No additional build steps are required.

### Testing

Run the unit tests using:

```bash
pytest tests/
```

The extension includes comprehensive tests:
- Configuration validation
- Audio processing
- Error handling
- Connection management
- Transcription result processing

## Usage

1. **Installation**: The extension is automatically installed with TEN Framework
2. **Configuration**: Set up your AWS credentials and Transcribe parameters
3. **Integration**: Use the extension through TEN Framework ASR interface
4. **Monitoring**: Check logs for debugging and monitoring

## Error Handling

The extension provides detailed error information through:
- Module error codes
- AWS-specific error details
- Comprehensive logging
- Graceful degradation and reconnection mechanisms

## Performance

- **Low Latency**: Optimized real-time processing using AWS Transcribe streaming API
- **High Throughput**: Efficient audio frame processing
- **Memory Efficient**: Minimal memory footprint
- **Connection Reuse**: Maintains persistent connections
- **Auto Reconnection**: Automatic reconnection on network interruptions

## Security

- **Credential Encryption**: Sensitive credentials are encrypted in configuration
- **Secure Communication**: Uses secure connections with AWS
- **Input Validation**: Comprehensive input validation and sanitization
- **IAM Permissions**: Supports AWS IAM permission management

## Supported AWS Features

The extension supports various AWS Transcribe features:
- **Multi-language Support**: Supports multiple languages and dialects
- **Custom Vocabulary**: Supports custom vocabulary tables
- **Vocabulary Filtering**: Supports vocabulary filtering functionality
- **Speaker Identification**: Supports speaker labels
- **Channel Identification**: Supports multi-channel audio processing
- **Partial Results**: Supports real-time partial results
- **Result Stabilization**: Supports result stabilization settings

## Audio Format Support

- **PCM16**: 16-bit PCM audio format
- **Sample Rates**: Supports various sample rates (e.g., 16000 Hz)
- **Mono Channel**: Supports mono channel audio processing

## Troubleshooting

### Common Issues

1. **Connection Failures**: Check AWS credentials and network connectivity
2. **Authentication Errors**: Verify AWS access keys and permissions
3. **Audio Quality Issues**: Validate audio format and sample rate settings
4. **Performance Issues**: Adjust buffer settings and language models
5. **Logging Issues**: Configure appropriate log levels

### Debug Mode

Enable debug mode by setting `dump: true` in configuration to record audio for analysis.

### Reconnection Mechanism

The extension includes automatic reconnection mechanism:
- Automatic reconnection on network interruptions
- Configurable reconnection strategies
- Connection status monitoring

## License

This extension is part of TEN Framework and is licensed under Apache License, Version 2.0.
