# Transcriber Demo Application

## Overview

Transcriber Demo is a real-time/offline speech transcription demonstration application built on the TEN Framework. The application supports real-time voice input through a web interface microphone or uploading/specifying offline audio files for transcription, with real-time display of transcription results.

## Features

- ✅ **Real-time Speech Transcription**: Support real-time speech recognition through microphone
- 📁 **Offline File Transcription**: Support uploading or specifying audio files for transcription
- 🌐 **Web Interface**: Provides a user-friendly web control interface
- 🔄 **Real-time Result Push**: Push transcription results in real-time via WebSocket
- 🎵 **Multiple Format Support**: Support various audio formats including WAV, MP3, FLAC, OGG, M4A
- 🔁 **Loop Playback**: Support audio file loop playback functionality

## Architecture

This application is based on the TEN Framework's Extension Graph architecture and includes the following core components:

### Extension Components

1. **web_audio_control_go**: Web server extension
   - Provides HTTP service and WebSocket connection
   - Handles audio control requests
   - Pushes transcription results to frontend in real-time

2. **audio_file_player_python**: Audio player extension
   - Reads audio files
   - Converts audio data to PCM frame stream
   - Supports loop playback

3. **azure_asr_python**: Azure speech recognition extension
   - Receives PCM audio frames
   - Calls Azure Speech Service for speech recognition
   - Returns recognition results (including interim and final results)

### Data Flow

```text
User Input (Microphone/File)
    ↓
web_audio_control_go / audio_file_player_python
    ↓ (PCM Audio Frames)
azure_asr_python
    ↓ (ASR Results)
web_audio_control_go
    ↓ (WebSocket)
Web Frontend Display
```

## Getting Started

### Prerequisites

1. **Runtime Environment**:
   - Go 1.16+ (for Go extensions)
   - Python 3.10 (for Python extensions)

2. **Azure Speech Service**:
   - Azure subscription account
   - Speech Service API Key and Region

### Configuration

Create a `.env` file or set environment variables:

```bash
# Azure Speech Service Configuration
AZURE_STT_KEY=your_azure_speech_api_key
AZURE_STT_REGION=your_azure_region  # e.g., eastus
AZURE_STT_LANGUAGE=en-US            # Language code, defaults to en-US
```

Supported language code examples:

- `en-US`: American English
- `zh-CN`: Simplified Chinese
- `ja-JP`: Japanese
- `ko-KR`: Korean

### Installing Dependencies

1. **Install Python Dependencies**:

```bash
# Execute in the application root directory
python3 scripts/install_python_deps.py
```

This script will automatically:

- Merge all Python extension dependencies
- Install to Python 3.10 environment

2. **Build Go Application**:

```bash
# Execute in the application root directory
go run ten_packages/system/ten_runtime_go/tools/build/main.go
```

### Starting the Application

```bash
# Execute in the application root directory
./bin/start
```

The application listens on the default port (usually 8002) and can be accessed through a browser after startup.

## Usage

### Method 1: Real-time Microphone Transcription

1. Open `http://localhost:8002/static/microphone.html` in your browser
2. Authorize the browser to use the microphone
3. Click the start recording button
4. Start speaking, transcription results will be displayed on the page in real-time

### Method 2: Offline File Transcription

1. Open `http://localhost:8002/static/index.html` in your browser
2. Choose a method:
   - **Upload File**: Click the "Select File" button to upload a local audio file
   - **Server Path**: Directly input the audio file path on the server
3. (Optional) Check "Loop Playback" for looping
4. Click "Start Transcription" to begin transcription
5. Transcription results will be displayed in real-time in the transcription results area below

### Transcription Results

- **Interim Results** (yellow marker): Temporary results during recognition, subject to change
- **Final Results** (green marker): Confirmed final recognition results

## Directory Structure

```text
transcriber_demo/
├── main.go                          # Application entry
├── manifest.json                    # Application manifest
├── property.json                    # Application properties and graph configuration
├── bin/
│   └── start                        # Startup script
├── scripts/
│   └── install_python_deps.py       # Python dependency installation script
├── docs/                            # Documentation directory
│   ├── README.zh-CN.md
│   └── README.en-US.md
└── ten_packages/
    └── extension/
        ├── web_audio_control_go/    # Web control extension
        ├── audio_file_player_python/# Audio player extension
        └── azure_asr_python/        # Azure ASR extension
```

## Development

### Modifying Configuration

The application's graph configuration is located in the `property.json` file, where you can modify:

- Extension connection relationships
- Extension property configurations
- Log output configuration

### Adding New Extensions

1. Add a new extension node in the `nodes` section of `property.json`
2. Define data flow connections in `connections`
3. Add dependency declarations in the `dependencies` section of `manifest.json`

### Debugging

Application logs are output to:

- Console: WARNING level and above
- File: `logs/debug.log` (DEBUG level and above)

## Troubleshooting

### Common Issues

1. **Empty or incorrect transcription results**
   - Check if Azure Speech Service configuration is correct
   - Verify API Key and Region are valid
   - Check network connection

2. **Unable to upload files**
   - Check if audio file format is supported
   - Verify file size does not exceed limits

3. **WebSocket connection failure**
   - Check if application started normally
   - Verify port is not occupied
   - Check firewall settings

4. **Python dependency installation failure**
   - Verify Python 3.10 is properly installed
   - Try using a mirror: `python3 scripts/install_python_deps.py --index-url https://pypi.org/simple`

## License

This application is part of the TEN Framework project and is licensed under Apache License 2.0.

## Related Links

- [TEN Framework Official Documentation](https://doc.theten.ai)
- [Azure Speech Service Documentation](https://docs.microsoft.com/azure/cognitive-services/speech-service/)
