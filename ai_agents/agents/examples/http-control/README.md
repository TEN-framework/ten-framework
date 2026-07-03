# HTTP Control Example

A comprehensive voice assistant with HTTP-based control capabilities, featuring real-time conversation via Agora RTC, Deepgram STT, OpenAI LLM, and ElevenLabs TTS, plus dynamic HTTP API integration for programmatic control.

## Features

### Voice Assistant Capabilities
- **Chained Model Real-time Voice Interaction**: Complete voice conversation pipeline with STT → LLM → TTS processing
- **Real-time Communication**: Low-latency audio streaming via Agora RTC
- **Natural Language Processing**: Powered by OpenAI GPT models
- **High-quality Speech Synthesis**: ElevenLabs text-to-speech with natural voices

### HTTP Control Features
- **HTTP Server Integration**: Built-in HTTP server extension for programmatic agent control
- **Dynamic Port Allocation**: Automatic random port assignment (8000-9000) with localStorage persistence
- **Text-based Messaging**: Send commands to the agent via HTTP POST requests
- **Always-visible Input Bar**: Convenient UI for sending messages directly to the agent
- **Proxy Middleware**: Transparent routing of requests to the dynamically allocated port

## How It Works

### Dynamic Port System

1. **Port Initialization**: On first load, the frontend automatically generates a random port number between 8000-9000
2. **Persistence**: The port is stored in localStorage and Redux state for session continuity
3. **Agent Configuration**: When starting the agent, the port is passed as a property override to `http_server_python` extension
4. **Proxy Routing**: Next.js proxy handles requests from `/proxy/{port}/cmd` to `http://localhost:{port}/cmd`

### Message Flow

```
User Input → Frontend (POST /proxy/{port}/cmd) → Proxy → HTTP Server Extension → Agent
```

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
OPENAI_MODEL=gpt-4
OPENAI_PROXY_URL=your_proxy_url_here

# ElevenLabs (required for text-to-speech)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here

# Optional
WEATHERAPI_API_KEY=your_weather_api_key_here
```

### 2. Install Dependencies

```bash
cd agents/examples/http-control
task install
```

This installs Python dependencies and frontend components.

### 3. Run the Application

```bash
cd agents/examples/http-control
task run
```

The application starts with the HTTP server enabled.

### 4. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483
- **HTTP Server Extension**: http://localhost:{random_port} (e.g., 8234)

## Sending Messages

### Via Input Bar

1. Open the frontend at http://localhost:3000
2. Click "Connect" to start the agent
3. Type your message in the input bar at the bottom
4. Click Send or press Enter

### Via HTTP API

You can send messages programmatically using the dynamically allocated port:

```bash
# Replace {port} with your assigned port (check browser localStorage or console)
curl -X POST http://localhost:{port}/cmd \
  -H "Content-Type: application/json" \
  -d '{
    "name": "message",
    "payload": {
      "text": "Hello, agent!"
    }
  }'
```

### Via Frontend Proxy

The frontend provides a proxy endpoint that automatically routes to the correct port:

```javascript
// In your frontend code
await axios.post(`/proxy/${httpPortNumber}/cmd`, {
  name: "message",
  payload: {
    text: "Hello, agent!"
  }
});
```

## Configuration

### Voice Assistant Graph

The voice assistant is configured in `tenapp/property.json` with the following extensions:

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "voice_assistant",
        "auto_start": true,
        "graph": {
          "nodes": [
            {
              "name": "agora_rtc",
              "addon": "agora_rtc",
              "property": {
                "app_id": "${env:AGORA_APP_ID}",
                "app_certificate": "${env:AGORA_APP_CERTIFICATE|}",
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
                  "language": "en-US",
                  "model": "nova-3"
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
            },
            {
              "name": "http_server_python",
              "addon": "http_server_python",
              "property": {
                "listen_port": 8070
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

| Parameter | Extension | Type | Default | Description |
|-----------|-----------|------|---------|-------------|
| `AGORA_APP_ID` | agora_rtc | string | - | Your Agora App ID (required) |
| `AGORA_APP_CERTIFICATE` | agora_rtc | string | - | Your Agora App Certificate (optional) |
| `DEEPGRAM_API_KEY` | deepgram_asr_python | string | - | Deepgram API key (required) |
| `OPENAI_API_KEY` | openai_llm2_python | string | - | OpenAI API key (required) |
| `OPENAI_MODEL` | openai_llm2_python | string | - | OpenAI model name (optional) |
| `OPENAI_PROXY_URL` | openai_llm2_python | string | - | Proxy URL for OpenAI API (optional) |
| `ELEVENLABS_TTS_KEY` | elevenlabs_tts2_python | string | - | ElevenLabs API key (required) |
| `WEATHERAPI_API_KEY` | weatherapi_tool_python | string | - | Weather API key (optional) |

### HTTP Port Number

The `http_port_number` is managed automatically:

- **First Load**: Random port between 8000-9000 is generated
- **Storage**: Saved in `localStorage` under the `__options__` key
- **Redux State**: Available at `state.global.options.http_port_number`
- **Agent Property**: Passed as override when starting the agent

### Agent Properties Override

When the agent starts, the frontend sends:

```json
{
  "request_id": "...",
  "channel_name": "...",
  "user_uid": 123456,
  "graph_name": "...",
  "properties": {
    "http_server_python": {
      "listen_port": 8234
    }
  }
}
```

This overrides the default `listen_port` configured in `tenapp/property.json`.

### Proxy Configuration

The Next.js proxy (`proxy.ts`) handles routing:

```typescript
// Matches /proxy/{port}/path and rewrites to http://localhost:{port}/path
const proxyMatch = pathname.match(/^\/proxy\/(\d+)(\/.*)?$/);
if (proxyMatch && req.method === "POST") {
  const portNumber = proxyMatch[1];
  const path = proxyMatch[2] || "/";
  url.href = `http://localhost:${portNumber}${path}`;
  return NextResponse.rewrite(url);
}
```

## HTTP API Reference

### POST /cmd

Send a command to the agent.

**Endpoint**: `/proxy/{http_port_number}/cmd` (via frontend) or `http://localhost:{http_port_number}/cmd` (direct)

**Method**: `POST`

**Request Body**:
```json
{
  "name": "message",
  "payload": {
    "text": "Your message here"
  }
}
```

**Response**: Depends on agent implementation

**Example**:
```bash
curl -X POST http://localhost:8234/cmd \
  -H "Content-Type: application/json" \
  -d '{"name": "message", "payload": {"text": "Hello"}}'
```

## Frontend Architecture

### Key Components

- **ChatCard.tsx**: Input bar and message display
  - Sends POST requests to `/proxy/{port}/cmd`
  - Displays chat history
  - Error handling with toast notifications

- **Action.tsx**: Agent lifecycle control
  - Start/stop agent with property overrides
  - Passes `http_port_number` to agent

- **proxy.ts**: Request routing
  - Proxies `/proxy/{port}/*` to `http://localhost:{port}/*`
  - Handles only POST requests for security

### State Management

Redux store (`state.global.options`) includes:
```typescript
{
  channel: string;
  userName: string;
  userId: number;
  appId: string;
  token: string;
  http_port_number?: number; // Auto-generated on first load
}
```

## Customization

### Changing Port Range

Edit `src/common/storage.ts`:

```typescript
if (!options.http_port_number) {
  // Change range from 8000-9000 to your desired range
  options.http_port_number = Math.floor(Math.random() * 1000) + 8000;
  localStorage.setItem(OPTIONS_KEY, JSON.stringify(options));
}
```

### Adding Custom Commands

Modify the request in `ChatCard.tsx`:

```typescript
await axios.post(`/proxy/${httpPortNumber}/cmd`, {
  name: "custom_command",  // Change command name
  payload: {
    text: inputValue,
    // Add additional properties
    priority: "high",
    metadata: {...}
  }
});
```

### Visual Designer

Access the TMAN Designer at http://localhost:49483 to visually customize your agent graph and extension properties.

## Release as Docker Image

**Note**: The following commands need to be executed outside of any Docker container.

### Build Image

```bash
cd ai_agents
docker build -f agents/examples/http-control/Dockerfile -t http-control-app .
```

### Run

```bash
docker run --rm -it --env-file .env -p 8080:8080 -p 3000:3000 -p 8000-9000:8000-9000 http-control-app
```

**Note**: The `-p 8000-9000:8000-9000` flag exposes the port range for the HTTP server extension.

### Access

- Frontend: http://localhost:3000
- API Server: http://localhost:8080

## Troubleshooting

### Port Already in Use

If the randomly assigned port is already in use:
1. Clear localStorage in your browser
2. Refresh the page to get a new random port
3. Or manually set a different port in browser DevTools:
   ```javascript
   localStorage.setItem('__options__', JSON.stringify({...options, http_port_number: 8500}))
   ```

### Message Not Sending

- Check browser console for errors
- Verify agent is connected (green status)
- Ensure `http_port_number` is set in localStorage
- Check that the HTTP server extension is running

### Proxy Not Working

- Verify `proxy.ts` exists in the project root (not in `src/`)
- Check Next.js logs for proxy execution
- Ensure the path starts with `/proxy/`

## Learn More

### Voice Assistant Services
- [Agora RTC Documentation](https://docs.agora.io/en/rtc/overview/product-overview)
- [Deepgram API Documentation](https://developers.deepgram.com/)
- [OpenAI API Documentation](https://platform.openai.com/docs)
- [ElevenLabs API Documentation](https://docs.elevenlabs.io/)

### TEN Framework
- [TEN Framework Documentation](https://doc.theten.ai)
- [TMAN Designer Guide](https://theten.ai/docs/ten_agent/customize_agent/tman-designer)
- [Next.js Proxy Documentation](https://nextjs.org/docs/app/building-your-application/routing/proxy)
