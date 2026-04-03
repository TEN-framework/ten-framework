# TEN Framework Quick Reference - voice-assistant-advanced

**Target**: Working with `voice-assistant-advanced` example
**Last Updated**: 2025-11-05

---

## ⚠️ IMPORTANT: Documentation Structure

This is the **QUICK REFERENCE** with essential commands only.

**For detailed information, see:**
- **[AI_working_with_ten.md](./AI_working_with_ten.md)** - Complete reference with all workflows, troubleshooting, extension creation, graph configuration, and production deployment guides.

---

## 1. Environment Setup (.env file)

**IMPORTANT**: Only ONE .env file is used: `/home/ubuntu/ten-framework/ai_agents/.env`

All other .env files (previously under `agents/`, `server/`, etc.) have been removed as redundant.

**Required variables**:
```bash
# Log & Server & Worker (from .env.example)
LOG_PATH=/tmp/ten_agent
LOG_STDOUT=true
GRAPH_DESIGNER_SERVER_PORT=49483
SERVER_PORT=8080
WORKERS_MAX=100
WORKER_QUIT_TIMEOUT_SECONDS=60

# TEN Framework Logging
TEN_LOG_FORMATTER=json
PYTHONUNBUFFERED=1

# Agora RTC
AGORA_APP_ID=your_app_id
AGORA_APP_CERTIFICATE=  # Optional

# API Keys (required for voice assistant)
DEEPGRAM_API_KEY=your_key
OPENAI_API_KEY=your_key
OPENAI_MODEL=gpt-4o
RIME_TTS_API_KEY=your_key
ELEVENLABS_TTS_KEY=your_key
```

**After editing .env**, choose one option:

**Option 1: Restart container** (slower, guaranteed to work):
```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d
```

**Option 2: Source .env and restart server** (faster, no container restart):
```bash
# Stop server
docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'"

# Source .env and restart
docker exec -d ten_agent_dev bash -c \
  "set -a && source /app/.env && set +a && \
   cd /app/server && ./bin/api -tenapp_dir=/app/agents/examples/voice-assistant-advanced/tenapp > /tmp/task_run.log 2>&1"
```

---

## 2. Starting the Container

```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose up -d
docker ps | grep ten_agent_dev  # Verify running
```

---

## 3. Build & Run voice-assistant-advanced

**CRITICAL: ALWAYS use `task run` to start the server, NEVER use `./bin/api` or `./bin/main` directly!**

### First Time or After Code Changes

```bash
# Install Python dependencies (NOT persisted across restarts!)
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

# Build and install (5-8 minutes first time)
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && task install"
```

### Start the Server

**IMPORTANT:** `task run` starts **BOTH** the API server AND the playground together. Do NOT start them separately!

```bash
# Start both API server + playground (use task run, NOT ./bin/main!)
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Wait for full startup (API + playground + TMAN Designer)
sleep 15
```

**What gets started:**
- API Server on port 8080
- Playground on port 3000 (or 3001 if 3000 is busy)
- TMAN Designer on port 49483

### Verify Server is Running

```bash
# Check API server
curl -s http://localhost:8080/health
# Expected: {"code":"0","data":null,"msg":"ok"}

# List available graphs
curl -s http://localhost:8080/graphs | jq -r '.data[].name'
# Expected: voice_assistant, voice_assistant_heygen, etc.
```

---

## 4. Running Playground Client

### ⚠️ CRITICAL: Node.js Version Requirement

**Playground requires Node.js 20.9.0+**

- ✅ Docker container has Node 22
- ❌ Host machine may have older version

**❌ WRONG:**
```bash
# Running from host with Node 18 - FAILS
cd /home/ubuntu/ten-framework/ai_agents/playground
npm run dev  # Error: Node.js version ">=20.9.0" is required
```

**✅ CORRECT:**
```bash
# Run from inside Docker container
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground.log 2>&1"
```

### Automatic Startup (Recommended)

**`task run` starts BOTH API server AND playground:**

```bash
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Verify both services (wait 15s for full startup)
sleep 15
curl -s http://localhost:8080/health  # API server
curl -s http://localhost:3000         # Playground (may use port 3001 if 3000 busy)

# Check actual port in logs if needed
docker exec ten_agent_dev bash -c "grep 'Local:' /tmp/task_run.log | tail -1"
```

### Manual Startup (If Needed)

```bash
# 1. Start API server only
docker exec -d ten_agent_dev bash -c \
  "cd /app/server && \
   ./bin/api -tenapp_dir=/app/agents/examples/voice-assistant-advanced/tenapp > /tmp/api.log 2>&1"

# 2. Start playground separately
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground.log 2>&1"
```

### Production Build

```bash
# Build for production
docker exec ten_agent_dev bash -c "cd /app/playground && npm run build"

# Start production server
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm start > /tmp/playground_prod.log 2>&1"
```

### Verify Playground is Running

```bash
# Check process
docker exec ten_agent_dev bash -c "ps aux | grep -E 'npm.*dev|node.*next' | grep -v grep"

# Check port
docker exec ten_agent_dev bash -c "netstat -tlnp | grep :3000"

# Test HTTP
curl -s -o /dev/null -w '%{http_code}' http://localhost:3000
# Expected: 200
```

### Common Issues

**Playground not accessible:**
```bash
# Start it
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground.log 2>&1"
```

**"502 Bad Gateway":**
```bash
# API server not running - start it
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
```

**"No graphs available":**
```bash
# Hard refresh browser (Ctrl+Shift+R)
# Or restart playground
docker exec ten_agent_dev bash -c "pkill -9 -f 'npm.*dev'"
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground.log 2>&1"
```

> **For complete deployment guide** (nginx setup, production deployment, troubleshooting), see full doc.

---

## 5. Cloudflare Tunnel (HTTPS Access)

### Start Tunnel

```bash
pkill cloudflared
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
sleep 5
```

### Get Tunnel URL

```bash
grep -o 'https://[^[:space:]]*\.trycloudflare\.com' /tmp/cloudflare_tunnel.log | head -1
```

**Example output**: `https://films-colon-msgid-incentives.trycloudflare.com`

**Note**: Free tunnels get random URLs that change on restart

### Nginx (Production HTTPS)

For production with custom domain and SSL:

```nginx
server {
    listen [::]:453 ssl ipv6only=on;
    listen 453 ssl;
    ssl_certificate /etc/letsencrypt/live/oai.agora.io/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/oai.agora.io/privkey.pem;
    include /etc/letsencrypt/options-ssl-nginx.conf;
    ssl_dhparam /etc/letsencrypt/ssl-dhparams.pem;

    # API endpoints
    location ~ ^/(health|ping|token|start|stop|graphs|list)(/|$) {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    # Playground
    location / {
        proxy_pass http://localhost:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

**Apply:** `sudo nginx -t && sudo systemctl reload nginx`

**Access:** `https://oai.agora.io:453/`

---

## 6. Server Architecture Notes

### Dynamic Channel Forwarding

The TEN Framework server automatically injects the `channel_name` from `/start` API requests into **all nodes that have a "channel" property**. This is future-proof - any new extension with a "channel" property will automatically receive the dynamic value without code changes.

**Example**: If you call `/start` with `{"channel_name": "user_123", ...}`, both `agora_rtc` and `heygen_avatar_python` extensions will receive `channel: "user_123"` if they have a "channel" property defined.

**Implementation**:
- `ai_agents/server/internal/http_server.go:646-690` (property-based auto-injection)
- `ai_agents/server/internal/config.go:31-52` (parameter mapping configuration)

### Extension Development - Signal Handlers

**⚠️ NEVER use signal handlers in extensions!**

Signal handlers (`signal.signal()`, `atexit.register()`) only work in main threads. TEN extensions run in worker threads and will fail with: `ValueError: signal only works in main thread`

**Use lifecycle methods instead:**
- `on_start()` - Initialize resources
- `on_stop()` - Cleanup resources (always called before termination)

> **For detailed information** on server architecture and signal handlers, see full doc.

---

## 7. Testing with Users in RTC Channel

### Playground URL
Open tunnel URL in browser (e.g., `https://your-random-name.trycloudflare.com`)

### Testing Flow
1. **Select Graph**: Choose from available graphs (voice_assistant, voice_assistant_heygen, etc.)
2. **Join Channel**: Enter an Agora channel name (e.g., "test123")
3. **Start Session**: Click "Start"
4. **Test**: Speak to the agent

### Monitoring Active Sessions

```bash
# View server logs
docker exec ten_agent_dev tail -f /tmp/task_run.log

# Check active workers/channels
curl -s http://localhost:8080/list | jq
```

### Log Monitoring

**All extension logs appear in `/tmp/task_run.log` with channel prefixes.**

**Log Configuration Requirements:**

**1. In `.env`** - Enable worker stdout (already in section 1):
```bash
LOG_STDOUT=true
TEN_LOG_FORMATTER=json
PYTHONUNBUFFERED=1
```

**2. In `property.json`** - TEN log handlers (required for `ten_env.log_*()` calls):
```json
{
  "ten": {
    "log": {
      "handlers": [
        {
          "matchers": [{"level": "debug"}],
          "formatter": {"type": "plain", "colored": false},
          "emitter": {"type": "console", "config": {"stream": "stdout"}}
        }
      ]
    },
    "predefined_graphs": [...]
  }
}
```

**Log format**: `[channel-name] timestamp level source_file@function:line message`

**How to access logs from HOST machine:**
```bash
# Monitor in real-time from host
/home/ubuntu/ten-framework/ai/monitor_logs.sh

# Copy to host and view
/home/ubuntu/ten-framework/ai/copy_logs.sh
tail -f /tmp/ten_task_run.log
```

**Logs inside container:**

**Real-time monitoring:**
```bash
# All logs
docker exec ten_agent_dev tail -f /tmp/task_run.log

# Filter for specific extension
docker exec ten_agent_dev tail -f /tmp/task_run.log | grep --line-buffered -i "extension_name"

# Filter for channel
docker exec ten_agent_dev tail -f /tmp/task_run.log | grep --line-buffered "channel_name"
```

**View recent logs:**
```bash
# Last 100 lines
docker exec ten_agent_dev tail -100 /tmp/task_run.log

# Search for errors
docker exec ten_agent_dev tail -200 /tmp/task_run.log | grep -a -E "(ERROR|Traceback)"

# Check extension loading
docker exec ten_agent_dev tail -100 /tmp/task_run.log | grep "Successfully registered addon"
```

**Log format:** Extension logs are prefixed with channel name:
```
[channel_name] Successfully registered addon 'my_extension'
[channel_name] Extension log message here
```

### Specific Channel Testing

When a user is in channel "test123":
1. **View logs for that channel**:
   ```bash
   docker exec ten_agent_dev bash -c \
     "grep 'test123' /tmp/task_run.log | tail -50"
   ```

2. **Monitor real-time**:
   ```bash
   docker exec ten_agent_dev bash -c \
     "tail -f /tmp/task_run.log | grep --line-buffered 'test123'"
   ```

3. **Stop specific session**:
   ```bash
   curl -X POST http://localhost:8080/stop \
     -H "Content-Type: application/json" \
     -d '{"channel_name": "test123"}'
   ```

---

## 8. Common Operations

### Full Persistent Startup (Survives Session Closure)

Use this when you want services to keep running after you close your terminal/session:

```bash
# 1. Clean up any existing processes
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"
ps -elf | grep 'bin/main' | grep -v grep | awk '{print $4}' | xargs -r sudo kill -9 2>/dev/null || true

# 2. Remove lock files
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"

# 3. Install Python dependencies
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

# 4. Start everything in detached mode (-d flag keeps it running)
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# 5. Wait and verify
sleep 15
curl -s http://localhost:8080/health && echo " ✓ API ready"
curl -s http://localhost:8080/graphs | jq -r '.data | length' | xargs echo "Graphs:"
curl -s http://localhost:3000 -o /dev/null -w '%{http_code}' | xargs echo "Playground:"
```

**Key points:**
- The `-d` flag with `docker exec` keeps processes running after you disconnect
- `task run` starts API server + playground + TMAN Designer together
- Services persist inside the Docker container until manually stopped
- Logs are in `/tmp/task_run.log` inside container

### After Container Restart

```bash
# 1. Reinstall Python dependencies
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

# 2. Start server
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# 3. Restart tunnel
pkill cloudflared
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
sleep 5 && grep -o 'https://[^[:space:]]*\.trycloudflare\.com' /tmp/cloudflare_tunnel.log | head -1
```

### After Changing property.json

**Server restart: NOT needed!** Property.json is loaded when each new session starts (when user joins a channel).

**Frontend restart: NEEDED if graph list changed!** The playground frontend caches the graph list from `/graphs` API.

**⚠️ RECOMMENDED: Use Nuclear Restart to avoid lock file issues:**

```bash
# Nuclear restart (safest - cleans everything)
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"
sleep 2

sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Wait for startup
sleep 12
curl -s http://localhost:8080/health && echo " ✓ API ready"
curl -s http://localhost:8080/graphs | jq '.data | length' | xargs echo "Graphs available:"
```

**Alternative (manual frontend restart - can cause lock issues):**

```bash
# Only if you know what you're doing
docker exec ten_agent_dev bash -c "pkill -9 -f 'bun.*dev'"
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground.log 2>&1"

# Check which port it started on
docker exec ten_agent_dev tail -10 /tmp/playground.log | grep "Local:"
```

To apply property changes to an existing session, stop and restart that session:
```bash
curl -X POST http://localhost:8080/stop \
  -H "Content-Type: application/json" \
  -d '{"channel_name": "test123"}'
```

### After Changing .env

**CRITICAL: MUST restart container - Option 1 doesn't work reliably!**

```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d

# Reinstall deps and restart server
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
```

### Check Logs

```bash
# Server logs
docker exec ten_agent_dev tail -100 /tmp/task_run.log

# Container logs
docker logs --tail 100 ten_agent_dev

# Follow logs in real-time
docker exec ten_agent_dev tail -f /tmp/task_run.log
```

---

## 9. Troubleshooting

### Zombie Worker Processes (STT/Audio still running after restart)

**Symptom**: Client shows STT transcribing even after server restart, or old sessions appear active

**Cause**: Worker processes (`bin/main`) survive server restart and container restart

**Solution**:
```bash
# Check for zombie workers
ps -elf | grep 'bin/main' | grep -v grep

# Kill all zombie workers
ps -elf | grep 'bin/main' | grep -v grep | awk '{print $4}' | xargs -r sudo kill -9

# Verify they're gone
curl -s http://localhost:8080/list | jq '.data | length'
# Expected: 0
```

**Prevention**: Always kill workers before restarting:
```bash
# Complete cleanup before restart
ps -elf | grep 'bin/main' | grep -v grep | awk '{print $4}' | xargs -r sudo kill -9
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'"
```

### Playground Shows "missing required error components, refreshing..."

**Symptom**: Playground continuously refreshes with error message

**Cause**: Stale/multiple next-server processes or corrupted .next directory

**Solution**:
```bash
# 1. Find and kill all next-server processes
sudo docker exec ten_agent_dev bash -c "ps aux | grep next-server | grep -v grep"
# Note the PIDs, then:
sudo docker exec ten_agent_dev bash -c "kill -9 PID1 PID2 PID3 2>/dev/null; exit 0"

# 2. Clean restart
sleep 3
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# 3. Wait and verify (20+ seconds for .next rebuild)
sleep 20
curl -s http://localhost:8080/health
```

**If that doesn't work** (nuclear option):
```bash
sudo docker exec ten_agent_dev bash -c "killall -9 node bun 2>/dev/null; exit 0"
sudo docker exec ten_agent_dev bash -c "rm -rf /app/playground/.next; rm -f /app/playground/.next/dev/lock"
sleep 3
sudo docker exec -d ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant-advanced && task run > /tmp/task_run.log 2>&1"
sleep 25
```

**Prevention**: Always check for stale processes before starting: `ps aux | grep next-server`

### Server Won't Start
```bash
# Check if Python dependencies are installed
docker exec ten_agent_dev pip3 list | grep -E 'aiohttp|pydantic|websockets'

# Reinstall if missing
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"
```

### API Returns Empty Graphs
```bash
# Check property.json exists
docker exec ten_agent_dev cat \
  /app/agents/examples/voice-assistant-advanced/tenapp/property.json | jq '.predefined_graphs[].name'
```

### Worker Process Crashes (Missing Extension)

**Symptom**: Worker fails with `ModuleNotFoundError: No module named 'ten_packages.extension.xxx'`

**Cause**: Extension used in graph is missing from `manifest.json`

**Solution**:
```bash
# 1. Add extension to manifest.json, then run tman install (creates symlinks automatically)
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && tman install"

# 2. Restart server
docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'"
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && task run > /tmp/task_run.log 2>&1"
```

**Note**: Never manually create symlinks with `ln -s`. Always use `tman install`.

### Tunnel Not Working
```bash
# Check if cloudflared is running
ps aux | grep cloudflared

# Restart tunnel
pkill cloudflared
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
```

### Agent Server Not Running After Container Restart

**Symptom**: Playground shows "502 Bad Gateway" or `/api/agents/graphs` returns empty/error

**Cause**: The agent server (`task run`) is not running inside the container. This happens after:
- Container restart (`docker restart ten_agent_dev`)
- Host machine reboot
- Container crash/stop

**Solution**:
```bash
# 1. Check if server is running
docker exec ten_agent_dev bash -c "ps aux | grep -E 'bin/api|go run' | grep -v grep"

# If nothing shows, start the server:
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# 2. Wait 5-8 seconds for startup, then verify
sleep 5
curl -s http://localhost:8080/health
# Expected: {"code":"0","data":null,"msg":"ok"}

# 3. Check graphs API
curl -s http://localhost:8080/graphs | jq '.graphs[].name'
```

**Note**: The server does NOT auto-start when container starts. You must manually start it after every container restart.

### Playground Node.js Version Issue

**Symptom**: When starting playground from host machine, get error: `Node.js version ">=20.9.0" is required`

**Cause**: Host machine has Node 18.x but playground requires Node 20+

**Solution**: Run playground from INSIDE container which has Node 22:
```bash
# DON'T run from host:
# cd /home/ubuntu/ten-framework/ai_agents/playground && npm run dev  # ❌ Fails with Node 18

# DO run from inside container:
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground_container.log 2>&1"

# Verify it started
sleep 3
docker exec ten_agent_dev bash -c "curl -s -o /dev/null -w '%{http_code}' http://localhost:3000"
# Expected: 200
```

**Note**: The `task run` command in step 3 automatically starts BOTH the agent server AND the playground. So you usually don't need to start playground separately.

---

## 10. Quick Reference Commands

```bash
# Complete restart from scratch
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d
docker exec ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant-advanced/tenapp && bash scripts/install_python_deps.sh"
docker exec -d ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant-advanced && task run > /tmp/task_run.log 2>&1"
sleep 5 && curl -s http://localhost:8080/health

# Start tunnel and get URL
pkill cloudflared
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
sleep 5 && grep -o 'https://[^[:space:]]*\.trycloudflare\.com' /tmp/cloudflare_tunnel.log | head -1
```

---

## 11. When to Restart What (Quick Reference)

| What Changed | Restart Container? | Restart Server? | Restart Frontend? | Notes |
|--------------|-------------------|-----------------|-------------------|-------|
| **property.json** (graphs added/removed) | ❌ No | ⚠️ Use Nuclear Restart | ⚠️ Use Nuclear Restart | Frontend caches graph list. Manual frontend restart can cause lock file issues. **Use Nuclear Restart (see section 9).** |
| **property.json** (config only) | ❌ No | ❌ No | ❌ No | Server loads per session. Existing sessions need reconnect to see changes. |
| **.env file** | ⚠️ Option 1: Yes<br>✅ Option 2: No, source instead | ✅ Yes | ❌ No | Option 1: `docker compose down && up`<br>Option 2: Source .env + restart server (faster) |
| **Python code** | ❌ No | ✅ Yes | ❌ No | Stop and restart server to reload Python extensions |
| **Go code** | ❌ No | ✅ Yes + rebuild | ❌ No | Run `task install` first, then restart server |

**Key Findings**:
- Only ONE .env file is active: `/home/ubuntu/ten-framework/ai_agents/.env`
- Frontend caches `/graphs` API response - use nuclear restart when adding/removing graphs
- Frontend and API server are coupled through `task run` - don't restart frontend alone

---

## 12. Essential Workflows

### Starting the Server
1. Use `task run` to start the server
2. Logs go to `/tmp/task_run.log` inside container
3. Server runs on port 8080

### After Container Restart
1. Reinstall Python dependencies
2. Start server with `task run`
3. Restart cloudflare tunnel

### After Code Changes
1. Kill any zombie processes:
   ```bash
   # Kill log monitors and API server
   sudo pkill -9 -f 'tail.*task_run.log' 2>/dev/null || true
   sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'" 2>/dev/null || true

   # Kill zombie workers (these can persist across restarts)
   ps -elf | grep 'bin/main' | grep -v grep | awk '{print $4}' | xargs -r sudo kill -9 2>/dev/null || true
   ```
2. Run `task install` to rebuild (5-8 minutes first time)
3. Start server with `task run`

### After .env Changes
1. Restart container: `docker compose down && docker compose up -d`
2. Reinstall Python deps
3. Start server

### After property.json Changes
1. Restart server only (no rebuild needed)

### Viewing Logs
- All logs in `/tmp/task_run.log` with `[channel-name]` prefix
- Use `tail -f` for real-time monitoring
- Use `grep` to filter by channel or extension name

### Before Committing
**Always run Black formatter to avoid CI failures:**
```bash
# From ai_agents directory
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents && black --line-length 80 \
  --exclude 'third_party/|http_server_python/|ten_packages/system' \
  ten_packages/extension"
```

> **For full pre-commit checklist and commit message rules**, see full doc.
