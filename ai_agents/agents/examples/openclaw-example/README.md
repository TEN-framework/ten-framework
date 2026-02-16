# OpenClaw Voice Assistant

A real-time voice assistant built on TEN Framework that supports STT → LLM → TTS and delegates complex tasks to an external OpenClaw Gateway.

## Features

- **Chained real-time voice interaction**: Deepgram STT → OpenAI LLM → ElevenLabs TTS
- **OpenClaw task delegation**: `claw_task_delegate` tool sends tasks to OpenClaw and receives async replies
- **Raw + narrated results**: OpenClaw result is shown in chat and then narrated by the assistant
- **Graph-based architecture**: configurable with TEN graph and TMAN Designer

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

5. **OpenClaw Gateway**
   - `OPENCLAW_GATEWAY_URL` - Gateway WebSocket endpoint (required for delegation)
   - `OPENCLAW_GATEWAY_TOKEN` - Gateway token (or use password)
   - `OPENCLAW_GATEWAY_ORIGIN` - Origin header expected by gateway origin checks
   - `OPENCLAW_GATEWAY_SCOPES` - Gateway scopes; minimum `operator.write`

### Optional Environment Variables

- `AGORA_APP_CERTIFICATE` - Agora App Certificate (optional)
- `OPENAI_MODEL` - OpenAI model name (optional)
- `OPENAI_PROXY_URL` - Proxy URL for OpenAI API (optional)
- `WEATHERAPI_API_KEY` - Weather API key for weather tool (optional)
- `OPENCLAW_GATEWAY_PASSWORD` - Alternative to token auth (optional)
- `OPENCLAW_GATEWAY_CLIENT_ID` - Defaults to `webchat-ui`
- `OPENCLAW_GATEWAY_CLIENT_MODE` - Defaults to `webchat`
- `OPENCLAW_CHAT_SESSION_KEY` - Defaults to `agent:main:main`

## Setup

### 1. Set Environment Variables

This example loads env from:

- root env: `ai_agents/.env`
- example env: `agents/examples/openclaw-example/tenapp/.env`

Add required variables to your `.env` and OpenClaw-related variables to `tenapp/.env` (or root `.env`).

Example (`tenapp/.env`):

```bash
OPENCLAW_GATEWAY_URL=ws://host.docker.internal:18789
OPENCLAW_GATEWAY_TOKEN=your_gateway_token
OPENCLAW_GATEWAY_ORIGIN=http://host.docker.internal:18789
OPENCLAW_GATEWAY_SCOPES=operator.write
```

### 2. Install Dependencies

```bash
cd agents/examples/openclaw-example
task install
```

This installs tenapp dependencies, Python dependencies, frontend dependencies, and builds the API server.

### 3. Run the Example

```bash
cd agents/examples/openclaw-example
task run
```

### 4. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483

## Configuration

The example graph is configured in `tenapp/property.json`.

Key graph nodes include:

- `agora_rtc`
- `stt` (`deepgram_asr_python`)
- `llm` (`openai_llm2_python`)
- `tts` (`elevenlabs_tts2_python`)
- `main_control` (`main_python`)
- `openclaw_gateway_tool_python`
- `agora_rtm`

OpenClaw flow:

1. `openclaw_gateway_tool_python` registers `claw_task_delegate` to LLM
2. Tool call sends summary to OpenClaw gateway (`chat.send`)
3. Async gateway reply is emitted as `openclaw_reply_event`
4. `main_control` publishes raw OpenClaw result + generates assistant narration

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `AGORA_APP_ID` | string | - | Agora App ID (required) |
| `AGORA_APP_CERTIFICATE` | string | - | Agora App Certificate (optional) |
| `DEEPGRAM_API_KEY` | string | - | Deepgram key (required) |
| `OPENAI_API_KEY` | string | - | OpenAI key (required) |
| `OPENAI_MODEL` | string | - | OpenAI model (optional) |
| `OPENAI_PROXY_URL` | string | - | OpenAI proxy URL (optional) |
| `ELEVENLABS_TTS_KEY` | string | - | ElevenLabs key (required) |
| `WEATHERAPI_API_KEY` | string | - | Weather tool key (optional) |
| `OPENCLAW_GATEWAY_URL` | string | `ws://127.0.0.1:18789` | OpenClaw gateway websocket URL |
| `OPENCLAW_GATEWAY_TOKEN` | string | - | OpenClaw gateway token |
| `OPENCLAW_GATEWAY_PASSWORD` | string | - | OpenClaw gateway password (optional alternative) |
| `OPENCLAW_GATEWAY_ORIGIN` | string | - | Origin header for gateway origin checks |
| `OPENCLAW_GATEWAY_SCOPES` | string | - | Comma-separated scopes; minimum `operator.write` |
| `OPENCLAW_GATEWAY_CLIENT_ID` | string | `webchat-ui` | Gateway client id |
| `OPENCLAW_GATEWAY_CLIENT_MODE` | string | `webchat` | Gateway client mode |
| `OPENCLAW_CHAT_SESSION_KEY` | string | `agent:main:main` | OpenClaw chat session key |

## Customization

You can customize this graph in TMAN Designer (http://localhost:49483):

- Swap STT/LLM/TTS vendors
- Update prompts and tool settings
- Tune OpenClaw gateway properties

For TMAN Designer usage, see the [TMAN Designer documentation](https://theten.ai/docs/ten_agent/customize_agent/tman-designer).

## Release as Docker image

**Note**: Run these commands outside of Docker containers.

### Build image

```bash
cd ai_agents
docker build -f agents/examples/openclaw-example/Dockerfile -t openclaw-example-app .
```

### Run

```bash
docker run --rm -it --env-file .env -p 8080:8080 -p 3000:3000 openclaw-example-app
```

### Access

- Frontend: http://localhost:3000
- API Server: http://localhost:8080

## Learn More

- [TEN Framework Documentation](https://doc.theten.ai)
- [OpenAI API Documentation](https://platform.openai.com/docs)
- [Deepgram API Documentation](https://developers.deepgram.com/)
- [ElevenLabs API Documentation](https://docs.elevenlabs.io/)
