# Voice Assistant Advanced

Advanced voice assistant configurations featuring avatar integration (HeyGen, Generic Video) and mental wellness analysis (Thymia) with Deepgram Flux STT for improved turn detection.

## Features

This example includes three advanced voice assistant graphs:

### 1. **voice_assistant_thymia** (Auto-start)
Mental wellness voice assistant with Thymia analysis capabilities:
- **STT**: Deepgram Flux (v2) with built-in turn detection (~260ms)
- **LLM**: OpenAI with mental wellness prompts
- **TTS**: ElevenLabs
- **Analysis**: Thymia mental wellness analyzer

### 2. **voice_assistant_heygen**
Voice assistant with HeyGen avatar video integration:
- **STT**: Deepgram ASR
- **LLM**: OpenAI
- **TTS**: ElevenLabs
- **Avatar**: HeyGen streaming avatar

### 3. **voice_assistant_generic_video**
Voice assistant with generic video avatar integration:
- **STT**: Deepgram ASR
- **LLM**: OpenAI
- **TTS**: ElevenLabs
- **Avatar**: Generic video stream

## Prerequisites

### Required Environment Variables

1. **Agora Account**: Get credentials from [Agora Console](https://console.agora.io/)
   - `AGORA_APP_ID` - Your Agora App ID (required)

2. **Deepgram Account**: Get credentials from [Deepgram Console](https://console.deepgram.com/)
   - `DEEPGRAM_API_KEY` - Your Deepgram API key (required)

3. **OpenAI Account**: Get credentials from [OpenAI Platform](https://platform.openai.com/)
   - `OPENAI_API_KEY` - Your OpenAI API key (required)

4. **ElevenLabs Account**: Get credentials from [ElevenLabs](https://elevenlabs.io/)
   - `ELEVENLABS_TTS_KEY` - Your ElevenLabs API key (required)

### Optional Environment Variables (for specific graphs)

- `AGORA_APP_CERTIFICATE` - Agora App Certificate (optional)
- `HEYGEN_API_KEY` - HeyGen API key (for HeyGen avatar graph)
- `GENERIC_VIDEO_API_KEY` - Generic Video API key (for generic video graph)

## Setup

### 1. Set Environment Variables

Add to your `.env` file:

```bash
# Agora (required for audio/video streaming)
AGORA_APP_ID=your_agora_app_id_here
AGORA_APP_CERTIFICATE=your_agora_certificate_here

# Deepgram (required for speech-to-text)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (required for language model)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o-mini

# ElevenLabs (required for text-to-speech)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here

# HeyGen (optional - for HeyGen avatar)
HEYGEN_API_KEY=your_heygen_api_key_here

# Generic Video (optional - for generic video avatar)
GENERIC_VIDEO_API_KEY=your_generic_video_api_key_here
```

### 2. Install Dependencies

```bash
cd agents/examples/voice-assistant-advanced
task install
```

This installs Python dependencies and frontend components.

### 3. Run the Voice Assistant

```bash
cd agents/examples/voice-assistant-advanced
task run
```

The `voice_assistant_thymia` graph starts automatically (auto_start: true).

### 4. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483

## Graph Selection

By default, the `voice_assistant_thymia` graph starts automatically. To use a different graph:

1. Access TMAN Designer at http://localhost:49483
2. Select the desired graph:
   - `voice_assistant_thymia` - Mental wellness analysis with Flux
   - `voice_assistant_heygen` - HeyGen avatar integration
   - `voice_assistant_generic_video` - Generic video avatar

Or modify `auto_start` settings in `tenapp/property.json`.

## Deepgram Flux (v2 API)

The Thymia graph uses Deepgram's Flux model with built-in turn detection:

```json
{
  "addon": "deepgram_ws_asr_python",
  "property": {
    "params": {
      "url": "wss://api.deepgram.com/v2/listen",
      "model": "flux-general-en",
      "eot_threshold": 0.7,
      "eot_timeout_ms": 3000
    }
  }
}
```

**Flux Features:**
- Built-in turn detection (~260ms latency)
- EndOfTurn/StartOfTurn events
- Progressive transcript refinement
- No external VAD required

## Configuration

All graphs are configured in `tenapp/property.json`. Each graph can be customized via TMAN Designer at http://localhost:49483.

## Customization

The advanced voice assistant uses a modular design that allows you to easily replace STT, LLM, TTS, or Avatar modules with other providers using TMAN Designer.

For detailed usage instructions, see the [TMAN Designer documentation](https://theten.ai/docs/ten_agent/customize_agent/tman-designer).

## Release as Docker image

**Note**: The following commands need to be executed outside of any Docker container.

### Build image

```bash
cd ai_agents
docker build -f agents/examples/voice-assistant-advanced/Dockerfile -t voice-assistant-advanced-app .
```

### Run

```bash
docker run --rm -it --env-file .env -p 8080:8080 -p 3000:3000 voice-assistant-advanced-app
```

### Access

- Frontend: http://localhost:3000
- API Server: http://localhost:8080

## Learn More

- [Agora RTC Documentation](https://docs.agora.io/en/rtc/overview/product-overview)
- [Deepgram API Documentation](https://developers.deepgram.com/)
- [Deepgram Flux Documentation](https://developers.deepgram.com/docs/flux)
- [OpenAI API Documentation](https://platform.openai.com/docs)
- [ElevenLabs API Documentation](https://docs.elevenlabs.io/)
- [HeyGen API Documentation](https://docs.heygen.com/)
- [TEN Framework Documentation](https://doc.theten.ai)
