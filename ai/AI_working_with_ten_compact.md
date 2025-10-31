# TEN Framework Quick Reference - voice-assistant-advanced

**Target**: Working with `voice-assistant-advanced` example
**Last Updated**: 2025-10-31

> **For comprehensive documentation**, see [AI_working_with_ten.md](./AI_working_with_ten.md)

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

```bash
# Start server (use task run, NOT ./bin/main!)
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
```

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

## 4. Cloudflare Tunnel (HTTPS Access)

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

## 5. Testing with Users in RTC Channel

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

## 6. Common Operations

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

```bash
# If you added/removed graphs, restart frontend to clear cache
docker exec ten_agent_dev bash -c "pkill -9 -f 'bun.*dev'"
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/playground && \
   bun run dev > /tmp/playground.log 2>&1"

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

**Option 1: Source .env and restart server** (Faster - No container restart):
```bash
# Stop current server
docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'"

# Source .env and restart server (MUST be in one command!)
docker exec -d ten_agent_dev bash -c \
  "set -a && source /app/.env && set +a && \
   cd /app/server && ./bin/api -tenapp_dir=/app/agents/examples/voice-assistant-advanced/tenapp > /tmp/task_run.log 2>&1"
```

**Option 2: Restart container** (Guaranteed to work, but slower):
```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d

# Then reinstall deps and restart server (see "After Container Restart" above)
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

## 7. Troubleshooting

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

### Tunnel Not Working
```bash
# Check if cloudflared is running
ps aux | grep cloudflared

# Restart tunnel
pkill cloudflared
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
```

---

## 8. Quick Reference Commands

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

## 9. When to Restart What (Quick Reference)

| What Changed | Restart Container? | Restart Server? | Restart Frontend? | Notes |
|--------------|-------------------|-----------------|-------------------|-------|
| **property.json** | ❌ No | ❌ No | ⚠️ Yes if graphs added/removed | Server loads per session. Frontend caches graph list. |
| **.env file** | ⚠️ Option 1: Yes<br>✅ Option 2: No, source instead | ✅ Yes | ❌ No | Option 1: `docker compose down && up`<br>Option 2: Source .env + restart server (faster) |
| **Python code** | ❌ No | ✅ Yes | ❌ No | Stop and restart server to reload Python extensions |
| **Go code** | ❌ No | ✅ Yes + rebuild | ❌ No | Run `task install` first, then restart server |

**Key Findings**:
- Only ONE .env file is active: `/home/ubuntu/ten-framework/ai_agents/.env`
- Frontend caches `/graphs` API response - restart to see new/removed graphs

---

## 10. Essential Workflows

### Starting the Server
1. Use `task run` to start the server
2. Logs go to `/tmp/task_run.log` inside container
3. Server runs on port 8080

### After Container Restart
1. Reinstall Python dependencies
2. Start server with `task run`
3. Restart cloudflare tunnel

### After Code Changes
1. Run `task install` to rebuild (5-8 minutes first time)
2. Start server with `task run`

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
