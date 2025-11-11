# Web Audio Control Go Extension

## Overview

This is a TEN Framework Web audio control extension written in Go. It provides a web interface that allows users to control audio playback and view real-time transcription text through a browser.

## Features

- ✅ Web interface control
- ✅ WebSocket real-time communication
- ✅ Send audio playback commands
- ✅ Receive and display transcription text
- ✅ Beautiful modern UI
- ✅ Automatic reconnection mechanism

## Architecture

```text
User Browser <--HTTP/WebSocket--> Web Server (Go Extension) <--TEN Protocol--> TEN Framework
                                                                  |
                                                                  v
                                                        Audio Player Extension
                                                                  |
                                                                  v
                                                          ASR Extension (Transcription)
```

## Configuration Properties

### `http_port` (int64, optional, default: 8080)

HTTP server listening port.

Example:

```json
{
  "http_port": 8080
}
```

## API

### Outgoing Commands

#### `start_play`

Send audio playback command.

**Command Properties**:

- `file_path` (string): Audio file path
- `loop_playback` (bool): Whether to loop playback

### Incoming Data

#### `display_text`

Receive transcription text data.

**Data Properties**:

- `text` (string): Transcribed text content

## Usage

### 1. Configure in Graph

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "web_control",
      "addon": "web_audio_control_go",
      "property": {
        "http_port": 8080
      }
    },
    {
      "type": "extension",
      "name": "audio_player",
      "addon": "audio_file_player_python"
    },
    {
      "type": "extension",
      "name": "asr",
      "addon": "your_asr_extension"
    }
  ],
  "connections": [
    {
      "extension": "web_control",
      "cmd_out": [
        {
          "name": "start_play",
          "dest": [
            {
              "extension": "audio_player"
            }
          ]
        }
      ]
    },
    {
      "extension": "audio_player",
      "audio_frame_out": [
        {
          "name": "pcm_frame",
          "dest": [
            {
              "extension": "asr"
            }
          ]
        }
      ]
    },
    {
      "extension": "asr",
      "data_out": [
        {
          "name": "display_text",
          "dest": [
            {
              "extension": "web_control"
            }
          ]
        }
      ]
    }
  ]
}
```

### 2. Access Web Interface

After starting the application, visit in your browser:

```text
http://localhost:8080
```

### 3. Using the Interface

1. Enter the audio file path in the "Audio File Path" input field
2. Optional: Check "Loop Playback" to enable loop playback
3. Click the "▶️ Start Transcription" button to begin
4. Transcription text will be displayed in real-time in the "Transcription Results" area below

## Web Interface Features

### Connection Status Indicator

- 🟢 **Connected**: Connected to server normally
- 🔴 **Disconnected**: Disconnected from server (will automatically reconnect)

### Real-time Transcription Display

- Transcription text is pushed to browser in real-time via WebSocket
- Each text is displayed as a card
- Automatically scrolls to latest content

### Error Messages

- Error messages displayed when file doesn't exist
- Clear error messages for network errors

## WebSocket Message Format

### Server -> Client

```json
{
  "type": "text",
  "data": "Transcribed text content"
}
```

## HTTP API

### POST /api/start_play

Start audio playback.

**Request Parameters** (Form Data):

- `file_path`: Audio file path (required)
- `loop_playback`: Whether to loop playback (true/false, optional)

**Response**:

```json
{
  "status": "ok",
  "message": "Playback started"
}
```

**Error Response**:

```json
{
  "error": "Error message"
}
```

## Technology Stack

- **Backend**: Go
- **Web Framework**: net/http (standard library)
- **WebSocket**: gorilla/websocket
- **Frontend**: HTML5 + CSS3 + Vanilla JavaScript
- **TEN Framework**: Go binding

## Development

### Dependencies

```go
require (
    github.com/gorilla/websocket v1.5.1
    ten_framework/ten_runtime v0.11.25
)
```

### Project Structure

```text
web_audio_control_go/
├── main.go                 # Extension entry point
├── server/
│   ├── server.go          # Web server implementation
│   └── static/
│       └── index.html     # Web frontend page
├── manifest.json          # Extension manifest
├── property.json          # Default configuration
├── go.mod                 # Go module definition
└── docs/                  # Documentation
```

## License

Apache License 2.0

## Authors

TEN Framework Team

## Changelog

### v0.11.25 (2025-10-29)

- Initial release
- Web interface control
- WebSocket real-time communication
- Audio playback command sending
- Transcription text reception and display
