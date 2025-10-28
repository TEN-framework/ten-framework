# Voice Assistant with Generic Video Generation

This example demonstrates a voice-assistant with generic video generation integration using the ConvoAI-to-Video protocol.

## Overview

This agent combines:
- **Speech-to-Text** (Deepgram)
- **LLM** (OpenAI GPT)
- **Text-to-Speech** (ElevenLabs)
- **Generic Video** - External video generation service via standardized API

## How It Works

```
User Speech → STT → LLM → TTS → Generic Video Extension → External Service
                                    ↓
                              Video Service joins Agora channel as UID 12345
```

The generic video extension:
1. Receives TTS audio frames from ElevenLabs
2. Starts a video generation session with your external service
3. Streams audio to external service via WebSocket
4. External service generates video synchronized with the audio
5. External service publishes the video to the Agora RTC channel
6. Users see the generated video speaking the responses

## Prerequisites

### Required API Keys

Set these in your `.env` file:

```bash
# Agora (for real-time communication)
AGORA_APP_ID=your_agora_app_id
AGORA_APP_CERTIFICATE=  # Optional, leave empty if not using token auth

# Your Video Generation Service
GENERIC_VIDEO_API_KEY=your_video_service_api_key

# Speech-to-Text
DEEPGRAM_API_KEY=your_deepgram_api_key

# LLM
OPENAI_API_KEY=your_openai_api_key
OPENAI_MODEL=gpt-4o-mini

# Text-to-Speech
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key
```

### External Video Generation Service

You need to implement a video generation service that follows the ConvoAI-to-Video protocol.
See: https://github.com/AgoraIO-Solutions/convoai_to_video

Your service must implement:
- `POST /session/start` - Start video generation session
- `DELETE /session/stop` - Stop video generation session
- WebSocket endpoint for audio streaming

## Configuration

### Video Service Settings

In `tenapp/property.json`, the avatar node is configured with:

```json
{
  "name": "avatar",
  "addon": "generic_video_python",
  "property": {
    "generic_video_api_key": "${env:GENERIC_VIDEO_API_KEY|}",
    "agora_appid": "${env:AGORA_APP_ID}",
    "agora_appcert": "${env:AGORA_APP_CERTIFICATE|}",
    "agora_channel_name": "ten_agent_test",
    "agora_video_uid": 12345,
    "avatar_id": "16cb73e7de08",
    "quality": "high",
    "version": "v1",
    "video_encoding": "H264",
    "enable_string_uid": false,
    "activity_idle_timeout": 60,
    "start_endpoint": "https://api.example.com/v1/sessions/start",
    "stop_endpoint": "https://api.example.com/v1/sessions/stop",
    "input_audio_sample_rate": 16000
  }
}
```

### Key Parameters

- **start_endpoint**: URL for starting video generation sessions
- **stop_endpoint**: URL for stopping video generation sessions
- **agora_video_uid**: The UID your service will use when joining the Agora channel (default: 12345)
- **avatar_id**: Identifier for the avatar/video style to use
- **quality**: Video quality setting ("low", "medium", "high")
- **video_encoding**: Video codec ("H264", "VP8", "AV1")
- **input_audio_sample_rate**: Audio sample rate from TTS (16000 for ElevenLabs)

## Protocol Specification

The generic_video_python extension implements the ConvoAI-to-Video protocol:

### Session Start Request

```json
POST /session/start
{
  "avatar_id": "16cb73e7de08",
  "quality": "high",
  "version": "v1",
  "video_encoding": "H264",
  "activity_idle_timeout": 60,
  "agora_settings": {
    "app_id": "your_agora_app_id",
    "token": "generated_token",
    "channel": "channel_name",
    "uid": "12345",
    "enable_string_uid": false
  }
}
```

### Session Start Response

```json
{
  "session_id": "550e8400-e29b-41d4-a716-446655440000",
  "websocket_address": "wss://api.example.com/v1/websocket",
  "session_token": "jwt_token_for_websocket_auth"
}
```

### WebSocket Audio Streaming

Once connected, the extension sends audio frames as:

```json
{
  "type": "agent.speak",
  "audio": "base64_encoded_audio_data",
  "event_id": "unique_event_id"
}
```

## Running the Example

1. **Set up your video generation service** following the ConvoAI-to-Video specification

2. **Install dependencies:**
   ```bash
   cd tenapp
   ./scripts/install_python_deps.sh
   ```

3. **Configure environment:**
   ```bash
   cp .env.example .env
   # Edit .env with your API keys and service endpoints
   ```

4. **Update property.json** with your service endpoints:
   ```json
   "start_endpoint": "https://your-service.com/v1/sessions/start",
   "stop_endpoint": "https://your-service.com/v1/sessions/stop"
   ```

5. **Start the agent:**
   ```bash
   task run
   ```

6. **Connect from client:**
   - The agent will join the Agora RTC channel
   - Your video service will join as UID 12345
   - Subscribe to UID 12345 to see the generated video

## Architecture

### Graph: voice_assistant_generic_video

**Audio Flow:**
```
Agora RTC (user audio) → STT → LLM → TTS → Generic Video Extension
                                              ↓
                                      External Video Service
                                              ↓
                                   Video Published to Agora
```

**Key Connections:**
- `agora_rtc → streamid_adapter → stt`: User audio → transcription
- `stt → main_control → llm → tts`: Speech processing pipeline
- `tts → avatar`: TTS audio routed to generic video extension

## Features

- **Standardized protocol**: Compatible with any service implementing ConvoAI-to-Video
- **Flexible configuration**: Support for multiple video qualities and encodings
- **Production-ready**: Includes session caching, auto-reconnection, WebSocket keepalive
- **Comprehensive error handling**: Detailed error codes and logging
- **Agora integration**: Seamless video publishing to Agora RTC channels

## Implementing a Video Generation Service

See the reference implementation and testing tools:
- https://github.com/AgoraIO-Solutions/convoai_to_video

The repository includes:
- Complete API specification
- Mock server for testing
- Test scripts for session management
- WebSocket audio streaming examples

## Known Limitations

- **Service dependency**: Requires an external video generation service
- **Channel override**: Currently requires backend API update to support dynamic channel names
- **Network latency**: Video quality depends on network conditions between services

## Troubleshooting

### Video not appearing

1. Check your video service is running and accessible
2. Verify API endpoints are correct in property.json
3. Check API key is valid
4. Confirm service is joining the Agora channel (check service logs)
5. Verify client is subscribing to UID 12345

### WebSocket connection failures

1. Check `websocket_address` returned by `/session/start`
2. Verify session token authentication
3. Check network connectivity to service
4. Review WebSocket keepalive settings

### Audio/video desync

- Ensure `input_audio_sample_rate` matches your TTS output
- Check network latency between TEN agent and video service
- Monitor video service processing time

## Documentation

For more details, see:
- `ai/Migrating_heygen_branch_to_main.md` - Complete migration guide
- `ai/AI_working_with_ten.md` - TEN Framework quick reference
- https://github.com/AgoraIO-Solutions/convoai_to_video - Protocol specification
