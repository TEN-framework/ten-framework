# NVIDIA Riva TTS Python Extension

This extension provides text-to-speech functionality using NVIDIA Riva Speech Skills.

## Features


- High-quality speech synthesis using NVIDIA Riva
- Support for multiple languages and voices
- Streaming and batch synthesis modes
- SSML support for advanced speech control
- GPU-accelerated inference for low latency

## Prerequisites

- NVIDIA Riva server running and accessible
- Python 3.8+
- nvidia-riva-client package

## Configuration

The extension can be configured through your property.json:

```json
{
  "params": {
    "server": "localhost:50051",
    "language_code": "en-US",
    "voice_name": "English-US.Female-1",
    "sample_rate": 16000,
    "use_ssl": false
  }
}
```

### Configuration Options

**Parameters inside `params` object:**
- `server` (required): Riva server address (format: "host:port")
- `language_code` (required): Language code (e.g., "en-US", "es-ES")
- `voice_name` (required): Voice identifier (e.g., "English-US.Female-1")
- `sample_rate` (optional): Audio sample rate in Hz (default: 16000)
- `use_ssl` (optional): Use SSL for gRPC connection (default: false)

### Available Voices

Common voice names include:
- `English-US.Female-1`
- `English-US.Male-1`
- `English-GB.Female-1`
- `Spanish-US.Female-1`

Check your Riva server documentation for the full list of available voices.

## Setting up NVIDIA Riva Server

Follow the [NVIDIA Riva Quick Start Guide](https://docs.nvidia.com/deeplearning/riva/user-guide/docs/quick-start-guide.html) to set up a Riva server.

Quick setup with Docker:

```bash
# Download Riva Quick Start scripts
ngc registry resource download-version nvidia/riva/riva_quickstart:2.17.0

# Initialize and start Riva
cd riva_quickstart_v2.17.0
bash riva_init.sh
bash riva_start.sh
```

## Environment Variables

Set the Riva server address via environment variable:

```bash
export NVIDIA_RIVA_SERVER=localhost:50051
```

## Architecture

This extension follows the TEN Framework TTS extension pattern:

- `extension.py`: Main extension class
- `riva_tts.py`: Client implementation with Riva SDK integration
- `config.py`: Configuration model
- `addon.py`: Extension addon registration

## License

Apache 2.0

## Contributing

Contributions are welcome! Please submit issues and pull requests to the TEN Framework repository.
