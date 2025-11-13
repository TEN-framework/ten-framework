# Voice Assistant Tavus - Project Structure

This document describes the complete structure of the voice-assistant-tavus agent.

## Directory Structure

```
voice-assistant-tavus/
├── README.md                      # Main documentation
├── Dockerfile                     # Container build configuration
├── Taskfile.yml                   # Build and run automation
├── Taskfile.docker.yml           # Docker-specific tasks
└── tenapp/                        # TEN application directory
    ├── main.go                    # Go application entry point
    ├── go.mod                     # Go module definition
    ├── manifest.json              # App dependencies and metadata
    ├── property.json              # Graph configuration with Tavus
    ├── .tenignore                 # Files to ignore in builds
    ├── bin/                       # Compiled binaries (created by build)
    └── scripts/
        ├── start.sh               # Application startup script
        └── install_python_deps.sh # Python dependencies installer
```

## Key Files

### Configuration Files

1. **tenapp/property.json**
   - Defines the TEN graph with Tavus extension
   - Configures Agora RTC for audio/video streaming
   - Sets up Tavus persona auto-creation
   - Environment variable mapping

2. **tenapp/manifest.json**
   - App metadata (name, version, type)
   - Dependencies:
     - ten_runtime_go (Go runtime)
     - agora_rtc (RTC extension)
     - ten_ai_base (AI base system)
     - tavus_python (Tavus extension - relative path)

3. **go.mod**
   - Go module configuration
   - Links to TEN runtime

### Application Files

4. **tenapp/main.go**
   - Go application entry point
   - Initializes TEN runtime
   - Loads property.json configuration
   - Starts the agent

5. **tenapp/scripts/start.sh**
   - Sets up Python path
   - Sets up library paths
   - Launches the main binary

6. **tenapp/scripts/install_python_deps.sh**
   - Builds Go application
   - Installs Python dependencies from extensions
   - Traverses ten_packages for requirements.txt files

### Build & Automation Files

7. **Taskfile.yml**
   - Development tasks:
     - `task install` - Install all dependencies
     - `task run` - Run agent, frontend, and API server
     - `task release` - Create release build
   - Orchestrates multiple services (tenapp, frontend, API server)

8. **Taskfile.docker.yml**
   - Production Docker tasks
   - Runs API server and frontend in production mode
   - Health check endpoints

9. **Dockerfile**
   - Multi-stage build:
     - Builder stage: Compiles and installs everything
     - Runtime stage: Minimal Ubuntu with runtime deps
   - Copies Tavus extension from ../tavus/ten_packages/
   - Exposes ports 8080 (API) and 3000 (Frontend)

### Documentation

10. **README.md**
    - Complete usage guide
    - Environment variable setup
    - Installation instructions
    - Configuration examples
    - Troubleshooting

## Dependencies

### External Dependencies

The agent depends on these TEN framework components:

- **ten_runtime_go** (v0.11) - Core TEN runtime
- **agora_rtc** (=0.23.9-t1) - Agora RTC extension
- **ten_ai_base** (v0.7) - AI base system
- **tavus_python** - Tavus extension (local path: ../../tavus/ten_packages/extension/tavus_python)
- **websocket_server** - Broadcast Tavus lifecycle events to browsers

### Shared Resources

The agent shares these with other TEN agents:

- **Frontend**: ../../../playground (Next.js web UI)
- **API Server**: ../../../server (Go HTTP API)
- **Extensions**: Can access ../../../ten_packages/extension/*

## Graph Architecture

The agent implements a simple graph with an additional WebSocket broadcast node:

```
┌─────────────┐         ┌──────────────┐
│  Agora RTC  │◄────────┤ Tavus Control│
│             │         │              │
│             │────────►│              │
└─────┬───────┘         └──────┬───────┘
      │                        │
      │                        │
   User Audio/Video      Persona Creation
   User Join/Leave       Conversation Creation
                                │
                                ▼
                        ┌──────────────┐
                        │WebSocket Srv │
                        │(events)      │
                        └──────────────┘
```

### Connections

1. **Agora RTC → Tavus Control**
   - Commands: `on_user_joined`, `on_user_left`
   - Audio frames: `pcm_frame`
   - Video frames: `video_frame`

2. **Tavus Control → Agora RTC**
   - Data messages: `tavus_persona_created`, `tavus_conversation_created`
   - Audio frames: `pcm_frame` (future bridging)
   - Video frames: `video_frame` (future bridging)

3. **Tavus Control → WebSocket Server**
   - Data: `text_data` messages with `data_type: "tavus_event"`
   - Used to broadcast persona/conversation lifecycle events to browsers

## Environment Variables

Required for operation:

```bash
# Agora
AGORA_APP_ID              # RTC app ID
AGORA_APP_CERTIFICATE     # RTC certificate (optional)

# Tavus
TAVUS_API_KEY            # Tavus API key
TAVUS_REPLICA_ID         # Avatar replica ID

# LLM
TAVUS_LLM_PROVIDER       # e.g., "openai", "anthropic"
TAVUS_LLM_MODEL          # e.g., "gpt-4"
TAVUS_LLM_API_KEY        # LLM provider API key
TAVUS_LLM_BASE_URL       # Custom endpoint (optional)

# TTS (optional)
TAVUS_TTS_PROVIDER       # TTS provider
TAVUS_TTS_VOICE_ID       # Voice ID
```

The embedded WebSocket server listens on `ws://localhost:8765` by default; adjust the value in
`property.json` if the port is already in use.

## Build Process

### Development Build (`task install`)

1. Install tenapp dependencies (`tman install`)
   - Downloads system packages (ten_runtime_go, agora_rtc, ten_ai_base)
   - Creates symlinks to shared extensions
   - Links to Tavus extension

2. Build Go application
   - Compiles main.go
   - Links TEN runtime
   - Creates bin/main executable

3. Install Python dependencies
   - Scans ten_packages/extension/*/requirements.txt
   - Scans ten_packages/system/*/requirements.txt
   - Installs via uv pip

4. Install frontend dependencies
   - Runs bun install in playground

5. Build API server
   - Compiles Go API server
   - Creates server/bin/api

### Release Build (`task release`)

1. Runs full install
2. Creates .release/ directory
3. Packages agent for distribution

### Docker Build

1. Builder stage:
   - Copies source files
   - Runs task install
   - Runs task release
   - Builds frontend (bun run build)

2. Runtime stage:
   - Minimal Ubuntu base
   - Installs runtime dependencies
   - Copies built artifacts
   - Sets up entrypoint

## Running the Agent

### Development

```bash
cd agents/examples/voice-assistant-tavus
task install  # First time only
task run      # Start everything
```

This launches:
- TMAN Designer (http://localhost:49483)
- Frontend (http://localhost:3000)
- API Server (http://localhost:8080)

### Production (Docker)

```bash
docker build -t voice-assistant-tavus .
docker run -p 3000:3000 -p 8080:8080 --env-file .env voice-assistant-tavus
```

## Frontend Integration

The agent uses the shared playground frontend at ../../../playground.

### Data Messages

The frontend should listen for these data messages from Agora:

1. **tavus_persona_created**
   - Sent on agent startup
   - Contains: `persona_id`

2. **tavus_conversation_created**
   - Sent when user joins
   - Contains: `conversation_id`, `conversation_url`

### Example Frontend Code

```javascript
agoraRtcEngine.on('streamMessage', (uid, data) => {
  const message = JSON.parse(data);

  if (message.type === 'tavus_conversation_created') {
    // Open Tavus conversation
    window.open(message.conversation_url, '_blank');
  }
});
```

## Customization

### Changing the Persona

Edit `tenapp/property.json`:

```json
{
  "property": {
    "system_prompt": "Your custom prompt here",
    "persona_name": "Your Persona Name",
    "llm_model": "gpt-4o"
  }
}
```

### Adding Extensions

1. Add dependency in `tenapp/manifest.json`:
```json
{
  "path": "../../../ten_packages/extension/your_extension"
}
```

2. Add node in `tenapp/property.json`:
```json
{
  "type": "extension",
  "name": "your_extension",
  "addon": "your_extension",
  "property": {}
}
```

3. Add connections as needed

## Troubleshooting

### Build Issues

- **"tman: command not found"**: Install tman from TEN framework
- **"go: cannot find module"**: Run `go mod tidy` in tenapp/
- **Python import errors**: Run `task install` to install dependencies

### Runtime Issues

- **Agent fails to start**: Check environment variables
- **Persona creation fails**: Verify TAVUS_API_KEY and LLM credentials
- **No conversation URL**: Check logs for Tavus API errors

### Logs

```bash
# View logs
tail -f tenapp/logs/app.log

# Filter for Tavus
tail -f tenapp/logs/app.log | grep -i tavus
```

## Next Steps

1. **Test the agent**: Follow README.md setup instructions
2. **Customize persona**: Edit system_prompt in property.json
3. **Add frontend handling**: Integrate conversation URL display
4. **Enable features**: Turn on perception, recording, etc.

## Support

- **TEN Framework**: https://github.com/TEN-framework/ten-framework
- **Tavus Docs**: https://docs.tavus.io
- **This Agent**: See README.md
