# AI Working with TEN Framework - Quick Reference

**Last Updated**: 2025-10-28
**Purpose**: Quick onboarding guide for AI assistants working on TEN Framework projects

---

## Environment Overview

**Server**: AWS EC2 (eu-west-2)
**IP**: 18.132.202.170
**Container**: ten_agent_dev
**Framework**: TEN Framework v0.11
**Current Branch**: avatar-integration

### Key Ports
- 8080: API Server
- 3000: Playground Frontend
- 9000/9001: TMAN Designer
- 49483: Cloudflare tunnel

### API Keys Location
See `/home/ubuntu/PERSISTENT_KEYS_CONFIG.md` for all API keys and configuration.

---

## Docker Environment Management

### Quick Commands

**Check Container Status:**
```bash
docker ps | grep ten_agent_dev
```

**Restart Container (Picks up .env changes):**
```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down
docker compose up -d
```

**Restart Services Inside Container:**
```bash
docker exec ten_agent_dev bash -c "fuser -k 8080/tcp 2>/dev/null; fuser -k 49483/tcp 2>/dev/null; fuser -k 3001/tcp 2>/dev/null"
docker exec -d ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant && task run > /tmp/task_run.log 2>&1"
```

**View Logs:**
```bash
# Container logs
docker logs --tail 100 ten_agent_dev

# Task logs
docker exec ten_agent_dev tail -f /tmp/task_run.log

# Session logs
docker exec ten_agent_dev ls -lt /tmp/ten_agent/*.log | head
```

**Enter Container:**
```bash
docker exec -it ten_agent_dev bash
```

---

## Environment Variable Configuration

### CRITICAL: How Environment Variables Work

Environment variables in TEN Framework can be loaded in several ways, but **they must be available when the process starts**:

#### ❌ Does NOT Work:
1. Editing `.env` files while services are running - container already loaded env vars at startup
2. Restarting services with `task run` - doesn't reload environment from .env files
3. Expecting environment variables to propagate automatically

#### ✅ Works:
1. **Restart Docker Container** (recommended for development):
   ```bash
   cd /home/ubuntu/ten-framework/ai_agents
   docker compose down
   docker compose up -d
   # Wait for container to start
   docker exec -d ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant && task run > /tmp/task_run.log 2>&1"
   ```

2. **Source .env before starting services**:
   ```bash
   docker exec -d ten_agent_dev bash -c '
   set -a
   source /app/agents/examples/voice-assistant/.env
   set +a
   cd /app/agents/examples/voice-assistant
   task run > /tmp/task_run.log 2>&1
   '
   ```

3. **Hardcode values in property.json** (for testing):
   ```json
   {
     "addon": "heygen_avatar_python",
     "property": {
       "heygen_api_key": "actual_key_value",
       "agora_appid": "actual_app_id"
     }
   }
   ```

### .env File Locations

**Host:**
- `/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant/.env`

**Container:**
- `/app/agents/examples/voice-assistant/.env` (primary)
- `/app/agents/examples/voice-assistant/tenapp/.env` (copy for redundancy)
- `/app/agents/.env` (Taskfile dotenv location)

### Property File Location

**Graph definitions with env var placeholders:**
- `/app/agents/examples/voice-assistant/tenapp/property.json`

This is where graph structures are defined. Environment variables use syntax:
- `${env:VAR_NAME}` - Required, error if missing
- `${env:VAR_NAME|}` - Optional, empty string if missing

---

## Testing Workflow

### 1. Health Check
```bash
curl -s http://localhost:8080/health
# Expected: {"code":"0","data":null,"msg":"ok"}
```

### 2. List Available Graphs
```bash
curl -s http://localhost:8080/graphs | jq '.'
```

### 3. Start a Session
```bash
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{
    "graph_name": "voice_assistant_with_avatar",
    "channel_name": "test_session",
    "remote_stream_id": 123456,
    "language": "en-US"
  }'
```

### 4. Generate Token for Client
```bash
curl -X POST http://localhost:8080/token/generate \
  -H "Content-Type: application/json" \
  -d '{
    "channel_name": "test_session",
    "uid": 0
  }' | jq '.'
```

### 5. List Active Sessions
```bash
curl -s http://localhost:8080/list | jq '.'
```

### 6. Stop Session
```bash
curl -X POST http://localhost:8080/stop \
  -H "Content-Type: application/json" \
  -d '{
    "channel_name": "test_session"
  }'
```

### 7. Check Session Logs
```bash
# Find session property file
docker exec ten_agent_dev ls -lt /tmp/ten_agent/property-*.json | head -3

# Check session logs
docker exec ten_agent_dev tail -200 /tmp/task_run.log | grep -E "(test_session|ERROR|error)"
```

---

## Cloudflare Tunnel Setup

For accessing the playground frontend from outside:

```bash
# Kill existing tunnels
pkill cloudflared

# Start new tunnel
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &

# Get URL (wait 5-8 seconds for tunnel to start)
sleep 8
cat /tmp/cloudflare_tunnel.log | grep -o 'https://[^[:space:]]*\.trycloudflare\.com'
```

**URL Format**: `https://random-words-here.trycloudflare.com`

Share this URL with users to access the playground.

---

## Common Issues and Solutions

### Issue 1: Extension Fails with "ModuleNotFoundError: No module named 'ten'"

**Cause**: Extension using old TEN Framework v0.8.x API
**Solution**: Change imports from `from ten import` to `from ten_runtime import`

```bash
# Find all files with old imports
grep -r "from ten import" --include="*.py"

# Fix manually or with sed
sed -i 's/from ten import/from ten_runtime import/g' file.py
```

### Issue 2: Environment Variables Not Loading

**Symptoms**:
```
Environment variable HEYGEN_API_KEY is not found, using default value .
```

**Cause**: Container loaded environment at startup, edits to .env not picked up
**Solution**: Restart container (see "Environment Variable Configuration" above)

### Issue 3: WebSocket Timeout Errors

**Symptoms**:
```
Error processing audio frame: sent 1011 (internal error) keepalive ping timeout
```

**Cause**: WebSocket connection missing keepalive configuration
**Solution**: Add ping_interval and ping_timeout to websockets.connect():
```python
async with websockets.connect(
    url,
    ping_interval=20,
    ping_timeout=10
) as ws:
```

### Issue 4: Agora RTC Mutex Errors

**Symptoms**:
```
terminating with uncaught exception of type std::__1::system_error:
mutex lock failed: Invalid argument
```

**Status**: Identified as specific to ben-dev branch modifications, NOT present in clean avatar-integration
**Solution**: Use avatar-integration branch (tested and confirmed working)

### Issue 5: Port Already in Use

**Symptoms**:
```
[ERROR] listen tcp :8080: bind: address already in use
```

**Solution**: Kill processes on ports:
```bash
docker exec ten_agent_dev bash -c "fuser -k 8080/tcp 2>/dev/null"
docker exec ten_agent_dev bash -c "fuser -k 3001/tcp 2>/dev/null"
docker exec ten_agent_dev bash -c "fuser -k 49483/tcp 2>/dev/null"
```

### Issue 6: Python Dependencies Missing

**Symptoms**:
```
ModuleNotFoundError: No module named 'aiohttp'
```

**Solution**: Install dependencies:
```bash
docker exec ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant/tenapp && ./scripts/install_python_deps.sh"
```

---

## Project Structure

### Key Directories (inside container)

```
/app/agents/examples/voice-assistant/
├── .env                          # API keys
├── Taskfile.yaml                 # Task automation
└── tenapp/
    ├── .env                      # Duplicate of parent .env
    ├── property.json             # Graph definitions
    ├── manifest.json             # App manifest
    ├── scripts/
    │   └── install_python_deps.sh
    └── ten_packages/
        ├── extension/
        │   ├── heygen_avatar_python/
        │   ├── generic_video_python/
        │   ├── agora_rtc/
        │   ├── deepgram_asr_python/
        │   ├── elevenlabs_tts2_python/
        │   └── openai_llm2_python/
        └── system/
            └── ten_ai_base/
```

### Log Locations

```
/tmp/
├── task_run.log              # Service logs
├── agoraapi.log              # Agora RTC connection logs
├── agorasdk.log              # Agora SDK logs
└── ten_agent/
    ├── property-{channel}-{timestamp}.json
    ├── app-{channel}-{timestamp}.log
    └── log-{channel}-{timestamp}.log (if created)
```

---

## Available Graphs

### voice_assistant (Standard)
- STT: Deepgram
- LLM: OpenAI GPT-4
- TTS: ElevenLabs
- RTC: Agora

### voice_assistant_with_avatar (HeyGen)
- All of above +
- Avatar: HeyGen API (real-time avatar video)
- Publishes avatar video to Agora channel as UID 12345

### voice_assistant_with_generic_avatar (Generic Video)
- All of standard +
- Avatar: Generic Video Protocol (custom video server)

---

## Quick Troubleshooting Checklist

When something isn't working:

- [ ] Is Docker container running? `docker ps | grep ten_agent_dev`
- [ ] Are services running inside container? `curl http://localhost:8080/health`
- [ ] Check recent logs: `docker exec ten_agent_dev tail -50 /tmp/task_run.log`
- [ ] Are environment variables set? Check logs for "Environment variable X is not found"
- [ ] Did you restart container after editing .env? `docker compose down && docker compose up -d`
- [ ] Are ports available? Try killing services and restarting
- [ ] Are Python dependencies installed? Run install_python_deps.sh
- [ ] Is the extension using correct API? Check for `from ten import` (old) vs `from ten_runtime import` (new)

---

## Extension Development Tips

### Testing New Extensions

1. Add extension to `ten_packages/extension/`
2. Add to property.json graph nodes
3. Add connections in property.json
4. Restart container or services
5. Check logs for initialization errors
6. Test with `/start` API call

### Common Extension Patterns

**Receiving Audio:**
```python
async def on_audio_frame(self, ten_env: AsyncTenEnv, audio_frame: AudioFrame) -> None:
    frame_buf = audio_frame.get_buf()
    # Process audio...
```

**Sending Data:**
```python
data = Data.create("data_name")
data.set_property_string("text", "hello")
await ten_env.send_data(data)
```

**Lifecycle:**
```python
async def on_start(self, ten_env: AsyncTenEnv) -> None:
    # Initialize extension
    ten_env.on_start_done()

async def on_stop(self, ten_env: AsyncTenEnv) -> None:
    # Cleanup
    ten_env.on_stop_done()
```

---

## Performance Notes

- Fresh Docker container takes ~10 seconds to fully start
- Service restart (task run) takes ~5-8 seconds
- First API call may be slower due to extension initialization
- Session startup takes 2-3 seconds (Agora RTC connection)
- HeyGen session creation takes 1-2 seconds

---

## Useful Commands Reference

```bash
# Quick health check
curl -s http://localhost:8080/health && echo " ✓ API healthy"

# Watch logs in real-time
docker exec ten_agent_dev tail -f /tmp/task_run.log

# Find latest session
docker exec ten_agent_dev ls -lt /tmp/ten_agent/*.json | head -1

# Check active Agora sessions
docker exec ten_agent_dev grep "onConnected" /tmp/agoraapi.log | tail -5

# Search logs for errors
docker exec ten_agent_dev grep -i error /tmp/task_run.log | tail -20

# Check Python extension imports
docker exec ten_agent_dev bash -c 'grep -r "from ten import" /app/agents/examples/voice-assistant/tenapp/ten_packages/extension/ --include="*.py"'
```

---

**Pro Tip**: Always check the logs first. Most issues show clear error messages in `/tmp/task_run.log` or the extension's log output.
