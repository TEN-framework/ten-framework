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
   - `OPENCLAW_GATEWAY_TOKEN` or `OPENCLAW_GATEWAY_PASSWORD` - Provide at least one for gateway auth

### Optional Environment Variables

- `AGORA_APP_CERTIFICATE` - Agora App Certificate (optional)
- `OPENAI_MODEL` - OpenAI model name (optional)
- `OPENAI_PROXY_URL` - Proxy URL for OpenAI API (optional)
- `OPENCLAW_GATEWAY_URL` - Defaults in `.env.example`
- `OPENCLAW_GATEWAY_ORIGIN` - Defaults in `.env.example`
- `OPENCLAW_GATEWAY_SCOPES` - Defaults in `.env.example` (`operator.write`)
- `OPENCLAW_GATEWAY_DEVICE_IDENTITY_PATH` - Defaults in `.env.example`; ensure the path is writable
- `OPENCLAW_GATEWAY_CLIENT_ID` - Defaults to `openclaw-control-ui`
- `OPENCLAW_GATEWAY_CLIENT_MODE` - Defaults to `ui`
- `OPENCLAW_CHAT_SESSION_KEY` - Defaults to `agent:main:main`

## Setup

### 1. Set Environment Variables

This example loads env from:

- root env: `ai_agents/.env`
- example env: `agents/examples/openclaw-example/tenapp/.env`

Recommended: use `tenapp/.env.example` as the template and fill in real values.

```bash
cd agents/examples/openclaw-example/tenapp
cp .env.example .env
```

Then update required keys in `tenapp/.env` (or place them in root `.env` if you prefer a shared env file).

Example (`tenapp/.env`):

```bash
OPENCLAW_GATEWAY_URL=ws://host.docker.internal:18789
OPENCLAW_GATEWAY_TOKEN=your_gateway_token
OPENCLAW_GATEWAY_ORIGIN=http://host.docker.internal:18789
OPENCLAW_GATEWAY_SCOPES=operator.write
OPENCLAW_GATEWAY_DEVICE_IDENTITY_PATH=/data/openclaw/device_identity.json
```

Notes:
- `OPENCLAW_GATEWAY_DEVICE_IDENTITY_PATH` must be writable.
- Keep either `OPENCLAW_GATEWAY_TOKEN` or `OPENCLAW_GATEWAY_PASSWORD` configured for initial auth/pairing.
- `OPENCLAW_GATEWAY_ORIGIN` must be a valid HTTP origin (`http(s)://host[:port]`) and must exactly match an entry in gateway `controlUi.allowedOrigins`.
  - In OpenClaw UI, go to **Settings -> Gateway -> Control UI Allowed Origins** and add the exact value of `OPENCLAW_GATEWAY_ORIGIN`.
  - Example default in this project: `http://host.docker.internal:18789`.
  - Use `https://...` (not `wss://...`), and do not include a path.
  - If `OPENCLAW_GATEWAY_ORIGIN` and `allowedOrigins` do not match exactly, gateway will reject with `origin not allowed`.

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

Pairing flow:

1. On startup, the OpenClaw extension performs gateway handshake using signed device identity.
2. If gateway requires pairing approval, frontend opens a blocking dialog with approve command.
3. User copies the command and runs it on gateway host:
   - `openclaw devices list`
   - `openclaw devices approve <requestId>` (or `openclaw devices approve --latest`)
4. Retry the request after approval.

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
| `OPENCLAW_GATEWAY_URL` | string | `ws://127.0.0.1:18789` | OpenClaw gateway websocket URL |
| `OPENCLAW_GATEWAY_TOKEN` | string | - | OpenClaw gateway token |
| `OPENCLAW_GATEWAY_PASSWORD` | string | - | OpenClaw gateway password (optional alternative) |
| `OPENCLAW_GATEWAY_ORIGIN` | string | - | Origin header for gateway origin checks |
| `OPENCLAW_GATEWAY_SCOPES` | string | - | Comma-separated scopes; minimum `operator.write` |
| `OPENCLAW_GATEWAY_CLIENT_ID` | string | `openclaw-control-ui` | Gateway client id |
| `OPENCLAW_GATEWAY_CLIENT_MODE` | string | `ui` | Gateway client mode |
| `OPENCLAW_GATEWAY_DEVICE_IDENTITY_PATH` | string | `~/.openclaw/identity/device.json` | Path to persisted device identity file |
| `OPENCLAW_CHAT_SESSION_KEY` | string | `agent:main:main` | OpenClaw chat session key |

## Pairing Dialog

- When pairing approval is required, frontend opens a dedicated dialog immediately.
- The dialog shows the exact approval command and includes one-click copy.
- Run the copied command on the gateway host, then retry the request.

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
