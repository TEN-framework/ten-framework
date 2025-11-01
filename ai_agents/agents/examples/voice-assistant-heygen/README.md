# Voice Assistant with HeyGen Avatar

This example demonstrates a voice-assistant with HeyGen AI avatar integration.

## Overview

This agent combines:
- **Speech-to-Text** (Deepgram)
- **LLM** (OpenAI GPT)
- **Text-to-Speech** (ElevenLabs)
- **HeyGen Avatar** - AI-powered avatar that syncs with TTS audio

## How It Works

```
User Speech → STT → LLM → TTS → HeyGen Avatar → Agora RTC (video)
                                    ↓
                              HeyGen Backend joins Agora channel as UID 12345
```

The HeyGen avatar extension:
1. Receives TTS audio frames from ElevenLabs
2. Streams audio to HeyGen backend via WebSocket
3. HeyGen generates avatar video synchronized with the audio
4. HeyGen publishes the avatar video to the Agora RTC channel
5. Users see the AI avatar speaking the generated responses

## Prerequisites

### Required API Keys

Set these in your `.env` file:

```bash
# Agora (for real-time communication)
AGORA_APP_ID=your_agora_app_id
AGORA_APP_CERTIFICATE=  # Optional, leave empty if not using token auth

# HeyGen (for avatar generation)
HEYGEN_API_KEY=your_heygen_api_key

# Speech-to-Text
DEEPGRAM_API_KEY=your_deepgram_api_key

# LLM
OPENAI_API_KEY=your_openai_api_key
OPENAI_MODEL=gpt-4o-mini

# Text-to-Speech
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key
```

### HeyGen Account

Sign up at [HeyGen](https://www.heygen.com/) and obtain your API key.

## Configuration

### Avatar Settings

In `tenapp/property.json`, the avatar node is configured with:

```json
{
  "name": "avatar",
  "addon": "heygen_avatar_python",
  "property": {
    "heygen_api_key": "${env:HEYGEN_API_KEY|}",
    "agora_appid": "${env:AGORA_APP_ID}",
    "agora_appcert": "${env:AGORA_APP_CERTIFICATE|}",
    "channel": "ten_agent_test",
    "agora_avatar_uid": 12345,
    "input_audio_sample_rate": 16000
  }
}
```

### Key Parameters

- **agora_avatar_uid**: The UID that HeyGen will use when joining the Agora channel (default: 12345)
- **input_audio_sample_rate**: Audio sample rate from TTS (16000 for ElevenLabs)
- **channel**: Agora channel name (dynamically overridden by API in production)

## Running the Example

1. **Install dependencies:**
   ```bash
   cd tenapp
   ./scripts/install_python_deps.sh
   ```

2. **Configure environment:**
   ```bash
   cp .env.example .env
   # Edit .env with your API keys
   ```

3. **Start the agent:**
   ```bash
   task run
   ```

4. **Connect from client:**
   - The agent will join the Agora RTC channel
   - HeyGen backend will join as UID 12345
   - Subscribe to UID 12345 to see the avatar video

## Architecture

### Graph: voice_assistant_heygen

**Audio Flow:**
```
Agora RTC (user audio) → STT → LLM → TTS → Avatar Extension
                                              ↓
                                         HeyGen API
                                              ↓
                                    Avatar Video Published to Agora
```

**Key Connections:**
- `agora_rtc → streamid_adapter → stt`: User audio → transcription
- `stt → main_control → llm → tts`: Speech processing pipeline
- `tts → avatar`: TTS audio routed to HeyGen instead of directly to Agora

## Features

- **Real-time avatar generation**: HeyGen generates avatar video synchronized with TTS
- **Seamless integration**: Avatar video published directly to Agora RTC channel
- **WebSocket stability**: Auto-reconnection and keepalive for reliable streaming
- **Session management**: Automatic cleanup of old HeyGen sessions

## Known Limitations

- **Channel override**: Currently requires backend API update to support dynamic channel names. For now, hardcode the channel in property.json for testing.
- **HeyGen API rate limits**: Check HeyGen documentation for usage limits

## Troubleshooting

### Avatar not appearing

1. Check HeyGen API key is valid
2. Verify HeyGen backend is joining the channel (check logs for "session_id")
3. Confirm client is subscribing to UID 12345
4. Check WebSocket connection logs

### Audio/video desync

- Ensure `input_audio_sample_rate` matches your TTS output (16000 for ElevenLabs)
- Check network latency between services

## Documentation

For more details on the migration and architecture, see:
- `ai/Migrating_heygen_branch_to_main.md` - Complete migration guide
- `ai/AI_working_with_ten.md` - TEN Framework quick reference
