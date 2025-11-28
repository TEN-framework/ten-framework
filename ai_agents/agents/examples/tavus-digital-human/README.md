# Tavus Digital Human

A digital human conversation interface powered by Tavus AI, integrated with TEN framework.

> **Note on RTC**: This example uses Daily.co WebRTC due to Tavus platform limitations. Tavus only supports Daily.co and LiveKit as RTC transports and does not currently support Agora RTC. See [ARCHITECTURE_ANALYSIS.md](ARCHITECTURE_ANALYSIS.md) for detailed technical analysis and potential future integration paths.

## Features

- **AI Digital Human**: Realistic video avatar with voice interaction
- **Real-time Conversation**: Powered by Tavus Conversational Video Interface
- **WebRTC Integration**: Uses Daily.co (Tavus platform requirement)
- **Simple Setup**: Easy-to-use API for creating and managing conversations

## Prerequisites

### Required Environment Variables

1. **Tavus API Key**: Get your API key from [Tavus Dashboard](https://platform.tavus.io/)
   - `TAVUS_API_KEY` - Your Tavus API key (required)
2. **Tavus Replica ID** (optional): Select a specific digital human persona to use
   - `TAVUS_REPLICA_ID` - Replica ID for the Tavus digital human (optional; Tavus can use its default if omitted)

### Optional Environment Variables

- `TAVUS_PERSONA_ID` - Custom Tavus persona ID (optional, uses default if not provided)

## Setup

### Option 1: Docker (Recommended)

#### Development Mode

1. **Set Environment Variables**

   Add to `.env` file in the project root (`/path/to/ten-framework/.env`):

   ```bash
   # Tavus Configuration
   TAVUS_API_KEY=your_tavus_api_key_here
   TAVUS_REPLICA_ID=your_tavus_replica_id_here  # optional
   TAVUS_PERSONA_ID=  # Optional: leave empty to use default persona

   # Required by TEN framework
   AGORA_APP_ID=00000000000000000000000000000000
   WORKERS_MAX=10
   WORKER_QUIT_TIMEOUT_SECONDS=60
   SERVER_PORT=8080
   LOG_PATH=./logs
   GRAPH_DESIGNER_SERVER_PORT=49483
   ```

2. **Start Docker Container**

   From the project root:

   ```bash
   cd /path/to/ten-framework/ai_agents
   docker-compose up -d
   ```

3. **Enter Container and Run**

   ```bash
   docker exec -it ten_agent_dev bash
   cd agents/examples/tavus-digital-human
   task install
   task run
   ```

4. **Access the Demo**

   Open your browser:
   - Frontend: http://localhost:3000
   - API Server: http://localhost:8080

#### Production Build

Build a standalone Docker image:

```bash
cd /path/to/ten-framework/ai_agents

# Build image
docker build \
  --build-arg USE_AGENT=agents/examples/tavus-digital-human \
  -f agents/examples/tavus-digital-human/Dockerfile \
  -t tavus-digital-human .

# Run container
docker run -p 8080:8080 -p 3000:3000 \
  -e TAVUS_API_KEY=your_key_here \
  tavus-digital-human
```

### Option 2: Local Development

1. **Set Environment Variables**

   Add to `.env` file in project root:

   ```bash
   # Tavus Configuration
   TAVUS_API_KEY=your_tavus_api_key_here
   TAVUS_REPLICA_ID=your_tavus_replica_id_here  # optional
   TAVUS_PERSONA_ID=  # Optional

   # Required by server
   AGORA_APP_ID=00000000000000000000000000000000
   WORKERS_MAX=10
   WORKER_QUIT_TIMEOUT_SECONDS=60
   SERVER_PORT=8080
   LOG_PATH=./logs
   ```

2. **Install Dependencies**

   ```bash
   cd agents/examples/tavus-digital-human
   task install
   ```

3. **Run the Application**

   ```bash
   task run
   ```

4. **Access the Demo**

   Open your browser and navigate to:
   ```
   http://localhost:3000
   ```

Click "Start Conversation" to begin interacting with the AI digital human!

## How It Works

### Architecture

```
Frontend (Daily.js)
   ↓
   ├─→ TEN API (/api/tavus/conversation/create)
   │      ↓
   │   TEN Extension (tavus_conversation_manager_python)
   │      ↓
   │   Tavus API
   │      ↓
   └─→ Daily.co WebRTC Room ←→ Tavus Digital Human
```

### Flow

1. **Create Conversation**: Frontend calls TEN API to create a Tavus conversation
2. **Get URL**: TEN extension calls Tavus API and returns a Daily.co room URL
3. **Join Room**: Frontend uses Daily.js SDK to join the WebRTC room
4. **Interact**: User talks to the AI digital human via video/voice
5. **End Conversation**: User can end the conversation anytime

## Configuration

### Customizing the Digital Human

You can customize the conversation by modifying `tenapp/property.json`:

```json
{
  "tavus_api_key": "${env:TAVUS_API_KEY}",
  "persona_id": "${env:TAVUS_PERSONA_ID|}",
  "default_greeting": "Your custom greeting here"
}
```

### Creating a Custom Persona

1. Go to [Tavus Dashboard](https://platform.tavus.io/)
2. Create a new persona with custom appearance and voice
3. Copy the persona ID
4. Set `TAVUS_PERSONA_ID` in your `.env` file

## Troubleshooting

### "Failed to create conversation"

- Check that `TAVUS_API_KEY` is set correctly in `.env`
- Verify your Tavus API key is valid
- Check API server logs for detailed error messages

### Video not loading

- Ensure you're using a supported browser (Chrome, Firefox, Safari)
- Check your internet connection
- Verify Daily.co is not blocked by firewall

### Extension not found

- Run `task install` to ensure all dependencies are installed
- Check that the extension symlink is created correctly

### Library Loading or Thread Lock Errors

**If you see errors like:**
```
libten_runtime_go.so: cannot open shared object file
ten_rwlock_lock Invalid argument
qemu: uncaught target signal 6 (Aborted)
```

**For Docker Desktop on macOS**: This is a **QEMU incompatibility issue**. Docker Desktop uses QEMU for Linux emulation, which doesn't properly support TEN Framework's custom spinlock implementation.

**Solution**: Use **Colima** or **OrbStack** instead (native virtualization):
```bash
# Install Colima
brew install colima

# Start Colima
colima start --arch x86_64 --memory 8

# Configure Docker to use Colima
export DOCKER_HOST=unix://$HOME/.colima/docker.sock
```

Or run natively on Linux / WSL2.

**For native LD_LIBRARY_PATH issues**:
```bash
export LD_LIBRARY_PATH=$(pwd)/tenapp/ten_packages/system/ten_runtime_go/lib:$LD_LIBRARY_PATH
```

## API Reference

### POST /api/tavus/conversation/create

Creates a new Tavus conversation.

**Response:**
```json
{
  "conversation_id": "conv_123abc",
  "conversation_url": "https://tavus.daily.co/room123"
}
```

### DELETE /api/tavus/conversation/:id

Ends a Tavus conversation.

## Extension Usage

The `tavus_conversation_manager_python` extension can be used in other TEN applications:

```json
{
  "name": "tavus_manager",
  "addon": "tavus_conversation_manager_python",
  "property": {
    "tavus_api_key": "${env:TAVUS_API_KEY}",
    "persona_id": "${env:TAVUS_PERSONA_ID|}",
    "default_greeting": "Hello!"
  }
}
```

Send commands:
- `create_conversation` - Creates a new conversation
- `end_conversation` - Ends an existing conversation

## Resources

- [Tavus Documentation](https://docs.tavus.io/)
- [Daily.js SDK](https://docs.daily.co/reference/daily-js)
- [TEN Framework](https://github.com/TEN-framework/ten-framework)

## License

Same as TEN Framework
