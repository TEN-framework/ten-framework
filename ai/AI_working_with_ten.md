# TEN Framework Development Guide

**Purpose**: Complete onboarding guide for developing with TEN Framework v0.11+
**Last Updated**: 2025-10-28

---

## Table of Contents

1. [Framework Overview](#framework-overview)
2. [Environment Setup](#environment-setup)
3. [Building and Running](#building-and-running)
4. [Creating Extensions](#creating-extensions)
5. [Graph Configuration](#graph-configuration)
6. [Debugging](#debugging)
7. [Remote Access](#remote-access)
8. [Common Issues](#common-issues)

---

## Framework Overview

### What is TEN Framework?

TEN (Temporal Event Network) is a graph-based AI agent framework that connects modular extensions (nodes) through defined data flows (connections). Version 0.11+ uses the `ten_runtime` Python API.

### Key Concepts

**Extensions**: Modular components that process data (e.g., speech-to-text, LLM, TTS, custom analyzers)

**Graphs**: Configurations that define which extensions run and how they connect

**Connections**: Data flows between extensions via:
- `cmd`: Commands (e.g., tool_register, on_user_joined)
- `data`: Data messages (e.g., asr_result, text_data)
- `audio_frame`: PCM audio streams
- `video_frame`: Video streams

**Property Files**: JSON configurations with environment variable substitution:
- `${env:VAR_NAME}` - Required variable (error if missing)
- `${env:VAR_NAME|}` - Optional variable (empty string if missing)

---

## Environment Setup

### Docker Environment

TEN Framework projects typically run in Docker containers for consistency.

**Key Files:**
- `docker-compose.yml` - Container configuration
- `Dockerfile` - Build instructions
- `.env` - Environment variables (mount into container)

**Standard Container Structure:**
```
/app/agents/examples/voice-assistant/
├── .env                    # API keys and config
├── Taskfile.yaml          # Build/run automation
└── tenapp/
    ├── property.json      # Graph definitions
    ├── manifest.json      # App manifest
    └── ten_packages/
        └── extension/     # Custom extensions
```

### API Keys Management

**Best Practice**: Keep API keys in a file OUTSIDE the git repository (e.g., `/home/ubuntu/PERSISTENT_KEYS_CONFIG.md` or `~/api_keys.txt`). This allows you to:
- Switch git branches without losing keys
- Never accidentally commit secrets
- Reference keys when creating new `.env` files

**Create .env file:**
```bash
# Example .env structure
AGORA_APP_ID=your_app_id
AGORA_APP_CERTIFICATE=your_certificate

OPENAI_API_KEY=sk-...
OPENAI_MODEL=gpt-4o-mini

DEEPGRAM_API_KEY=...
ELEVENLABS_TTS_KEY=...

# Custom extension keys
YOUR_API_KEY=...
```

### Environment Variable Loading

**CRITICAL**: Environment variables must be available when the container/process starts.

#### ❌ Does NOT Work:
1. Editing `.env` while services are running
2. Restarting services with `task run` (doesn't reload .env)
3. Expecting variables to propagate automatically

#### ✅ Works:
1. **Restart Docker container** (recommended):
   ```bash
   docker compose down
   docker compose up -d
   ```

2. **Source .env before starting**:
   ```bash
   docker exec -d container_name bash -c '
   set -a
   source /path/to/.env
   set +a
   cd /app/agents/examples/voice-assistant
   task run > /tmp/task_run.log 2>&1
   '
   ```

3. **Hardcode in property.json** (testing only):
   ```json
   {
     "property": {
       "api_key": "actual_value_here"
     }
   }
   ```

---

## Building and Running

### Docker Commands

**Check container status:**
```bash
docker ps | grep your_container_name
```

**Restart container (picks up .env changes):**
```bash
cd /path/to/docker-compose-dir
docker compose down
docker compose up -d
```

**Enter container:**
```bash
docker exec -it container_name bash
```

**View logs:**
```bash
# Container logs
docker logs --tail 100 container_name

# Application logs
docker exec container_name tail -f /tmp/task_run.log
```

### Running Services

**Inside container, start services:**
```bash
cd /app/agents/examples/voice-assistant
task run > /tmp/task_run.log 2>&1 &
```

**Or from outside (detached):**
```bash
docker exec -d container_name bash -c \
  "cd /app/agents/examples/voice-assistant && task run > /tmp/task_run.log 2>&1"
```

### Health Checks

**API server:**
```bash
curl -s http://localhost:8080/health
# Expected: {"code":"0","data":null,"msg":"ok"}
```

**List available graphs:**
```bash
curl -s http://localhost:8080/graphs | jq '.data[].name'
```

**Check ports:**
```bash
# Common ports:
# 8080: API Server
# 3000: Playground Frontend
# Other ports depend on your setup
netstat -tlnp | grep -E ":(8080|3000)"
```

### Installing Python Dependencies

Python dependencies are **NOT persisted** across container restarts. After restarting:

```bash
docker exec container_name pip3 install pydantic aiohttp aiofiles websockets
```

Or create an install script in your extension:
```bash
docker exec container_name bash -c \
  "cd /app/agents/examples/voice-assistant/tenapp && ./scripts/install_python_deps.sh"
```

---

## Creating Extensions

### Extension Directory Structure

```
ten_packages/extension/my_extension_python/
├── __init__.py           # Empty or package init
├── addon.py              # Extension registration
├── extension.py          # Main extension logic
├── manifest.json         # Extension metadata
├── property.json         # Default properties
├── requirements.txt      # Python dependencies
└── README.md             # Documentation
```

### Learning from Existing Extensions

**Before creating a new extension, ALWAYS explore existing similar extensions:**

```bash
# Find all extensions
ls -la /app/agents/ten_packages/extension/
ls -la /app/agents/examples/voice-assistant/tenapp/ten_packages/extension/

# Search for similar patterns
grep -r "AsyncLLMToolBaseExtension" --include="*.py"  # LLM tools
grep -r "on_audio_frame" --include="*.py"             # Audio processing
grep -r "websockets" --include="*.py"                 # WebSocket usage
```

**Study existing property.json files:**

```bash
# Find all graph configurations
find /app/agents -name "property.json" -type f

# Look for similar connection patterns
grep -A 10 "audio_frame" /app/agents/examples/*/tenapp/property.json
grep -A 10 "tool_register" /app/agents/examples/*/tenapp/property.json
```

**Use existing extensions as templates:**
- **weatherapi_tool_python**: Simple LLM tool pattern
- **deepgram_asr_python**: Audio frame processing
- **openai_llm2_python**: LLM integration
- **elevenlabs_tts2_python**: TTS with audio generation
- **agora_rtc**: Real-time communication
- **main_python**: Control flow and coordination

Copy similar extension structure, then modify for your needs.

### Required Files

#### 1. `addon.py`
```python
from ten_runtime import Addon, register_addon_as_extension
from .extension import MyExtension

@register_addon_as_extension("my_extension_python")
class MyExtensionAddon(Addon):
    def on_create_instance(self, ten_env, name, context):
        from ten_runtime import Extension
        return MyExtension(name)
```

#### 2. `extension.py`

**Basic Extension:**
```python
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
    AudioFrame,
)

class MyExtension(AsyncExtension):
    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        # Load properties
        api_key_result = await ten_env.get_property_string("api_key")
        # CRITICAL: Extract value from tuple!
        self.api_key = api_key_result[0] if isinstance(api_key_result, tuple) else api_key_result

        ten_env.log_info("Extension started")
        ten_env.on_start_done()

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("Extension stopped")
        ten_env.on_stop_done()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        # Handle commands
        cmd_name = cmd.get_name()
        ten_env.log_info(f"Received command: {cmd_name}")

        # Send response
        cmd_result = Cmd.create("cmd_result")
        await ten_env.return_result(cmd_result, cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        # Handle data messages
        data_name = data.get_name()
        ten_env.log_info(f"Received data: {data_name}")

    async def on_audio_frame(self, ten_env: AsyncTenEnv, audio_frame: AudioFrame) -> None:
        # Handle audio frames
        pcm_data = audio_frame.get_buf()
        # Process audio...
```

**LLM Tool Extension:**
```python
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension
from ten_ai_base.types import LLMToolMetadata, LLMToolResult

class MyToolExtension(AsyncLLMToolBaseExtension):
    def get_tool_metadata(self, ten_env) -> list[LLMToolMetadata]:
        """Register tools for LLM to call"""
        return [
            LLMToolMetadata(
                name="my_tool",
                description="Tool description for LLM",
                parameters=[
                    {
                        "name": "param1",
                        "type": "string",
                        "description": "Parameter description"
                    }
                ]
            )
        ]

    async def run_tool(self, ten_env, name: str, args: dict) -> LLMToolResult:
        """Called when LLM invokes the tool"""
        ten_env.log_info(f"Tool called: {name} with args: {args}")

        # Process and return result
        return LLMToolResult(
            type="text",
            content="Tool execution result"
        )
```

#### 3. `manifest.json`
```json
{
  "type": "extension",
  "name": "my_extension_python",
  "version": "0.1.0",
  "dependencies": [
    {
      "type": "system",
      "name": "ten_runtime_python",
      "version": "0.11"
    }
  ],
  "api": {
    "property": {
      "api_key": {
        "type": "string"
      },
      "param1": {
        "type": "int64"
      },
      "param2": {
        "type": "float64"
      }
    }
  }
}
```

#### 4. `property.json`
```json
{
  "api_key": "${env:MY_API_KEY|}",
  "param1": 100,
  "param2": 0.5
}
```

### Critical Patterns

#### Property Loading (MUST KNOW!)

**ALL** TEN Framework property getters return tuples `(value, error_or_none)`, NOT just the value:

```python
# ❌ WRONG - Will cause TypeError in comparisons
self.threshold = await ten_env.get_property_float("threshold")
self.count = await ten_env.get_property_int("count")
self.api_key = await ten_env.get_property_string("api_key")

# ✅ CORRECT - Extract first element from tuple
threshold_result = await ten_env.get_property_float("threshold")
self.threshold = threshold_result[0] if isinstance(threshold_result, tuple) else threshold_result

count_result = await ten_env.get_property_int("count")
self.count = count_result[0] if isinstance(count_result, tuple) else count_result

api_key_result = await ten_env.get_property_string("api_key")
self.api_key = api_key_result[0] if isinstance(api_key_result, tuple) else api_key_result
```

**This applies to ALL property types**: `get_property_string()`, `get_property_int()`, `get_property_float()`, `get_property_bool()`

#### Import Statements

TEN Framework v0.11+ uses `ten_runtime`, NOT `ten`:

```python
# ✅ CORRECT (v0.11+)
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
    AudioFrame,
)

# ❌ WRONG (v0.8.x - old API)
from ten import (
    AsyncExtension,
    AsyncTenEnv,
)
```

If you see `ModuleNotFoundError: No module named 'ten'`, change imports to `ten_runtime`.

---

## Graph Configuration

### Adding Extension to Graph

Edit `tenapp/property.json` to add your extension as a node:

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "my_graph",
        "auto_start": false,
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "my_extension",
              "addon": "my_extension_python",
              "extension_group": "default",
              "property": {
                "api_key": "${env:MY_API_KEY|}",
                "param1": 123
              }
            }
          ],
          "connections": [
            // ... connection definitions
          ]
        }
      }
    ]
  }
}
```

### Configuring Connections

Connections define data flow between extensions:

```json
{
  "connections": [
    {
      "extension": "main_control",
      "cmd": [
        {
          "names": ["tool_register"],
          "source": [{"extension": "my_extension"}]
        }
      ],
      "data": [
        {
          "name": "asr_result",
          "source": [{"extension": "stt"}]
        },
        {
          "name": "text_data",
          "source": [{"extension": "my_extension"}]
        }
      ]
    },
    {
      "extension": "agora_rtc",
      "audio_frame": [
        {
          "name": "pcm_frame",
          "dest": [
            {"extension": "stt"},
            {"extension": "my_extension"}
          ]
        }
      ]
    }
  ]
}
```

### Parallel Audio Routing

To send audio to multiple extensions, split at the **source**, not intermediate nodes:

```json
{
  "extension": "agora_rtc",
  "audio_frame": [
    {
      "name": "pcm_frame",
      "dest": [
        {"extension": "streamid_adapter"},
        {"extension": "my_analyzer"}
      ]
    }
  ]
}
```

**Note**: Splitting from intermediate nodes (like `streamid_adapter`) may cause crashes.

---

## Debugging

### Log Locations

**Application logs:**
```bash
/tmp/task_run.log              # Main service logs
```

**Session logs:**
```bash
/tmp/ten_agent/
├── property-{channel}-{timestamp}.json  # Session config
├── app-{channel}-{timestamp}.log         # App logs
└── log-{channel}-{timestamp}.log         # Session logs
```

**Agora logs:**
```bash
/tmp/agoraapi.log              # Connection logs
/tmp/agorasdk.log              # SDK logs
```

### Finding Errors

**Python tracebacks:**
```bash
docker exec container_name tail -200 /tmp/task_run.log | grep -A 20 "Traceback"
```

**Extension errors:**
```bash
docker exec container_name tail -200 /tmp/task_run.log | grep -E "(ERROR|Uncaught exception)"
```

**Check loaded extensions:**
```bash
docker exec container_name tail -200 /tmp/task_run.log | grep "Successfully registered addon"
```

**Session-specific logs:**
```bash
# Find latest session
docker exec container_name ls -lt /tmp/ten_agent/*.json | head -1

# Check session logs
docker exec container_name tail -200 /tmp/task_run.log | grep -E "(channel_name|ERROR)"
```

### Testing Workflow

**1. Health check:**
```bash
curl -s http://localhost:8080/health
```

**2. List graphs:**
```bash
curl -s http://localhost:8080/graphs | jq '.data[].name'
```

**3. Start session:**
```bash
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{
    "graph_name": "my_graph",
    "channel_name": "test_channel",
    "remote_stream_id": 123456
  }'
```

**4. Generate token:**
```bash
curl -X POST http://localhost:8080/token/generate \
  -H "Content-Type: application/json" \
  -d '{
    "channel_name": "test_channel",
    "uid": 0
  }' | jq '.'
```

**5. List active sessions:**
```bash
curl -s http://localhost:8080/list | jq '.'
```

**6. Stop session:**
```bash
curl -X POST http://localhost:8080/stop \
  -H "Content-Type: application/json" \
  -d '{"channel_name": "test_channel"}'
```

### Restarting Services

**Kill services on specific ports:**
```bash
docker exec container_name bash -c "fuser -k 8080/tcp 2>/dev/null"
docker exec container_name bash -c "fuser -k 3001/tcp 2>/dev/null"
```

**Restart services:**
```bash
docker exec -d container_name bash -c \
  "cd /app/agents/examples/voice-assistant && task run > /tmp/task_run.log 2>&1"
```

**Full container restart:**
```bash
cd /path/to/docker-compose-dir
docker compose down
docker compose up -d
```

---

## Remote Access

### Cloudflare Tunnel (HTTPS)

To access the playground frontend remotely via HTTPS:

**1. Kill existing tunnels:**
```bash
pkill cloudflared
```

**2. Start new tunnel:**
```bash
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
```

**3. Get public URL (wait 5-8 seconds for startup):**
```bash
sleep 8
cat /tmp/cloudflare_tunnel.log | grep -o 'https://[^[:space:]]*\.trycloudflare\.com'
```

**URL format**: `https://random-words.trycloudflare.com`

Share this URL to access the playground from anywhere.

**Script for convenience:**
```bash
#!/bin/bash
# Save as setup_cloudflare_tunnel.sh

pkill cloudflared
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
echo "Waiting for tunnel to start..."
sleep 8
URL=$(cat /tmp/cloudflare_tunnel.log | grep -o 'https://[^[:space:]]*\.trycloudflare\.com' | head -1)
echo "Tunnel URL: $URL"
```

---

## Common Issues

### Issue 1: ModuleNotFoundError: No module named 'ten'

**Cause**: Extension using old TEN Framework v0.8.x API
**Solution**: Change imports from `from ten import` to `from ten_runtime import`

```bash
# Find all files with old imports
grep -r "from ten import" --include="*.py"

# Fix with sed
sed -i 's/from ten import/from ten_runtime import/g' file.py
```

### Issue 2: Environment Variables Not Loading

**Symptoms**:
```
Environment variable MY_API_KEY is not found, using default value .
```

**Cause**: Container loaded environment at startup, edits to `.env` not picked up
**Solution**: Restart container to reload environment variables

```bash
cd /path/to/docker-compose-dir
docker compose down
docker compose up -d
```

### Issue 3: TypeError with Property Comparisons

**Symptoms**:
```
TypeError: '>' not supported between instances of 'float' and 'tuple'
TypeError: '<' not supported between instances of 'int' and 'tuple'
```

**Cause**: TEN Framework property getters return tuples `(value, None)`, not just values
**Solution**: Extract first element from tuple

```python
# ❌ WRONG
self.threshold = await ten_env.get_property_float("threshold")
if self.threshold > 0.5:  # TypeError!

# ✅ CORRECT
threshold_result = await ten_env.get_property_float("threshold")
self.threshold = threshold_result[0] if isinstance(threshold_result, tuple) else threshold_result
if self.threshold > 0.5:  # Works!
```

**Applies to ALL property types**: string, int, float, bool

### Issue 4: Port Already in Use

**Symptoms**:
```
[ERROR] listen tcp :8080: bind: address already in use
```

**Solution**: Kill processes on ports

```bash
docker exec container_name bash -c "fuser -k 8080/tcp 2>/dev/null"
docker exec container_name bash -c "fuser -k 3001/tcp 2>/dev/null"
```

### Issue 5: Python Dependencies Missing

**Symptoms**:
```
ModuleNotFoundError: No module named 'aiohttp'
```

**Solution**: Install dependencies (NOT persisted across restarts)

```bash
docker exec container_name pip3 install aiohttp aiofiles pydantic websockets
```

### Issue 6: Agent Server Not Running

**Symptoms**:
- `curl http://localhost:8080/health` returns connection refused
- Only Next.js frontend running (port 3000)

**Cause**: Backend API server stopped or crashed
**Solution**: Start the agent server

```bash
docker exec -d container_name bash -c \
  "cd /app/agents/examples/voice-assistant && task run > /tmp/task_run.log 2>&1"
```

**Verify:**
```bash
curl -s http://localhost:8080/health
# Expected: {"code":"0","data":null,"msg":"ok"}
```

### Issue 7: WebSocket Timeout Errors

**Symptoms**:
```
Error processing frame: sent 1011 (internal error) keepalive ping timeout
```

**Cause**: WebSocket connection missing keepalive configuration
**Solution**: Add ping_interval and ping_timeout

```python
async with websockets.connect(
    url,
    ping_interval=20,
    ping_timeout=10
) as ws:
    # Process frames
```

### Issue 8: Parallel Audio Routing Crashes

**Symptoms**: Worker crashes when trying to route audio to multiple destinations from intermediate node

**Cause**: TEN Framework doesn't support splitting audio from intermediate nodes
**Solution**: Split audio at source (agora_rtc)

```json
{
  "extension": "agora_rtc",
  "audio_frame": [
    {
      "name": "pcm_frame",
      "dest": [
        {"extension": "stt"},
        {"extension": "my_analyzer"}
      ]
    }
  ]
}
```

---

## Quick Troubleshooting Checklist

When something isn't working:

- [ ] Is Docker container running? `docker ps`
- [ ] Are services running inside container? `curl http://localhost:8080/health`
- [ ] Check recent logs: `docker exec container_name tail -50 /tmp/task_run.log`
- [ ] Are environment variables set? Check logs for "Environment variable X is not found"
- [ ] Did you restart container after editing .env?
- [ ] Are ports available? Try killing services and restarting
- [ ] Are Python dependencies installed? `pip3 install ...`
- [ ] Is extension using correct API? Check for `from ten import` (old) vs `from ten_runtime import` (new)
- [ ] Are property values being extracted from tuples?

---

## Best Practices

### Security
- **Never commit API keys** to git
- Store keys in a persistent file outside the repo (e.g., `/home/ubuntu/api_keys.txt`)
- Use environment variable placeholders: `${env:VAR_NAME|}`
- Add `.env` files to `.gitignore`

### Development
- **Study existing extensions first** - Find similar patterns in existing code before writing new extensions
- Explore property.json files for connection examples matching your use case
- Always use `ten_runtime` imports (not `ten`)
- Extract values from property getter tuples
- Test health endpoint before testing graphs
- Check logs frequently during development
- Use descriptive extension and graph names

### Debugging
- Start with simple health checks
- Test individual extensions before complex graphs
- Use `ten_env.log_info()` liberally for debugging
- Check session-specific logs for runtime errors
- Monitor resource usage (memory, CPU) during development

---

## Additional Resources

- [TEN Framework Documentation](https://doc.theten.ai)
- Python dependencies: `aiohttp`, `pydantic`, `websockets`
- Agora RTC: Audio/video streaming
- LLM tool pattern: `AsyncLLMToolBaseExtension`

---

**Pro Tip**: Always check the logs first. Most issues show clear error messages in `/tmp/task_run.log` or session logs.
