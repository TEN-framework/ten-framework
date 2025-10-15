# Live2D Voice Assistant

A voice assistant with **Live2D character integration** and real-time conversation capabilities using Agora RTC, Deepgram STT, OpenAI LLM, and ElevenLabs TTS. This example features animated Live2D characters that respond to audio input with synchronized mouth movements and expressions, providing an engaging and interactive user experience.

> **Note**: This example shares the same backend configuration as the [voice-assistant](../voice-assistant/) example but includes an enhanced frontend with Live2D character support.

## Features

- **Live2D Character Integration**: Interactive Live2D models with audio synchronization and mouth movement
- **Chained Model Real-time Voice Interaction**: Complete voice conversation pipeline with STT → LLM → TTS processing
- **Real-time Voice Communication**: Powered by Agora RTC

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

### Optional Environment Variables

- `AGORA_APP_CERTIFICATE` - Agora App Certificate (optional)
- `OPENAI_MODEL` - OpenAI model name (optional, defaults to configured model)
- `OPENAI_PROXY_URL` - Proxy URL for OpenAI API (optional)
- `WEATHERAPI_API_KEY` - Weather API key for weather tool (optional)

## Setup

### 1. Set Environment Variables

Add to your `.env` file:

```bash
# Agora (required for audio streaming)
AGORA_APP_ID=your_agora_app_id_here
AGORA_APP_CERTIFICATE=your_agora_certificate_here

# Deepgram (required for speech-to-text)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (required for language model)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# ElevenLabs (required for text-to-speech)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here

# Optional
OPENAI_PROXY_URL=your_proxy_url_here
WEATHERAPI_API_KEY=your_weather_api_key_here
```

### 2. Install Dependencies

```bash
cd agents/examples/voice-assistant-live2d
task install
```

This installs Python dependencies and frontend components.

### 3. Run the Voice Assistant

```bash
cd agents/examples/voice-assistant-live2d
task run
```

The voice assistant starts with all capabilities enabled.

### 4. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483

## Live2D Models

The application includes pre-configured Live2D models:
- **Kei Vowels Pro** - Multi-language character with voice sync
- Models are located in `frontend/public/models/` and can be easily swapped or extended

### Live2D Character Customization

The Live2D frontend supports custom character models:
- Replace models in `frontend/public/models/` directory
- Supported formats: `.model3.json`, `.moc3`, `.physics3.json`
- Configure character settings in the frontend components

## Configuration

The voice assistant is configured in `tenapp/property.json`:

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "voice_assistant_live2d",
        "auto_start": true,
        "graph": {
          "nodes": [
            {
              "name": "agora_rtc",
              "addon": "agora_rtc",
              "property": {
                "app_id": "${env:AGORA_APP_ID}",
                "app_certificate": "${env:AGORA_APP_CERTIFICATE|}",
                "channel": "ten_agent_test",
                "subscribe_audio": true,
                "publish_audio": true,
                "publish_data": true
              }
            },
            {
              "name": "stt",
              "addon": "deepgram_asr_python",
              "property": {
                "params": {
                  "api_key": "${env:DEEPGRAM_API_KEY}",
                  "language": "en-US"
                }
              }
            },
            {
              "name": "llm",
              "addon": "openai_llm2_python",
              "property": {
                "api_key": "${env:OPENAI_API_KEY}",
                "model": "${env:OPENAI_MODEL}",
                "max_tokens": 512,
                "greeting": "TEN Agent connected. How can I help you today?"
              }
            },
            {
              "name": "tts",
              "addon": "elevenlabs_tts2_python",
              "property": {
                "params": {
                  "key": "${env:ELEVENLABS_TTS_KEY}",
                  "model_id": "eleven_multilingual_v2",
                  "voice_id": "pNInz6obpgDQGcFmaJgB",
                  "output_format": "pcm_16000"
                }
              }
            }
          ]
        }
      }
    ]
  }
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `AGORA_APP_ID` | string | - | Your Agora App ID (required) |
| `AGORA_APP_CERTIFICATE` | string | - | Your Agora App Certificate (optional) |
| `DEEPGRAM_API_KEY` | string | - | Deepgram API key (required) |
| `OPENAI_API_KEY` | string | - | OpenAI API key (required) |
| `OPENAI_MODEL` | string | - | OpenAI model name (optional) |
| `OPENAI_PROXY_URL` | string | - | Proxy URL for OpenAI API (optional) |
| `ELEVENLABS_TTS_KEY` | string | - | ElevenLabs API key (required) |
| `WEATHERAPI_API_KEY` | string | - | Weather API key (optional) |

## Customization

The voice assistant uses a modular design that allows you to easily replace STT, LLM, or TTS modules with other providers using TMAN Designer.

Access the visual designer at http://localhost:49483 to customize your voice agent. For detailed usage instructions, see the [TMAN Designer documentation](https://theten.ai/docs/ten_agent/customize_agent/tman-designer).

## Release as Docker image

**Note**: The following commands need to be executed outside of any Docker container.

### Build image

```bash
cd ai_agents
docker build -f agents/examples/voice-assistant-live2d/Dockerfile -t voice-assistant-live2d .
```

### Run

```bash
docker run --rm -it --env-file .env -p 8080:8080 -p 3000:3000 voice-assistant-live2d
```

### Access

- Frontend: http://localhost:3000
- API Server: http://localhost:8080

## Learn More

- [Agora RTC Documentation](https://docs.agora.io/en/rtc/overview/product-overview)
- [Deepgram API Documentation](https://developers.deepgram.com/)
- [OpenAI API Documentation](https://platform.openai.com/docs)
- [ElevenLabs API Documentation](https://docs.elevenlabs.io/)
- [Live2D Documentation](https://docs.live2d.com/)
- [TEN Framework Documentation](https://doc.theten.ai)
