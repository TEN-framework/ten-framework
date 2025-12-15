# TEN Framework Development Guide

**Purpose**: Complete reference for developing with TEN Framework v0.11+
**Last Updated**: 2025-12-12

---

## Documentation Structure

- **AI_working_with_ten_compact.md** - Quick reference with critical rules, copy-paste commands
- **AI_working_with_ten.md** (this file) - Detailed explanations, troubleshooting, extension development

---

## Table of Contents

1. [Framework Overview](#framework-overview)
2. [Environment Setup](#environment-setup)
3. [Building and Running](#building-and-running)
4. [Server Architecture](#server-architecture)
5. [Creating Extensions](#creating-extensions)
6. [Graph Configuration](#graph-configuration)
7. [Debugging and Logs](#debugging-and-logs)
8. [Remote Access](#remote-access)
9. [Troubleshooting](#troubleshooting)

---

## Framework Overview

### What is TEN Framework?

TEN (Transformative Extensions Network) is a graph-based AI agent framework that connects modular extensions (nodes) through defined data flows (connections). Version 0.11+ uses the `ten_runtime` Python API.

### Key Concepts

**Extensions**: Modular components that process data (speech-to-text, LLM, TTS, custom analyzers)

**Graphs**: Configurations defining which extensions run and how they connect

**Connections**: Data flows between extensions:
- `cmd`: Commands (tool_register, on_user_joined)
- `data`: Data messages (asr_result, text_data)
- `audio_frame`: PCM audio streams
- `video_frame`: Video streams

**Property Files**: JSON configurations with environment variable substitution:
- `${env:VAR_NAME}` - Required variable (error if missing)
- `${env:VAR_NAME|}` - Optional variable (empty string if missing)

### Repository Structure

```
ai_agents/
├── .env                          # ONLY env file used
├── agents/examples/
│   └── voice-assistant-advanced/
│       ├── Taskfile.yaml         # Build/run automation
│       └── tenapp/
│           ├── property.json     # Graph definitions
│           ├── rebuild_property.py  # Generates property.json
│           └── manifest.json     # Extension dependencies
├── agents/ten_packages/extension/ # Extension source code
├── playground/                   # Next.js frontend UI
└── server/                       # Go API server
```

---

## Environment Setup

### Single .env File

**Only ONE .env file is used**: `/home/ubuntu/ten-framework/ai_agents/.env`

This is loaded by `docker-compose.yml`. All other .env files have been removed.

### Required Environment Variables

```bash
# Log & Server & Worker
LOG_PATH=/tmp/ten_agent
LOG_STDOUT=true
SERVER_PORT=8080
WORKERS_MAX=100

# Agora RTC
AGORA_APP_ID=your_app_id
AGORA_APP_CERTIFICATE=  # Optional

# API Keys (depends on extensions used)
DEEPGRAM_API_KEY=your_key
OPENAI_API_KEY=your_key
ELEVENLABS_TTS_KEY=your_key
ANAM_API_KEY=your_key
```

### API Keys Best Practice

Store keys outside the git repository (e.g., `/home/ubuntu/PERSISTENT_KEYS_CONFIG.md`) to:
- Switch branches without losing keys
- Never accidentally commit secrets

---

## Building and Running

### First Time Setup

```bash
# Enter container
docker exec -it ten_agent_dev bash

# Build and install (5-8 minutes first time)
cd /app/agents/examples/voice-assistant-advanced
task install
```

### After Container Restart

Python dependencies don't persist across container restarts:

```bash
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"
```

### Starting the Server

**ALWAYS use `task run`** - never `./bin/api` or `./bin/main` directly:

```bash
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
```

`task run` starts:
- API Server on port 8080
- Playground on port 3000
- TMAN Designer on port 49483

### Why `task run`?

- Sets PYTHONPATH correctly for ten_runtime and ten_ai_base
- Direct `./bin/main` will fail with Python import errors
- Task commands are documented in Taskfile.yaml

---

## Server Architecture

### Dynamic Property Injection

The Go server (`server/internal/http_server.go`) auto-injects request parameters into graph nodes.

**Channel Name Auto-Injection**: When `/start` is called with `channel_name`, the server injects this value into **all nodes with a "channel" property**.

Example: Both `agora_rtc` and `heygen_avatar_python` extensions receive the dynamic channel value automatically.

**Other injected parameters**:
- `RemoteStreamId` → `agora_rtc.remote_stream_id`
- `BotStreamId` → `agora_rtc.stream_id`
- `Token` → `agora_rtc.token` and `agora_rtm.token`

### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/health` | GET | Health check |
| `/graphs` | GET | List available graphs |
| `/start` | POST | Start agent session |
| `/stop` | POST | Stop agent session |
| `/list` | GET | List active sessions |
| `/token/generate` | POST | Generate Agora RTC token |

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
└── requirements.txt      # Python dependencies
```

### Required Files

#### addon.py
```python
from ten_runtime import Addon, register_addon_as_extension
from .extension import MyExtension

@register_addon_as_extension("my_extension_python")
class MyExtensionAddon(Addon):
    def on_create_instance(self, ten_env, name, context):
        return MyExtension(name)
```

#### extension.py (Basic)
```python
from ten_runtime import AsyncExtension, AsyncTenEnv, Cmd, Data, AudioFrame

class MyExtension(AsyncExtension):
    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        # CRITICAL: Property getters return tuples (value, error)
        api_key_result = await ten_env.get_property_string("api_key")
        self.api_key = api_key_result[0] if isinstance(api_key_result, tuple) else api_key_result

        ten_env.log_info("Extension started")
        ten_env.on_start_done()

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        # Cleanup resources here
        ten_env.log_info("Extension stopped")
        ten_env.on_stop_done()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"Received command: {cmd_name}")
        cmd_result = Cmd.create("cmd_result")
        await ten_env.return_result(cmd_result, cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"Received data: {data_name}")

    async def on_audio_frame(self, ten_env: AsyncTenEnv, audio_frame: AudioFrame) -> None:
        pcm_data = audio_frame.get_buf()
        # Process audio...
```

#### extension.py (LLM Tool)
```python
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension
from ten_ai_base.types import LLMToolMetadata, LLMToolResult

class MyToolExtension(AsyncLLMToolBaseExtension):
    def get_tool_metadata(self, ten_env) -> list[LLMToolMetadata]:
        return [
            LLMToolMetadata(
                name="my_tool",
                description="Tool description for LLM",
                parameters=[
                    {"name": "param1", "type": "string", "description": "Parameter description"}
                ]
            )
        ]

    async def run_tool(self, ten_env, name: str, args: dict) -> LLMToolResult:
        ten_env.log_info(f"Tool called: {name} with args: {args}")
        return LLMToolResult(type="text", content="Tool result")
```

### Critical Patterns

#### NEVER Use Signal Handlers

Signal handlers only work in the main thread. TEN extensions run in worker threads.

```python
# ❌ WRONG - Will crash with ValueError
signal.signal(signal.SIGTERM, self._cleanup)
atexit.register(self._emergency_cleanup)

# ✅ CORRECT - Use lifecycle methods
async def on_stop(self, ten_env):
    # Cleanup here - always called before termination
    if self.websocket:
        await self.websocket.close()
```

#### Property Loading Returns Tuples

ALL property getters return `(value, error_or_none)`:

```python
# ❌ WRONG - Will cause TypeError in comparisons
self.threshold = await ten_env.get_property_float("threshold")

# ✅ CORRECT - Extract first element
threshold_result = await ten_env.get_property_float("threshold")
self.threshold = threshold_result[0] if isinstance(threshold_result, tuple) else threshold_result
```

#### Use ten_runtime, not ten

```python
# ✅ CORRECT (v0.11+)
from ten_runtime import AsyncExtension, AsyncTenEnv

# ❌ WRONG (v0.8.x - old API)
from ten import AsyncExtension
```

---

## Graph Configuration

### Adding Extension to Graph

Edit `tenapp/property.json`:

```json
{
  "ten": {
    "predefined_graphs": [{
      "name": "my_graph",
      "auto_start": false,
      "graph": {
        "nodes": [{
          "type": "extension",
          "name": "my_extension",
          "addon": "my_extension_python",
          "extension_group": "default",
          "property": {
            "api_key": "${env:MY_API_KEY|}"
          }
        }],
        "connections": [...]
      }
    }]
  }
}
```

### Connection Types

```json
{
  "connections": [
    {
      "extension": "main_control",
      "cmd": [{"names": ["tool_register"], "source": [{"extension": "my_extension"}]}],
      "data": [{"name": "asr_result", "source": [{"extension": "stt"}]}]
    },
    {
      "extension": "agora_rtc",
      "audio_frame": [{"name": "pcm_frame", "dest": [{"extension": "stt"}, {"extension": "analyzer"}]}]
    }
  ]
}
```

### IMPORTANT: Keep property.json and rebuild_property.py In Sync

When modifying graphs:
1. Update `rebuild_property.py` first
2. Run the script to regenerate `property.json`
3. Do nuclear restart

Or if editing property.json directly, also update rebuild_property.py to avoid overwriting on next script run.

---

## Debugging and Logs

### Log Locations

| Log | Location |
|-----|----------|
| All extension logs | `/tmp/task_run.log` |
| Session configs | `/tmp/ten_agent/property-{channel}-{timestamp}.json` |
| Agora logs | `/tmp/agoraapi.log`, `/tmp/agorasdk.log` |

### Log Monitoring

```bash
# Real-time all logs
docker exec ten_agent_dev tail -f /tmp/task_run.log

# Filter by channel
docker exec ten_agent_dev tail -f /tmp/task_run.log | grep --line-buffered "channel_name"

# Filter errors
docker exec ten_agent_dev tail -200 /tmp/task_run.log | grep -E "(ERROR|Traceback)"
```

### Log Configuration

In `property.json`:
```json
{
  "ten": {
    "log": {
      "handlers": [{
        "matchers": [{"level": "debug"}],
        "formatter": {"type": "plain", "colored": false},
        "emitter": {"type": "console", "config": {"stream": "stdout"}}
      }]
    }
  }
}
```

Without this, `ten_env.log_*()` calls are silent.

### Testing Workflow

```bash
# 1. Health check
curl -s http://localhost:8080/health

# 2. List graphs
curl -s http://localhost:8080/graphs | jq '.data[].name'

# 3. Start session
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{"graph_name": "my_graph", "channel_name": "test", "remote_stream_id": 123}'

# 4. List active sessions
curl -s http://localhost:8080/list | jq '.'

# 5. Stop session
curl -X POST http://localhost:8080/stop \
  -H "Content-Type: application/json" \
  -d '{"channel_name": "test"}'
```

---

## Remote Access

### Cloudflare Tunnel (Quick HTTPS)

```bash
pkill cloudflared
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
sleep 5
grep -o 'https://[^[:space:]]*\.trycloudflare\.com' /tmp/cloudflare_tunnel.log | head -1
```

### Nginx Reverse Proxy (Production)

```nginx
server {
    listen 443 ssl;
    server_name your-domain.com;

    ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;

    # API endpoints
    location ~ ^/(health|ping|token|start|stop|graphs|list)(/|$) {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    # Playground
    location / {
        proxy_pass http://localhost:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

---

## Troubleshooting

### Nuclear Restart (Use First!)

When in doubt, nuclear restart:

```bash
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"
sleep 2
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
sleep 12
curl -s http://localhost:8080/health
```

### Common Issues

#### "No graphs available" in playground
Frontend cached empty response. Nuclear restart.

#### "502 Bad Gateway"
API server not running. Start with `task run`.

#### ModuleNotFoundError
Reinstall Python deps after container restart.

#### Environment variable not found
Restart container after .env changes.

#### Lock file error
Clear lock: `rm -f /app/playground/.next/dev/lock`, then nuclear restart.

#### Zombie workers persist after restart
Workers run on host, not in container:
```bash
ps -elf | grep 'bin/main' | grep -v grep | awk '{print $4}' | xargs -r sudo kill -9
```

### Pre-commit Checks

```bash
# Format Python code (required for CI)
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents && black --line-length 80 \
  --exclude 'third_party/|http_server_python/|ten_packages/system' \
  ten_packages/extension"
```

### Pre-commit Hook Setup

Create `.git/hooks/pre-commit` to auto-check formatting and prevent API key commits:

```bash
cat > /home/ubuntu/ten-framework/.git/hooks/pre-commit << 'HOOK'
#!/bin/bash
# Pre-commit hook: check for API keys and black formatting

# Check for API keys in staged files
if git diff --cached --name-only | xargs grep -l -E "(API_KEY|api_key).*=.*[A-Za-z0-9]{20,}" 2>/dev/null; then
    echo "ERROR: Potential API key found in staged files!"
    exit 1
fi

# Check black formatting for Python extension files
staged_py_files=$(git diff --cached --name-only --diff-filter=ACM | grep -E "^ai_agents/agents/ten_packages/extension/.*\.py$" | grep -v "third_party/\|http_server_python/\|ten_packages/system")

if [ -n "$staged_py_files" ]; then
    if command -v docker &> /dev/null && sudo docker ps -q -f name=ten_agent_dev &> /dev/null; then
        unformatted=$(echo "$staged_py_files" | xargs -I {} sudo docker exec ten_agent_dev bash -c "cd /app && black --check --line-length 80 {} 2>&1" 2>/dev/null | grep "would reformat" || true)
    elif command -v black &> /dev/null; then
        unformatted=$(echo "$staged_py_files" | xargs black --check --line-length 80 2>&1 | grep "would reformat" || true)
    else
        echo "WARNING: black not available, skipping format check"
        echo "To format: sudo docker exec ten_agent_dev bash -c 'cd /app/agents && black --line-length 80 --exclude \"third_party/|http_server_python/|ten_packages/system\" ten_packages/extension'"
        unformatted=""
    fi

    if [ -n "$unformatted" ]; then
        echo "ERROR: Python files need black formatting!"
        echo "$unformatted"
        echo "Run: sudo docker exec ten_agent_dev bash -c 'cd /app && black --line-length 80 agents/ten_packages/extension'"
        exit 1
    fi
fi
exit 0
HOOK
chmod +x /home/ubuntu/ten-framework/.git/hooks/pre-commit
```

### Commit Message Rules

- Subject must be lowercase
- Valid types: build, chore, ci, docs, feat, fix, perf, refactor, revert, style, test
- Body lines ≤100 characters

```bash
# Example
fix: correct import statements in heygen extension
```

---

## Additional Resources

- [TEN Framework Documentation](https://doc.theten.ai)
- Base classes: `agents/ten_packages/system/ten_ai_base/interface/ten_ai_base/`
- API interfaces: `agents/ten_packages/system/ten_ai_base/api/*.json`

---

**Pro Tip**: Always check `/tmp/task_run.log` first. Most issues show clear error messages there.
