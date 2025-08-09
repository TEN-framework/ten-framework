# AWS ASR Python Extension

A Python extension for AWS Automatic Speech Recognition (ASR) service, providing real-time speech-to-text conversion functionality with full asynchronous operation support using AWS Transcribe streaming API.

## Features

- **Full Async Support**: Complete asynchronous architecture for high-performance speech recognition
- **Real-time Streaming**: Low-latency real-time audio streaming using AWS Transcribe streaming API
- **AWS Transcribe API**: Enterprise-grade performance using AWS Transcribe streaming transcription API
- **Audio Dumping**: Optional audio recording functionality for debugging and analysis
- **Error Handling**: Comprehensive error handling and detailed logging
- **Multi-language Support**: Support for multiple languages through AWS Transcribe
- **Reconnection Management**: Automatic reconnection mechanism for service stability
- **Session Management**: Support for session ID and audio timeline management

## Configuration

The extension requires the following configuration parameters:

### Required Parameters

- `params`: AWS Transcribe configuration parameters, including authentication information and transcription settings

### Optional Parameters

- `dump`: Enable audio dumping (default: false)
- `dump_path`: Path for dumped audio files (default: "aws_asr_in.pcm")
- `log_level`: Log level (default: "INFO")
- `finalize_mode`: Finalization mode, either "disconnect" or "mute_pkg" (default: "disconnect")
- `mute_pkg_duration_ms`: Mute package duration in milliseconds (default: 800)

### AWS Transcribe Configuration Parameters

- `region`: AWS region, e.g. 'us-west-2'
- `access_key_id`: AWS access key ID
- `secret_access_key`: AWS secret access key
- `language_code`: Language code, e.g. 'en-US', 'zh-CN'
- `media_sample_rate_hz`: Audio sample rate (Hz), e.g. 16000
- `media_encoding`: Audio encoding format, e.g. 'pcm'
- `vocabulary_name`: Custom vocabulary name (optional) Reference: https://docs.aws.amazon.com/transcribe/latest/dg/custom-vocabulary.html
- `session_id`: Session ID (optional)
- `vocab_filter_method`: Vocabulary filter method (optional)
- `vocab_filter_name`: Vocabulary filter name (optional)
- `show_speaker_label`: Whether to show speaker labels (optional)
- `enable_channel_identification`: Whether to enable channel identification (optional)
- `number_of_channels`: Number of channels (optional)
- `enable_partial_results_stabilization`: Whether to enable partial results stabilization (optional)
- `partial_results_stability`: Partial results stability setting (optional)
- `language_model_name`: Language model name (optional)

### Configuration Example

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

The extension implements the `AsyncASRBaseExtension` interface, providing the following key methods:

### Core Methods

- `on_init()`: Initialize AWS ASR client and configuration
- `start_connection()`: Establish connection with AWS Transcribe service
- `stop_connection()`: Close connection with ASR service
- `send_audio()`: Send audio frames for recognition
- `finalize()`: Complete current recognition session
- `is_connected()`: Check connection status

### Internal Methods

- `_handle_transcript_event()`: Handle transcription events
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

Run unit tests:

```bash
pytest tests/
```

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

## Reconnection Mechanism

The extension includes automatic reconnection mechanism:
- Maximum 5 reconnection attempts
- Exponential backoff strategy: 300ms, 600ms, 1.2s, 2.4s, 4.8s
- Automatic counter reset after successful connection
- Detailed logging for monitoring and debugging

## Audio Format Support

- **PCM16**: 16-bit PCM audio format
- **Sample Rate**: Support for various sample rates (e.g., 16000 Hz)
- **Mono**: Support for mono audio processing

## Troubleshooting

### Common Issues

1. **Connection Failure**: Check AWS credentials and network connection
2. **Authentication Error**: Verify AWS access keys and permissions
3. **Audio Quality Issues**: Verify audio format and sample rate settings
4. **Performance Issues**: Adjust buffer settings and language models
5. **Logging Issues**: Configure appropriate log levels

### Debug Mode

Enable debug mode by setting `dump: true` in the configuration to record audio for analysis.

## License

This extension is part of TEN Framework and is licensed under Apache License, Version 2.0.
