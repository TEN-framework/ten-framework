# RTM Transport Example

A voice assistant demonstrating dual-transport architecture using both Agora RTC (for real-time audio) and Agora RTM (Real-Time Messaging for data/text) to enable richer real-time communication capabilities.

## Features

- **Dual Transport Architecture**: Combines Agora RTC (audio) and Agora RTM (messaging/data) for multi-modal communication
- **Stream ID-Based Routing**: Uses stream IDs to route audio from multiple users to separate ASR sessions
- **Message Collection & Chunking**: Collects text messages, chunks them into base64, and sends via both RTC data channel and RTM
- **Complete Voice Pipeline**: STT → LLM → TTS processing with real-time audio streaming
- **Multi-User Support**: Configured for handling multiple remote streams with proper session management

## Architecture Overview

This example demonstrates how to use multiple Agora transports simultaneously:

### Audio Flow (RTC)
```
agora_rtc (incoming audio)
  → streamid_adapter (adds session_id metadata based on stream_id)
    → stt (speech-to-text)
      → llm (language model)
        → tts (text-to-speech)
          → agora_rtc (outgoing audio)
```

### Message Flow (RTM + RTC Data)
```
message_collector2 (receives "message" data)
  → chunks text into base64
    → sends via agora_rtc data channel
    → agora_rtm sends "rtm_message_event" to main_control
```

### Key Extensions

| Extension | Purpose |
|-----------|---------|
| **agora_rtc** | Handles real-time audio streaming (RTC) and data channel |
| **agora_rtm** | Handles real-time messaging (RTM) for low-latency text/data |
| **streamid_adapter** | Converts RTC stream_id to session_id metadata for proper audio routing |
| **message_collector2** | Collects text messages, chunks them, and queues for delivery |
| **main_control** | Orchestrates the agent lifecycle and receives RTM messages |

## Prerequisites

### Required Environment Variables

1. **Agora Account**: Get credentials from [Agora Console](https://console.agora.io/)
   - `AGORA_APP_ID` - Your Agora App ID (required for both RTC and RTM)

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
# Agora (required for audio streaming and messaging)
AGORA_APP_ID=your_agora_app_id_here
AGORA_APP_CERTIFICATE=your_agora_certificate_here

# Deepgram (required for speech-to-text)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (required for language model)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4
OPENAI_PROXY_URL=your_proxy_url_here

# ElevenLabs (required for text-to-speech)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here

# Optional
WEATHERAPI_API_KEY=your_weather_api_key_here
```

### 2. Install Dependencies

```bash
cd agents/examples/rtm-transport
task install
```

This installs Python dependencies and frontend components.

### 3. Run the RTM Transport Example

```bash
cd agents/examples/rtm-transport
task run
```

The application starts with both RTC and RTM capabilities enabled.

### 4. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483

## Configuration

The RTM transport example is configured in `tenapp/property.json`:

### Key Configuration Parameters

#### Agora RTC Configuration
```json
{
  "name": "agora_rtc",
  "addon": "agora_rtc",
  "property": {
    "app_id": "${env:AGORA_APP_ID}",
    "app_certificate": "${env:AGORA_APP_CERTIFICATE|}",
    "channel": "ten_agent_test",
    "stream_id": 1234,          // Local stream ID
    "remote_stream_id": 123,    // Remote stream ID to subscribe
    "subscribe_audio": true,
    "publish_audio": true,
    "publish_data": true,
    "enable_agora_asr": false
  }
}
```

#### Agora RTM Configuration
```json
{
  "name": "agora_rtm",
  "addon": "agora_rtm",
  "property": {
    "channel_type": "message",
    "channel": "ten_agent_test",
    "app_id": "${env:AGORA_APP_ID}",
    "token": "<rtm_token>",      // RTM authentication token
    "user_id": "1234",           // RTM user identifier
    "rtm_enabled": true
  }
}
```

#### Stream ID Adapter
The `streamid_adapter` extension extracts the `stream_id` property from incoming audio frames and converts it to a `session_id` in the metadata. This allows multiple users (with different stream IDs) to have separate ASR sessions.

```json
{
  "name": "streamid_adapter",
  "addon": "streamid_adapter",
  "property": {}
}
```

#### Message Collector
The `message_collector2` extension receives text messages via the "message" data input, chunks them into base64-encoded pieces, and sends them via the RTC data channel with a 40ms interval between chunks.

```json
{
  "name": "message_collector",
  "addon": "message_collector2",
  "property": {}
}
```

### Configuration Parameters Reference

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `AGORA_APP_ID` | string | - | Your Agora App ID (required for both RTC and RTM) |
| `AGORA_APP_CERTIFICATE` | string | - | Your Agora App Certificate (optional) |
| `stream_id` | int | 1234 | Local stream ID for published audio |
| `remote_stream_id` | int | 123 | Remote stream ID to subscribe to |
| `rtm_enabled` | bool | true | Enable/disable RTM functionality |
| `user_id` | string | "1234" | RTM user identifier |
| `DEEPGRAM_API_KEY` | string | - | Deepgram API key (required) |
| `OPENAI_API_KEY` | string | - | OpenAI API key (required) |
| `OPENAI_MODEL` | string | - | OpenAI model name (optional) |
| `OPENAI_PROXY_URL` | string | - | Proxy URL for OpenAI API (optional) |
| `ELEVENLABS_TTS_KEY` | string | - | ElevenLabs API key (required) |
| `WEATHERAPI_API_KEY` | string | - | Weather API key (optional) |

## Customization

The RTM transport example uses a modular design that allows you to easily replace STT, LLM, or TTS modules with other providers using TMAN Designer.

Access the visual designer at http://localhost:49483 to customize your voice agent. For detailed usage instructions, see the [TMAN Designer documentation](https://theten.ai/docs/ten_agent/customize_agent/tman-designer).

## Use Cases

This dual-transport architecture is ideal for:

- **Multi-user voice assistants**: Use stream IDs to route audio from multiple users to separate processing sessions
- **Chat + Voice applications**: Combine text messaging (RTM) with voice communication (RTC)
- **Live streaming with interaction**: Send low-latency messages/commands while streaming audio
- **Collaborative tools**: Support both voice chat and text messaging in real-time
- **Gaming voice chat**: Use RTM for game state/commands while RTC handles voice

## Release as Docker Image

**Note**: The following commands need to be executed outside of any Docker container.

### Build Image

```bash
cd ai_agents
docker build -f agents/examples/rtm-transport/Dockerfile -t rtm-transport-app .
```

### Run

```bash
docker run --rm -it --env-file .env -p 8080:8080 -p 3000:3000 rtm-transport-app
```

### Access

- Frontend: http://localhost:3000
- API Server: http://localhost:8080

## Learn More

- [Agora RTC Documentation](https://docs.agora.io/en/rtc/overview/product-overview)
- [Agora RTM Documentation](https://docs.agora.io/en/Real-time-Messaging/product_rtm)
- [Deepgram API Documentation](https://developers.deepgram.com/)
- [OpenAI API Documentation](https://platform.openai.com/docs)
- [ElevenLabs API Documentation](https://docs.elevenlabs.io/)
- [TEN Framework Documentation](https://doc.theten.ai)
