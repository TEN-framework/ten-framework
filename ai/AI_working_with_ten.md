# TEN Framework Development Guide

**Purpose**: Complete onboarding guide for developing with TEN Framework v0.11+
**Last Updated**: 2025-11-05

---

## 📚 About This Documentation

This is the **COMPLETE REFERENCE** with detailed explanations, troubleshooting guidance, and step-by-step workflows.

**Two Documentation Files**:

1. **AI_working_with_ten.md** (this file - 2468 lines)
   - Full explanations of how things work
   - Comprehensive troubleshooting with root cause analysis
   - Step-by-step guides for creating extensions
   - Architecture details and best practices
   - **Use when**: You're learning TEN Framework OR need to understand "why"

2. **AI_working_with_ten_compact.md** (726 lines)
   - Copy-paste commands only
   - Quick syntax reference
   - Minimal explanation
   - **Use when**: You know what to do, just need the exact command

**Recommendation**: Read this full doc once, then bookmark compact doc for daily use.

---

## Table of Contents

1. [Framework Overview](#framework-overview)
2. [Environment Setup](#environment-setup)
3. [Building and Running](#building-and-running)
4. [Running Playground Client](#running-playground-client)
5. [Server Architecture & Property Injection](#server-architecture--property-injection)
6. [Creating Extensions](#creating-extensions)
7. [Graph Configuration](#graph-configuration)
8. [Debugging](#debugging)
9. [Remote Access](#remote-access)
10. [Common Issues](#common-issues)

---

## Framework Overview

### What is TEN Framework?

TEN (Transformative Extensions Network) is a graph-based AI agent framework that connects modular extensions (nodes) through defined data flows (connections). Version 0.11+ uses the `ten_runtime` Python API.

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
├── Taskfile.yaml          # Build/run automation
└── tenapp/
    ├── property.json      # Graph definitions
    ├── manifest.json      # App manifest
    └── ten_packages/
        └── extension/     # Custom extensions
```

**IMPORTANT: .env File Location**

**Only ONE .env file is used**: `/home/ubuntu/ten-framework/ai_agents/.env`

This file is loaded by `docker-compose.yml` and becomes the container's environment. All other .env files (previously found under `agents/`, `server/`, `playground/`, example subdirectories) have been removed as they were redundant.

### API Keys Management

**Best Practice**: Keep API keys in a file OUTSIDE the git repository (e.g., `/home/ubuntu/PERSISTENT_KEYS_CONFIG.md` or `~/api_keys.txt`). This allows you to:
- Switch git branches without losing keys
- Never accidentally commit secrets
- Reference keys when creating new `.env` files

**Create .env file at `/home/ubuntu/ten-framework/ai_agents/.env`:**
```bash
# Example .env structure
AGORA_APP_ID=your_app_id
AGORA_APP_CERTIFICATE=your_certificate

OPENAI_API_KEY=sk-...
OPENAI_MODEL=gpt-4o-mini

DEEPGRAM_API_KEY=...
ELEVENLABS_TTS_KEY=...
RIME_TTS_API_KEY=...

# Custom extension keys
YOUR_API_KEY=...
```

### Environment Variable Loading

**CRITICAL**: Environment variables must be available when the container/process starts.

#### When Do You Need to Restart?

| What Changed | Restart Container? | Restart Server? | Restart Frontend? | Notes |
|--------------|-------------------|-----------------|-------------------|-------|
| **property.json** | ❌ No | ❌ No | ⚠️ Yes if graphs added/removed | Server loads per session. Frontend caches graph list from `/graphs` API. |
| **.env file** | ⚠️ Option 1: Yes<br>✅ Option 2: No | ✅ Yes | ❌ No | Option 1: Full restart (`docker compose down && up`)<br>Option 2: Source .env + restart server (faster) |
| **Python code** | ❌ No | ✅ Yes | ❌ No | Python extensions are reloaded when server restarts |
| **Go code** | ❌ No | ✅ Yes + rebuild | ❌ No | Must run `task install` to rebuild, then restart server |

#### ❌ Does NOT Work:
1. Editing `.env` while services are running and expecting immediate effect
2. Restarting server without sourcing .env (server inherits old environment)
3. Expecting variables to propagate automatically

#### ✅ Works - Two Options:

**Option 1: Restart Docker container** (Recommended - guaranteed to work):
```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down
docker compose up -d

# Wait for container to start
sleep 3

# Reinstall Python deps (always needed after container restart)
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

# Start services with task run
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
```

**Option 2: Hardcode in property.json** (testing only, not recommended):
```json
{
  "property": {
    "api_key": "actual_value_here"
  }
}
```

⚠️ **IMPORTANT**: Environment variables are loaded when the container starts. `task run` automatically uses the correct environment if the container was started with the right .env file. Never try to source .env manually - just restart the container.

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

### Build and Run Workflow

**IMPORTANT**: Always use `task` commands, NOT direct binary execution!

**First time setup or after code changes:**
```bash
# Inside container
cd /app/agents/examples/voice-assistant-advanced
task install  # Installs dependencies, builds Go binary (~5-8 mins first time)
```

**After container restart, install Python dependencies:**
```bash
# Python packages don't persist across container restarts!
cd /app/agents/examples/voice-assistant-advanced/tenapp
bash scripts/install_python_deps.sh
```

**Start the server:**
```bash
# Inside container
cd /app/agents/examples/voice-assistant-advanced
task run > /tmp/task_run.log 2>&1 &

# Or from outside (detached)
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && task run > /tmp/task_run.log 2>&1"
```

**Why use `task run` instead of `./bin/main`?**
- `task run` sets up PYTHONPATH correctly for ten_runtime and ten_ai_base
- `./bin/main` directly will fail with Python import errors
- `task` commands are documented in Taskfile.yaml

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

### Quick System Health Diagnostic

**Run this ONE command to check everything**:

```bash
echo "=== API Server ===" && \
curl -s http://localhost:8080/health && \
echo -e "\n\n=== Graphs Count ===" && \
curl -s http://localhost:8080/graphs | jq '.data | length' && \
echo -e "\n=== Playground ===" && \
curl -s -o /dev/null -w 'HTTP %{http_code}\n' http://localhost:3000 && \
echo -e "\n=== Running Processes ===" && \
sudo docker exec ten_agent_dev bash -c "ps aux | grep -E 'bin/api|next.*dev|bun.*dev' | grep -v grep | wc -l" && \
echo " processes running (expect 2-3)"
```

**Expected Output**:
```
=== API Server ===
{"code":"0","data":null,"msg":"ok"}

=== Graphs Count ===
12

=== Playground ===
HTTP 200

=== Running Processes ===
3 processes running (expect 2-3)
```

**If Any Check Fails**, see [Nuclear Option](#nuclear-option-complete-system-reset) or [Common Issues](#common-issues) section.

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

## Running Playground Client

The TEN Framework includes a Next.js-based playground frontend that provides a web UI for testing voice agents. This section covers how to run and deploy the playground client.

### Playground Architecture

**Components:**
- **Frontend**: Next.js app (React) on port 3000
- **Backend API**: TEN Framework agent server (Go + Python extensions) on port 8080
- **Agora RTC**: Real-time audio/video communication

**Communication Flow:**
```
Browser → Playground (3000) → API Server (8080) → Extensions
                    ↓
               Agora RTC SDK
```

### ⚠️ CRITICAL: Node.js Version Requirement

**The playground requires Node.js 20.9.0 or higher.**

- ✅ Docker container (`ten_agent_dev`) has Node 22 installed
- ❌ Host machine may have older version (e.g., Node 18)

**❌ WRONG - Running from host with old Node version:**
```bash
# This FAILS if host has Node < 20.9.0
cd /home/ubuntu/ten-framework/ai_agents/playground
npm run dev  # Error: Node.js version ">=20.9.0" is required
```

**✅ CORRECT - Run from inside Docker container:**
```bash
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground.log 2>&1"
```

### Starting Playground: Automatic vs Manual

#### Option 1: Automatic Startup (Recommended)

The `task run` command automatically starts BOTH the agent server AND the playground:

```bash
# This starts everything
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Wait a few seconds for startup
sleep 5

# Verify both services are running
curl -s http://localhost:8080/health  # API server
curl -s http://localhost:3000         # Playground
```

**This is the easiest method for development and testing.**

#### Option 2: Custom Configuration

⚠️ **ALWAYS use `task run`** - never start services manually with `./bin/api`.

If you need custom ports or configuration:
- Edit `/app/agents/examples/voice-assistant-advanced/Taskfile.yml`
- Modify port settings or add environment variables there
- Then run `task run` as usual

Running `./bin/api` directly will fail with Python import errors because PYTHONPATH won't be set correctly.

### Building for Production

For production deployment, build the playground for optimized performance:

```bash
# 1. Build playground inside container
docker exec ten_agent_dev bash -c "cd /app/playground && npm run build"

# 2. Start production server
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm start > /tmp/playground_prod.log 2>&1"
```

**Production vs Development:**
- **Development** (`npm run dev`): Hot reload, source maps, slower
- **Production** (`npm run build && npm start`): Optimized bundles, faster, no hot reload

### Testing TypeScript Build

Before pushing code changes to the playground, test the TypeScript compilation locally to catch type errors early:

```bash
# Run TypeScript build test inside container
docker exec ten_agent_dev bash -c "cd /app/playground && npm run build"
```

**What this does:**
1. Compiles TypeScript files to check for type errors
2. Runs Next.js production build
3. Outputs any TypeScript errors with file location and line number

**Example output on error:**
```
Failed to compile.

./src/components/Agent/View.tsx:18:53
Type error: Argument of type 'IRemoteAudioTrack | undefined' is not assignable to parameter of type 'IMicrophoneAudioTrack | MediaStreamTrack | undefined'.

  > 18 |   const subscribedVolumes = useMultibandTrackVolume(audioTrack, 12);
       |                                                     ^
```

**Example output on success:**
```
 ✓ Compiled successfully in 8.0s
   Running TypeScript ...
   Collecting page data ...
   Generating static pages (0/5) ...
 ✓ Generating static pages (5/5) in 413.0ms
```

**When to run this test:**
- Before committing playground code changes
- After modifying TypeScript types or interfaces
- Before pushing to CI/CD pipelines
- When adding new components or hooks

**Note:** This is the same test that runs during Docker build, so running it locally helps avoid CI failures.

### Accessing Playground

#### Local Access (Same Machine)

```bash
# From host browser
http://localhost:3000

# Test with curl
curl -s http://localhost:3000 | head -20
```

#### Remote Access via Cloudflare Tunnel (HTTPS)

For remote testing without domain setup (free, temporary HTTPS):

```bash
# 1. Kill existing tunnels
pkill cloudflared

# 2. Start new tunnel
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &

# 3. Get public URL (wait 5-8 seconds for startup)
sleep 8
cat /tmp/cloudflare_tunnel.log | grep -o 'https://[^[:space:]]*\.trycloudflare\.com'
```

**Example output**: `https://films-colon-msgid-incentives.trycloudflare.com`

**⚠️ Important**: Free Cloudflare tunnels get random URLs that change on every restart. For permanent URLs, use named tunnels or nginx.

#### Remote Access via Nginx (Production)

For production deployment with custom domain and SSL certificate:

See the [Remote Access - Nginx Reverse Proxy](#nginx-reverse-proxy-production) section for complete nginx configuration.

**Quick setup:**
1. Point domain to server IP
2. Get SSL certificate (Let's Encrypt)
3. Configure nginx to proxy port 3000 (playground) and 8080 (API)
4. Reload nginx

### Verifying Playground is Running

**Check playground process:**
```bash
docker exec ten_agent_dev bash -c "ps aux | grep -E 'npm.*dev|node.*next' | grep -v grep"
# Expected: Shows node/npm process running
```

**Check port 3000 is listening:**
```bash
docker exec ten_agent_dev bash -c "netstat -tlnp | grep :3000"
# Expected: Shows process listening on port 3000
```

**Test HTTP response:**
```bash
curl -s -o /dev/null -w '%{http_code}' http://localhost:3000
# Expected: 200
```

**View playground logs:**
```bash
# Last 50 lines
docker exec ten_agent_dev tail -50 /tmp/playground.log

# Real-time monitoring
docker exec ten_agent_dev tail -f /tmp/playground.log
```

### Playground Environment Configuration

The playground reads configuration from environment variables and API endpoints.

**Environment variables** (from `.env` file mounted into Docker):
```bash
# API server endpoint (defaults to localhost:8080)
NEXT_PUBLIC_API_URL=http://localhost:8080

# Agora configuration (read from API server)
AGORA_APP_ID=${env:AGORA_APP_ID}
AGORA_APP_CERTIFICATE=${env:AGORA_APP_CERTIFICATE|}
```

**API endpoints the playground uses:**
- `/graphs` - List available voice agent graphs
- `/start` - Start agent session in a channel
- `/stop` - Stop agent session
- `/token/generate` - Generate Agora RTC token for client
- `/list` - List active sessions
- `/health` - Health check

**⚠️ CRITICAL**: If the playground shows "No graphs available", the API server (port 8080) is likely not running.

### Deploying Playground to Public Demo Server

Complete workflow for deploying the playground to a production server:

#### Step 1: Prepare Environment

```bash
# 1. Update .env with production API keys
cd /home/ubuntu/ten-framework/ai_agents
nano .env

# Add/update production keys:
# AGORA_APP_ID=production_app_id
# AGORA_APP_CERTIFICATE=production_cert
# DEEPGRAM_API_KEY=production_key
# OPENAI_API_KEY=production_key
# ... other production keys

# 2. Restart container to load new environment
docker compose down && docker compose up -d
```

#### Step 2: Build Playground for Production

```bash
# Build optimized production bundle
docker exec ten_agent_dev bash -c "cd /app/playground && npm run build"

# Verify build succeeded
docker exec ten_agent_dev bash -c "ls -lh /app/playground/.next"
```

#### Step 3: Configure Nginx Reverse Proxy

**Create nginx configuration** (`/etc/nginx/sites-enabled/ten-framework`):

```nginx
server {
    listen [::]:443 ssl ipv6only=on;
    listen 443 ssl;
    server_name your-domain.com;

    ssl_certificate /etc/letsencrypt/live/your-domain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/your-domain.com/privkey.pem;
    include /etc/letsencrypt/options-ssl-nginx.conf;
    ssl_dhparam /etc/letsencrypt/ssl-dhparams.pem;

    # Proxy TEN Framework API endpoints to localhost:8080
    location ~ ^/(health|ping|token|start|stop|graphs|list)(/|$) {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    # Proxy TEN Framework Playground to localhost:3000
    location / {
        proxy_pass http://localhost:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # WebSocket support for Next.js (required!)
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

**Apply nginx configuration:**
```bash
# Test configuration
sudo nginx -t

# Reload nginx
sudo systemctl reload nginx

# Verify nginx is listening
sudo netstat -tlnp | grep :443
```

#### Step 4: Start Production Services

```bash
# 1. Start agent server
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# 2. Wait for server startup
sleep 5

# 3. Verify API server
curl -s http://localhost:8080/health
# Expected: {"code":"0","data":null,"msg":"ok"}

# 4. Verify playground is running
curl -s -o /dev/null -w '%{http_code}' http://localhost:3000
# Expected: 200
```

#### Step 5: Test Public Access

```bash
# Test from external machine
curl -s https://your-domain.com/health
# Expected: {"code":"0","data":null,"msg":"ok"}

# Access playground in browser
# https://your-domain.com/
```

#### Step 6: Monitor Logs

```bash
# Monitor API server logs
docker exec ten_agent_dev tail -f /tmp/task_run.log

# Monitor playground logs
docker exec ten_agent_dev tail -f /tmp/playground.log

# Monitor nginx access logs
sudo tail -f /var/log/nginx/access.log

# Monitor nginx error logs
sudo tail -f /var/log/nginx/error.log
```

### Troubleshooting Playground

#### Issue: Playground Not Accessible (localhost:3000 fails)

**Symptom**: `curl http://localhost:3000` returns "Connection refused"

**Cause**: Playground not running

**Solution:**
```bash
# Check if playground is running
docker exec ten_agent_dev bash -c "ps aux | grep npm | grep dev | grep -v grep"

# If not running, start it
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground.log 2>&1"

# Check logs for errors
docker exec ten_agent_dev tail -50 /tmp/playground.log
```

#### Issue: "502 Bad Gateway" When Accessing Playground

**Symptom**: Playground loads but shows "502 Bad Gateway" error on certain API calls

**Cause**: Agent server (port 8080) is not running

**Solution:**
```bash
# Verify agent server is running
curl -s http://localhost:8080/health

# If fails, start agent server
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Wait and verify
sleep 5
curl -s http://localhost:8080/health
```

#### Issue: Playground Shows "No Graphs Available"

**Symptom**: Playground loads but graph dropdown is empty

**Cause 1**: Frontend cached `/graphs` API response before server started

**Solution:**
```bash
# Hard refresh browser (Ctrl+Shift+R or Cmd+Shift+R)
# Or restart playground to clear cache
docker exec ten_agent_dev bash -c "pkill -9 -f 'npm.*dev'"
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground.log 2>&1"
```

**Cause 2**: API server not running or no graphs configured

**Solution:**
```bash
# Check API server
curl -s http://localhost:8080/graphs | jq '.data[].name'

# If empty, check property.json
docker exec ten_agent_dev cat \
  /app/agents/examples/voice-assistant-advanced/tenapp/property.json | \
  jq '.ten.predefined_graphs[].name'
```

#### Issue: Playground Won't Start After Container Restart

**Symptom**: After `docker restart ten_agent_dev`, playground not accessible

**Cause**: Services don't auto-start when container starts

**Solution:**
```bash
# Manual startup after container restart:

# 1. Reinstall Python dependencies
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

# 2. Start agent server (this also starts playground)
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# 3. Verify both services
sleep 5
curl -s http://localhost:8080/health  # API
curl -s http://localhost:3000 | head   # Playground
```

#### Issue: "EADDRINUSE: Port 3000 Already in Use"

**Symptom**: Playground won't start, logs show port 3000 already in use

**Solution:**
```bash
# Find process using port 3000
docker exec ten_agent_dev bash -c "lsof -i :3000"

# Kill the process
docker exec ten_agent_dev bash -c "fuser -k 3000/tcp"

# Or kill all npm dev processes
docker exec ten_agent_dev bash -c "pkill -9 -f 'npm.*dev'"

# Restart playground
docker exec -d ten_agent_dev bash -c \
  "cd /app/playground && npm run dev > /tmp/playground.log 2>&1"
```

### Playground Deployment Checklist

Use this checklist when deploying playground to production:

- [ ] Production `.env` file configured with all API keys
- [ ] Container restarted to load new environment: `docker compose down && up -d`
- [ ] Python dependencies installed: `bash scripts/install_python_deps.sh`
- [ ] Playground built for production: `npm run build`
- [ ] Nginx configured with SSL certificate and reverse proxy
- [ ] Nginx configuration tested: `sudo nginx -t`
- [ ] Nginx reloaded: `sudo systemctl reload nginx`
- [ ] Agent server running: `curl http://localhost:8080/health` returns OK
- [ ] Playground running: `curl http://localhost:3000` returns 200
- [ ] Graphs available: `curl http://localhost:8080/graphs` shows graphs
- [ ] Public HTTPS access working: `curl https://your-domain.com/health`
- [ ] Browser access working: Open `https://your-domain.com/` in browser
- [ ] WebSocket connection working: Test voice agent session
- [ ] Monitoring logs: `tail -f /tmp/task_run.log` and `/tmp/playground.log`

---

## Server Architecture & Property Injection

### Dynamic Property Injection

The TEN Framework Go server (`ai_agents/server/internal/http_server.go`) implements property-based auto-injection for dynamic configuration values. This allows request parameters to be automatically injected into graph nodes without hardcoding extension names.

**Key Feature: Channel Name Auto-Injection**

When a client calls the `/start` API endpoint with a `channel_name` parameter, the server automatically injects this value into **all nodes that have a "channel" property** defined in their configuration.

**How it works:**

1. Client sends `/start` request with `{"channel_name": "user_channel_123", ...}`
2. Server loads the base `property.json` template for the selected graph
3. Server scans all nodes in the graph for a "channel" property
4. Any node with a "channel" property receives the dynamic channel value
5. Session starts with properly configured channel across all extensions

**Example:**

```json
{
  "nodes": [
    {
      "name": "agora_rtc",
      "addon": "agora_rtc",
      "property": {
        "channel": "default_channel",  // ← Will be replaced with dynamic value
        "app_id": "${env:AGORA_APP_ID}"
      }
    },
    {
      "name": "avatar",
      "addon": "heygen_avatar_python",
      "property": {
        "channel": "default_channel",  // ← Will be replaced with same dynamic value
        "heygen_api_key": "${env:HEYGEN_API_KEY}"
      }
    }
  ]
}
```

**Benefits:**
- **Future-proof**: New extensions with "channel" properties automatically receive dynamic values
- **No code changes needed**: Adding new extensions that need channel forwarding requires no server code changes
- **Type-safe**: Works for any extension type (audio, video, avatar, analytics)

**Implementation location:**
- Server logic: `ai_agents/server/internal/http_server.go:646-690`
- Configuration mapping: `ai_agents/server/internal/config.go:31-52`

**Other injected parameters:**

The server also injects other request parameters using the `startPropMap` configuration:
- `RemoteStreamId` → `agora_rtc.remote_stream_id`
- `BotStreamId` → `agora_rtc.stream_id`
- `Token` → `agora_rtc.token` and `agora_rtm.token`
- `WorkerHttpServerPort` → `http_server.listen_port`

To add new parameters, update `startPropMap` in `server/internal/config.go`.

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

#### Signal Handlers (NEVER USE!)

**❌ CRITICAL: Do NOT use signal handlers in TEN extensions!**

Signal handlers (`signal.signal()`, `atexit.register()`) **only work in the main thread** of a process. TEN Framework extensions run in worker threads, not the main thread, causing runtime errors.

**❌ WRONG - This will fail:**
```python
import signal
import atexit

class MyExtension(AsyncExtension):
    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        # This will raise: ValueError: signal only works in main thread
        signal.signal(signal.SIGTERM, self._cleanup)
        signal.signal(signal.SIGINT, self._cleanup)
        atexit.register(self._emergency_cleanup)
```

**Error you'll see:**
```
ValueError: signal only works in main thread of the main interpreter
```

**✅ CORRECT - Use lifecycle methods instead:**
```python
class MyExtension(AsyncExtension):
    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("Extension starting")
        # Initialize resources here

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("Extension stopping - cleanup here")
        # Clean up resources here (close connections, flush data, etc.)
        if self.websocket:
            await self.websocket.close()
```

**Why this matters:**
- TEN Framework's worker processes handle cleanup automatically
- `on_stop()` is always called before worker termination
- Signal handlers are unnecessary and cause production failures

**Real-world issue:** The heygen_avatar_python extension had signal handlers that caused production failures. These were removed and replaced with proper `on_stop()` cleanup, resolving the issue completely.

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

### After Modifying property.json

**Server restart: NOT needed!** The API server loads property.json when each new session starts (when user joins a channel).

**Frontend restart: NEEDED if graph list changed!** The playground frontend caches the graph list from the `/graphs` API endpoint.

```bash
# If you added/removed graphs, restart frontend to clear cache
docker exec ten_agent_dev bash -c "pkill -9 -f 'bun.*dev'"
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/playground && \
   bun run dev > /tmp/playground.log 2>&1"

# Check which port it started on
docker exec ten_agent_dev tail -10 /tmp/playground.log | grep "Local:"
```

**To apply changes to existing session**, stop and restart that specific session:
```bash
curl -X POST http://localhost:8080/stop \
  -H "Content-Type: application/json" \
  -d '{"channel_name": "your_channel"}'
```

---

## Creating Example Variants

### When to Create a New Example

Create a new example variant when you want to:
- Add advanced features without affecting the basic example
- Test experimental configurations
- Separate different use cases (e.g., HeyGen avatars, mental wellness analysis)
- Avoid impacting other developers working on the main example

### Creating a New Example from Existing One

**Example: Creating `voice-assistant-advanced` from `voice-assistant`**

#### Step 1: Copy Directory Structure

```bash
cd /app/agents/examples/
cp -r voice-assistant voice-assistant-advanced
cd voice-assistant-advanced
```

#### Step 2: Split Graphs in property.json

Use Python to programmatically split graphs:

```python
import json

# Load existing property.json
with open('tenapp/property.json', 'r') as f:
    data = json.load(f)

# Get all graphs
all_graphs = data['ten']['predefined_graphs']

# Split into basic and advanced
basic_graphs = [g for g in all_graphs if g['name'] in ['voice_assistant']]
advanced_graphs = [g for g in all_graphs if g['name'] in ['voice_assistant_thymia', 'voice_assistant_heygen', 'voice_assistant_generic_video']]

# Update advanced example with only advanced graphs
data['ten']['predefined_graphs'] = advanced_graphs
with open('tenapp/property.json', 'w') as f:
    json.dump(data, f, indent=2)

# Update basic example (in original directory)
basic_data = {'ten': {'predefined_graphs': basic_graphs}}
with open('../voice-assistant/tenapp/property.json', 'w') as f:
    json.dump(basic_data, f, indent=2)
```

**Key Points:**
- Each example has its own `property.json` with different graphs
- Graphs can use different extensions (e.g., different STT providers)
- Set `"auto_start": true` on the default graph for each example

#### Step 3: Update manifest.json with New Dependencies

If your graphs use new extensions, add them to `tenapp/manifest.json`:

```json
{
  "type": "app",
  "name": "agent_demo",
  "version": "0.10.0",
  "dependencies": [
    {
      "type": "system",
      "name": "ten_runtime_go",
      "version": "0.11"
    },
    {
      "path": "../../../ten_packages/extension/deepgram_asr_python"
    },
    {
      "path": "../../../ten_packages/extension/deepgram_ws_asr_python"
    },
    {
      "path": "../../../ten_packages/extension/thymia_analyzer_python"
    }
  ]
}
```

**Critical:** If you add a new extension to a graph but forget to add it to `manifest.json`, the worker process will fail silently!

#### Step 4: Install Dependencies

After updating `manifest.json`, run `tman install` to automatically create symlinks and install dependencies:

```bash
cd tenapp
tman install
```

> **⚠️ IMPORTANT**: `tman install` creates symlinks automatically. Never manually create symlinks with `ln -s`.

**What `tman install` does:**
- Resolves dependencies from `manifest.json`
- **Automatically creates symlinks** from shared extensions to `tenapp/ten_packages/extension/`
- Downloads external packages if needed
- Updates `manifest-lock.json`
- Installs Python packages from `requirements.txt`

**Verify symlinks were created:**
```bash
ls -la tenapp/ten_packages/extension/ | grep deepgram_ws_asr_python
# Should show: deepgram_ws_asr_python -> /app/agents/ten_packages/extension/deepgram_ws_asr_python
```

**Why symlinks?**
- Extensions are stored once in `/app/agents/ten_packages/extension/`
- Each example links to them rather than copying
- Saves space and keeps extensions in sync

**Important:**
- Run `tman install` inside the Docker container if using Docker
- Run it after every `manifest.json` change
- Python dependencies may need separate `pip install` after container restarts

#### Step 5: Update README.md

Document your new example:

```bash
cd voice-assistant-advanced
cat > README.md << 'EOF'
# Voice Assistant Advanced

Advanced voice assistant configurations with specialized features.

## Graphs

1. **voice_assistant_thymia** (auto_start: true)
   - Mental wellness analysis with Thymia
   - Deepgram Flux STT with turn detection
   - ElevenLabs TTS

2. **voice_assistant_heygen**
   - HeyGen avatar integration
   - Standard Deepgram Nova-3 STT

3. **voice_assistant_generic_video**
   - Generic video avatar protocol
   - Standard Deepgram Nova-3 STT

## Running

⚠️ **ALWAYS use `task run`** - never run `./bin/api` directly.

```bash
cd /app/agents/examples/voice-assistant-advanced
task run
```

## Testing

```bash
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{"graph_name": "voice_assistant_thymia", "channel_name": "test", "remote_stream_id": 123}'
```
EOF
```

### Running Different Examples

The API server can only load one example at a time. To switch examples, use `task run` from the desired example directory.

#### In Docker Container

**To switch to a different example:**
```bash
# Stop current services
docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"

# Start the desired example (e.g., voice-assistant-advanced)
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Wait for startup
sleep 10

# Verify correct example loaded
curl -s http://localhost:8080/graphs | python3 -m json.tool
```

⚠️ **IMPORTANT**: Always use `task run` from the example directory. Never use `./bin/api` directly as it will fail with incorrect PYTHONPATH.

#### Using Docker Build (Production)

```bash
# Build image for specific example
cd ai_agents
docker build -f agents/examples/voice-assistant-advanced/Dockerfile \
  -t voice-assistant-advanced \
  --build-arg USE_AGENT=agents/examples/voice-assistant-advanced .

# Run container
docker run --rm -it --env-file .env \
  -p 8080:8080 -p 3000:3000 \
  voice-assistant-advanced
```

**Note:** Dockerfile may need adjustments for new examples. See `ai/deepgram/DOCKER_BUILD_NOTES.md` for details.

#### Verifying the Correct Example is Loaded

```bash
# Check which graphs are available
curl -s http://localhost:8080/graphs | python3 -c "import json,sys; data=json.load(sys.stdin); print('\\n'.join([g['name'] for g in data['data']]))"

# Expected output for voice-assistant-advanced:
# voice_assistant_heygen
# voice_assistant_generic_video
# voice_assistant_thymia
```

### Common Issues When Creating Examples

#### Issue 1: Worker Process Fails Silently

**Symptoms:**
- Session starts successfully (`/start` returns `{"code":"0"}`)
- Worker process dies after exactly 60 seconds
- No log file created in `/tmp/ten_agent/`
- Session shows as active but no activity

**Causes:**
1. **Missing extension in manifest.json** ← Most common!
   - Graph references `deepgram_ws_asr_python`
   - But `manifest.json` doesn't include it as dependency
   - Worker fails during initialization, no logs written

2. **Missing symlink**
   - Extension listed in manifest.json
   - But symlink doesn't exist in `tenapp/ten_packages/extension/`

3. **Python import error**
   - Extension depends on package not installed
   - Check with: `docker exec container pip3 list | grep aiohttp`

**Solutions:**

```bash
# 1. Verify extension is in manifest.json
cat tenapp/manifest.json | grep -A2 "deepgram_ws_asr_python"

# 2. Add extension to manifest.json if missing, then run tman install
# This creates the symlink automatically - never create manually!
cd tenapp && tman install

# 3. Verify symlink was created
ls -la tenapp/ten_packages/extension/ | grep deepgram_ws_asr_python

# 4. Check Python dependencies
docker exec container pip3 install aiohttp pydantic
```

#### Issue 2: Wrong API Server Running

**Symptoms:**
- Start session for `voice_assistant_thymia`
- But only basic graphs are available
- `/graphs` endpoint shows wrong graphs

**Cause:** API server still pointing to old tenapp directory

**Solution:**
```bash
# Check API server logs for tenapp_dir
docker exec container tail -20 /tmp/api_advanced.log | grep tenappDir

# Should show: tenappDir=/app/agents/examples/voice-assistant-advanced/tenapp
# If not, kill and restart pointing to correct directory
```

#### Issue 3: Environment Variables Not Loading

**Symptoms:**
- Session starts but extension can't connect to API
- Logs show: `Environment variable DEEPGRAM_API_KEY is not found`

**Cause:** Server not started with environment variables from .env file

**Solutions:**
```bash
# Option 1: Restart container (recommended - guaranteed to work)
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d

# Wait for container to start
sleep 3

# Reinstall Python deps
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

# Start services with task run
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Option 2: Hardcode for testing (temporary, not recommended)
# Edit property.json: "api_key": "actual_key_here"
```

⚠️ **IMPORTANT**: Never try to source .env manually. Environment variables are loaded at container startup. Always use `task run` to start services.

**Remember**: Only `/home/ubuntu/ten-framework/ai_agents/.env` is used. All other .env files have been removed.

#### Issue 4: Graph Configuration Errors

**Symptoms:**
- Extension loads successfully
- But graph fails to start or crashes

**Common mistakes:**
1. **Wrong extension name in graph:**
   ```json
   // ❌ WRONG - addon name doesn't match
   {"addon": "deepgram_asr", "extension_group": "stt"}

   // ✅ CORRECT
   {"addon": "deepgram_asr_python", "extension_group": "stt"}
   ```

2. **Missing required properties:**
   ```json
   // ❌ WRONG - missing api_key
   {"addon": "deepgram_ws_asr_python", "property": {"params": {"model": "flux"}}}

   // ✅ CORRECT
   {"addon": "deepgram_ws_asr_python", "property": {"params": {"api_key": "${env:DEEPGRAM_API_KEY}", "model": "flux"}}}
   ```

3. **Wrong property nesting:**
   - Some extensions expect `property.params.api_key`
   - Others expect `property.api_key`
   - Check existing extension's `property.json` for correct structure

### Testing Individual Extensions

Test extensions locally before pushing to avoid CI failures. The task command handles dependency installation and runs pytest in the correct environment.

**Command:**
```bash
cd /home/ubuntu/ten-framework/ai_agents
docker exec ten_agent_dev bash -c "cd /app && task test-extension EXTENSION=agents/ten_packages/extension/<ext_name> -- -s -v"
```

**What it does:**
1. `cd` into the extension directory
2. `tman -y install --standalone` - Installs extension dependencies in standalone mode
3. `./tests/bin/start -s -v` - Runs pytest with verbose output and no output capture

**Example:**
```bash
# Test heygen_avatar_python extension
docker exec ten_agent_dev bash -c "cd /app && task test-extension EXTENSION=agents/ten_packages/extension/heygen_avatar_python -- -s -v"
```

**Common test failures:**
- **Permission denied on start script**: `chmod +x tests/bin/start`
- **ModuleNotFoundError for 'ten'**: Use `from ten_runtime import` not `from ten import`
- **Missing required argument in API calls**: Check CmdResult.create() signature requires `(status, cmd)`
- **Missing environment variables**: Add empty defaults in property.json: `"api_key": "${env:API_KEY|}"`

**Best practice:** Always test extensions locally before committing. Run tests for all modified extensions before pushing.

### Best Practices for Example Variants

1. **Naming Convention:**
   - Basic: `voice-assistant`
   - Advanced: `voice-assistant-advanced`
   - Specialized: `voice-assistant-live2d`, `voice-assistant-avatar`

2. **Graph Organization:**
   - Keep simple, common use case in basic example
   - Move experimental/specialized features to variant
   - Each example should have clear, focused purpose

3. **Documentation:**
   - Update README.md for each example
   - Document which graphs are included
   - Explain differences from basic example
   - Provide testing instructions

4. **Dependency Management:**
   - Only include extensions actually used in graphs
   - Run `tman install` after manifest changes
   - Document any special Python dependencies

5. **Testing New Examples:**
   ```bash
   # 1. Run tman install (creates symlinks automatically)
   cd tenapp && tman install

   # 2. Verify symlinks were created
   ls -la tenapp/ten_packages/extension/ | grep your_extension

   # 3. Start server with task run
   cd /app/agents/examples/your-example
   task run > /tmp/task_run.log 2>&1 &
   sleep 10

   # 4. Check graphs loaded
   curl http://localhost:8080/graphs | jq '.data[].name'

   # 5. Test session start
   curl -X POST http://localhost:8080/start \
     -H "Content-Type: application/json" \
     -d '{"graph_name": "your_graph", "channel_name": "test", "remote_stream_id": 123}'

   # 6. Monitor logs
   tail -f /tmp/api_advanced.log
   ```

### Checklist for Creating New Example

- [ ] Copy existing example directory
- [ ] Split/update `property.json` with desired graphs
- [ ] Update `manifest.json` with all extension dependencies
- [ ] Run `tman install` in tenapp directory (creates symlinks automatically)
- [ ] Verify symlinks were created: `ls -la tenapp/ten_packages/extension/`
- [ ] Update README.md with graph descriptions
- [ ] Copy or reference `.env` file
- [ ] Test API server startup with new tenapp_dir
- [ ] Verify graphs load correctly (`/graphs` endpoint)
- [ ] Test session start with each graph
- [ ] Check worker logs for errors
- [ ] Document any special configuration needs

---

## Debugging

### Log Monitoring

**CRITICAL**: All Python extension logs appear in `/tmp/task_run.log` inside the container, NOT in docker logs.

**Primary monitoring command (real-time):**
```bash
docker exec ten_agent_dev tail -f /tmp/task_run.log
```

**Filter for specific channel:**
```bash
docker exec ten_agent_dev tail -f /tmp/task_run.log | grep --line-buffered "channel_name"
```

**Filter for specific extension:**
```bash
docker exec ten_agent_dev tail -f /tmp/task_run.log | grep --line-buffered -i "thymia"
```

**View recent logs:**
```bash
docker exec ten_agent_dev tail -200 /tmp/task_run.log | grep -a "pattern"
```

### Log Locations

**Application logs:**
```bash
/tmp/task_run.log              # Main service logs + ALL Python extension output
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

### Log Format

All worker/extension logs are prefixed with the channel name:
```
[channel_name] Successfully registered addon 'my_extension'
[channel_name] Extension message here
```

Server-level logs have no prefix or include `service=HTTP_SERVER`.

### Finding Errors

**Python tracebacks:**
```bash
docker exec ten_agent_dev tail -200 /tmp/task_run.log | grep -A 20 "Traceback"
```

**Extension errors:**
```bash
docker exec ten_agent_dev tail -200 /tmp/task_run.log | grep -E "(ERROR|Uncaught exception)"
```

**Check loaded extensions:**
```bash
docker exec ten_agent_dev tail -200 /tmp/task_run.log | grep "Successfully registered addon"
```

**Session-specific logs:**
```bash
# Find latest session
docker exec ten_agent_dev ls -lt /tmp/ten_agent/*.json | head -1

# Monitor specific channel in real-time
docker exec ten_agent_dev tail -f /tmp/task_run.log | grep --line-buffered "channel_name"
```

### Python Extension Logging

**Using TEN framework logging (recommended):**
```python
async def on_start(self, ten_env: AsyncTenEnv) -> None:
    ten_env.log_info("Extension starting...")
    ten_env.log_warn("Warning message")
    ten_env.log_error("Error message")
```

**Using print() for debugging:**
```python
import sys
print(f"Debug message", flush=True)
sys.stdout.flush()
```

Both methods output to `/tmp/task_run.log` with the channel name prefix.

### Log Configuration Requirements ✅

**CRITICAL**: For TEN framework logging to work, you need proper configuration in both `.env` and `property.json`.

**Required Configuration:**

**1. .env variables** (use standard variables from `.env.example`):
```bash
# Log & Server & Worker
LOG_PATH=/tmp/ten_agent
LOG_STDOUT=true                    # ← CRITICAL: Enables worker stdout/stderr output
GRAPH_DESIGNER_SERVER_PORT=49483
SERVER_PORT=8080
WORKERS_MAX=100
WORKER_QUIT_TIMEOUT_SECONDS=60

# Optional but helpful for logging
TEN_LOG_FORMATTER=json
PYTHONUNBUFFERED=1
```

**Key variable**: `LOG_STDOUT=true` is the official way to enable log output from worker processes.

**2. property.json log configuration** (CRITICAL - without this, `ten_env.log_info()` is silent):
```json
{
  "ten": {
    "log": {
      "handlers": [
        {
          "matchers": [
            {
              "level": "debug"
            }
          ],
          "formatter": {
            "type": "plain",
            "colored": false
          },
          "emitter": {
            "type": "console",
            "config": {
              "stream": "stdout"
            }
          }
        }
      ]
    },
    "predefined_graphs": [...]
  }
}
```

**Important notes:**
- Use lowercase `"level": "debug"` (not "DEBUG")
- Use `"colored": false` (not "with_color")
- Nest stream in config object: `"config": {"stream": "stdout"}` (not `"stream": "stdout"` directly)
- Without this configuration, Python `ten_env.log_*()` calls will be completely silent
- `print()` statements work regardless of this configuration
- **No worker.go modifications needed** - the original platform code works correctly with proper configuration

**After changing property.json:**

**⚠️ If adding/removing graphs:** The playground frontend caches the graph list from `/graphs` API. Use the [Nuclear Restart](#nuclear-option-complete-system-reset) to avoid lock file and cache issues.

**If only modifying existing graph configs:**
```bash
# Just restart the server (no rebuild needed)
docker exec ten_agent_dev bash -c "pkill -f 'task run'"
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
```

**Note**: Property.json is loaded when each new session starts (when user joins a channel), so existing sessions won't see changes until they reconnect.

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

### Nginx Reverse Proxy (Production)

For production deployments with a custom domain and SSL certificate, use nginx as a reverse proxy.

**Prerequisites:**
- Domain name (e.g., oai.agora.io)
- SSL certificate (Let's Encrypt recommended)
- Nginx installed on server

**Configuration Pattern:**

The TEN Framework requires proxying two services:
1. **API Server** (port 8080) - Handles `/health`, `/ping`, `/token`, `/start`, `/stop`, `/graphs`, `/list`
2. **Playground Frontend** (port 3000) - Next.js app with WebSocket support

**Add to `/etc/nginx/sites-enabled/default`:**

```nginx
server {
    listen [::]:453 ssl ipv6only=on;
    listen 453 ssl;
    ssl_certificate /etc/letsencrypt/live/oai.agora.io/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/oai.agora.io/privkey.pem;
    include /etc/letsencrypt/options-ssl-nginx.conf;
    ssl_dhparam /etc/letsencrypt/ssl-dhparams.pem;

    # Proxy TEN Framework API endpoints to localhost:8080
    location ~ ^/(health|ping|token|start|stop|graphs|list)(/|$) {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    # Proxy TEN Framework Playground to localhost:3000
    location / {
        proxy_pass http://localhost:3000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # WebSocket support for Next.js hot reload
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

**Apply configuration:**
```bash
# Test nginx configuration
sudo nginx -t

# Reload nginx
sudo systemctl reload nginx

# Verify port is listening
sudo netstat -tlnp | grep :453
```

**Access:**
- Playground: `https://oai.agora.io:453/`
- API Health: `https://oai.agora.io:453/health`
- Graphs List: `https://oai.agora.io:453/graphs`

**Notes:**
- Use custom SSL ports (e.g., 453, 454) to host multiple playgrounds on the same server
- Each port can point to a different Docker container with different TEN Framework configurations
- WebSocket support is essential for Next.js development mode hot reload
- For standard HTTPS (port 443), change `listen 453 ssl` to `listen 443 ssl`

---

## Common Issues

### Nuclear Option: Complete System Reset

**When to use**:
- Multiple services not responding
- Conflicting processes running
- Lock file errors
- "No graphs" issue persists
- After major configuration changes
- When you're not sure what's broken

⚠️ **This should be your FIRST troubleshooting step, not your last.**

**The Nuclear Command** (copy-paste this):

```bash
# Step 1: Kill EVERYTHING
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"
rm -f /tmp/cloudflare_tunnel.log
sleep 2

# Step 2: Clean up lock files
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"

# Step 3: Start everything fresh
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Step 4: Wait for startup (DO NOT SKIP THIS)
echo "Waiting for services to start..."
sleep 12

# Step 5: Verify everything is working
echo "=== API Health ===" && curl -s http://localhost:8080/health
echo -e "\n=== Graphs ===" && curl -s http://localhost:8080/graphs | jq '.data | length'
echo -e "\n=== Playground ===" && curl -s -o /dev/null -w 'HTTP %{http_code}\n' http://localhost:3000
```

**Expected Result**: All three checks pass (health OK, graphs > 0, playground 200)

**If Still Failing**: Container restart required
```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d
# Then run nuclear command again
```

**Success Indicators**:
- ✅ API health returns `{"code":"0"}`
- ✅ Graphs count > 0
- ✅ Playground returns HTTP 200
- ✅ Can see graphs in dropdown at your URL

---

### Issue: Playground Shows "No Graphs Available"

**Symptoms**:
- Playground loads successfully
- Graph dropdown is empty or shows "No graphs available"
- `/graphs` API endpoint returns correct data when tested with curl

**Cause**: Frontend cached the `/graphs` API response before server was ready

**Diagnosis**:
```bash
# 1. Verify API server has graphs
curl -s http://localhost:8080/graphs | jq '.data | length'
# If > 0, the problem is frontend cache

# 2. Check frontend is running
curl -s -o /dev/null -w '%{http_code}' http://localhost:3000
# Should return 200
```

**Solution - Use Nuclear Restart** (see above section)

**Why This Happens**:
- Playground starts before API server is ready
- Makes /graphs request, gets error or empty response
- Caches the bad response
- Server starts correctly later, but frontend still shows cached empty list

**Prevention**:
- Always start services together with `task run`
- Wait 10-12 seconds after startup before accessing frontend
- Use the nuclear restart procedure which ensures correct startup order

---

### Issue: Next.js Lock File Error

**Symptoms**:
```
⨯ Unable to acquire lock at /app/playground/.next/dev/lock, is another instance of next dev running?
error: script "dev" exited with code 1
Received interrupt signal, cleaning up workers...
```

**Cause**: Multiple scenarios can cause this:
1. Previous Next.js process didn't clean up properly
2. Attempting to restart frontend while `task run` is managing it
3. Manual frontend restart causing conflict with task runner
4. Container restart without cleaning lock files

**Why It's Serious**: When `task run` starts the frontend and it crashes due to lock file, the **entire task run fails**, taking down the API server too.

**Solution - Use Nuclear Restart:**
```bash
# Kill everything cleanly
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"
sleep 2

# Start fresh
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Wait and verify
sleep 12
curl -s http://localhost:8080/health
```

**Prevention**:
- **Don't manually restart the frontend** when it's managed by `task run`
- When adding/removing graphs from property.json, use nuclear restart instead of selective restarts
- If you need to restart only the frontend for development, stop `task run` first, then start frontend separately

**Key Insight**: The frontend (playground) and API server are coupled through `task run`. Killing just the frontend can leave the task runner in a bad state, requiring a full nuclear restart.

---

### Issue: Playground Shows "missing required error components, refreshing..."

**Symptoms**:
- Playground loads but shows "missing required error components, refreshing..." message
- Page continuously refreshes/reloads
- Console shows: `Error: ENOENT: no such file or directory, open '/app/playground/.next/dev/server/pages/_app/build-manifest.json'`

**Cause**: One or more of the following:
1. **Stale Next.js server processes** from previous sessions (can persist for days!)
2. Multiple `next-server` processes running simultaneously and conflicting
3. Corrupted `.next` build directory (often caused by deleting `.next` while server was running)
4. Next.js cache out of sync with code changes

**Why It's Serious**: This completely breaks the playground. Users can't access the interface or test graphs.

**Diagnosis**:
```bash
# Check for multiple/stale next-server processes
sudo docker exec ten_agent_dev bash -c "ps aux | grep next-server"

# Look for processes from old dates (e.g., Nov10 when today is Nov11)
# or multiple next-server processes running
```

**Solution - Kill All Next Processes and Rebuild:**
```bash
# 1. Find all next-server PIDs
sudo docker exec ten_agent_dev bash -c "ps aux | grep -E 'next-server|next dev' | grep -v grep"

# 2. Kill them by PID (replace with actual PIDs from step 1)
sudo docker exec ten_agent_dev bash -c "kill -9 PID1 PID2 PID3 2>/dev/null; exit 0"

# 3. Optionally reinstall dependencies if corruption suspected
sudo docker exec ten_agent_dev bash -c "cd /app/playground && npm install"

# 4. Clean restart
sleep 3
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# 5. Wait for full startup
sleep 20
curl -s http://localhost:8080/health
```

**Alternative - Nuclear Restart (if above doesn't work):**
```bash
# Kill everything
sudo docker exec ten_agent_dev bash -c "killall -9 node bun 2>/dev/null; exit 0"
sudo docker exec ten_agent_dev bash -c "rm -rf /app/playground/.next"
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"
sleep 3

# Fresh start
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# Wait longer for .next rebuild
sleep 25
curl -s http://localhost:8080/health
```

**Prevention**:
- **Always use clean shutdowns**: Use nuclear restart procedure instead of killing individual processes
- **Don't delete `.next` while server is running**: This corrupts the build state
- **Check for stale processes**: Run `ps aux | grep next-server` before starting services
- **Use proper restart procedures**: Follow documented restart workflows

**Root Cause**: Next.js development mode creates long-lived server processes that can survive container restarts and persist across sessions. When multiple instances run, they conflict over the `.next/dev` directory, causing build manifest errors. The "missing required error components" message is Next.js's generic error when it can't load its internal build files.

---

### Issue 1: ValueError: signal only works in main thread

**Symptoms**:
```
ValueError: signal only works in main thread of the main interpreter
```

**Cause**: Extension attempting to register signal handlers (`signal.signal()`, `atexit.register()`)

**Why it fails**: TEN Framework extensions run in worker threads, not the main thread. Signal handlers only work in the main thread.

**Solution**: Remove all signal handler code and use lifecycle methods instead

```python
# ❌ Remove signal handler imports
# import signal
# import atexit

class MyExtension(AsyncExtension):
    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        # ❌ Remove signal handler registration
        # signal.signal(signal.SIGTERM, self._cleanup)
        # atexit.register(self._emergency_cleanup)

        # ✅ Just initialize resources
        ten_env.log_info("Extension starting")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        # ✅ Cleanup happens here
        ten_env.log_info("Extension stopping - performing cleanup")
        if self.websocket:
            await self.websocket.close()
```

**See also**: [Signal Handlers (NEVER USE!)](#signal-handlers-never-use) in the Creating Extensions section.

---

### Issue 2: ModuleNotFoundError: No module named 'ten'

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

**Solution**:
```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d

# Wait for container
sleep 3

# Reinstall Python deps
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

# Start services
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
```

**Remember**: Only `/home/ubuntu/ten-framework/ai_agents/.env` is used. All other .env files have been removed.

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

### ⚠️ CRITICAL: Zombie Worker Processes

**Symptom**: Client shows STT transcribing even after server restart, or old sessions appear active

**Cause**: Worker processes (`bin/main`) survive server restart AND container restart. They run on the host, not in Docker.

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

**Complete cleanup script** (use before every restart):
```bash
# Kill zombie workers (on host - NOT in container!)
ps -elf | grep 'bin/main' | grep -v grep | awk '{print $4}' | xargs -r sudo kill -9

# Kill log monitors
sudo pkill -9 -f 'tail.*task_run.log' 2>/dev/null || true

# Kill API server (in container)
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'" 2>/dev/null || true
```

---

When something isn't working:

- [ ] **Are zombie workers running?** `ps -elf | grep 'bin/main' | grep -v grep` (kill them!)
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

### Pre-commit Checks

**IMPORTANT:** Always run these checks before committing to avoid CI failures:

```bash
# Run from ai_agents directory
cd ai_agents

# 1. Check Python formatting (required for Python files)
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents && black --check --line-length 80 \
  --exclude 'third_party/|agents/ten_packages/extension/http_server_python/|ten_packages/system' \
  ten_packages/extension"

# If formatting fails, auto-fix with:
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents && black --line-length 80 \
  --exclude 'third_party/|agents/ten_packages/extension/http_server_python/|ten_packages/system' \
  ten_packages/extension"

# 2. Run all checks (includes formatting, linting, tests)
sudo docker exec ten_agent_dev bash -c "cd /app/agents && task check"
```

**Common formatting issues:**
- Lines longer than 80 characters will be auto-wrapped by Black
- Black reformats imports, spacing, quotes to match PEP 8
- Run `black` formatter before committing to avoid CI failures

### Commit Messages

Follow conventional commits format strictly. A local git hook (`.git/hooks/commit-msg`) validates commit messages before they're created, and the CI will reject commits that don't follow these rules.

**Rules:**
- **Subject must be lowercase** - No Sentence-case, Start-case, UPPER-CASE allowed
- **Valid types**: build, chore, ci, docs, feat, fix, perf, refactor, revert, style, test
- **Body/footer lines must be ≤100 characters**
- **No references to AI assistants or code generation tools**

**Examples:**
```bash
# ✅ CORRECT
fix: correct import statements in heygen extension

feat: add deepgram flux v2 support

chore: add execute permissions to test script

test: update heygen extension test configuration

# ❌ WRONG - subject not lowercase
Fix: Correct import statements in heygen extension

# ❌ WRONG - invalid type
update: add new feature

# ❌ WRONG - AI assistant reference
fix: correct imports

Generated with Claude Code
```

**Wrapping long commit bodies:**
```bash
# Use fold to wrap lines at 100 characters
git commit -m "$(cat <<'EOF'
fix: correct multiple import errors in extensions

This commit addresses several import issues that were causing test
failures. Changed from legacy 'ten' imports to 'ten_runtime' across
all affected files.
EOF
)"
```

**Setting up git hooks:**

There are two git hooks that enforce code quality:

1. **pre-commit** - Runs before commit, checks:
   - API keys not in staged files
   - Black formatting for Python files in `ai_agents/agents/ten_packages/extension`

2. **commit-msg** - Validates commit message format:
   - Conventional commit type required (feat, fix, etc.)
   - Subject must be lowercase
   - No references to AI assistants
   - Body lines ≤100 characters

If the hooks are missing or need to be updated, create them with:

**Pre-commit hook (.git/hooks/pre-commit):**
```bash
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
# Pre-commit hook to prevent committing API keys and check formatting

# Check for common API key patterns in staged files
if git diff --cached --name-only | xargs grep -l -E "(API_KEY|api_key|HEYGEN_API_KEY|THYMIA_API_KEY|DEEPGRAM_API_KEY).*=.*[A-Za-z0-9]{20,}" 2>/dev/null; then
    echo "ERROR: Potential API key found in staged files!"
    echo "Files containing potential keys:"
    git diff --cached --name-only | xargs grep -l -E "(API_KEY|api_key).*=.*[A-Za-z0-9]{20,}" 2>/dev/null
    echo ""
    echo "Please remove API keys before committing."
    echo "Use environment variables or .env files instead."
    exit 1
fi

# Check for PERSISTENT_KEYS_CONFIG file
if git diff --cached --name-only | grep -i "PERSISTENT.*KEY.*CONFIG"; then
    echo "ERROR: Attempting to commit PERSISTENT_KEYS_CONFIG file!"
    echo "This file should never be committed to git."
    exit 1
fi

# Check black formatting for staged Python files in ai_agents/agents/ten_packages/extension
staged_py_files=$(git diff --cached --name-only --diff-filter=ACM | grep -E "^ai_agents/agents/ten_packages/extension/.*\.py$" | grep -v "third_party/\|http_server_python/\|ten_packages/system")

if [ -n "$staged_py_files" ]; then
    # Try to run black check via docker first, fall back to local
    if command -v docker &> /dev/null && docker ps -q -f name=ten_agent_dev &> /dev/null; then
        # Docker container is running
        unformatted=$(echo "$staged_py_files" | xargs -I {} sudo docker exec ten_agent_dev bash -c "cd /app && black --check --line-length 80 {} 2>&1" 2>/dev/null | grep "would reformat" || true)
    elif command -v black &> /dev/null; then
        # Use local black
        unformatted=$(echo "$staged_py_files" | xargs black --check --line-length 80 2>&1 | grep "would reformat" || true)
    else
        # No black available, skip check with warning
        echo "WARNING: black not available, skipping format check"
        unformatted=""
    fi

    if [ -n "$unformatted" ]; then
        echo "ERROR: Python files need black formatting!"
        echo "$unformatted"
        echo ""
        echo "Run: sudo docker exec ten_agent_dev bash -c 'cd /app && black --line-length 80 agents/ten_packages/extension'"
        echo "Or fix specific files shown above."
        exit 1
    fi
fi

exit 0
EOF

chmod +x .git/hooks/pre-commit
```

**Commit-msg hook (.git/hooks/commit-msg):**
```bash
cat > .git/hooks/commit-msg << 'EOF'
#!/bin/bash
# Git commit-msg hook for conventional commits validation

commit_msg_file=$1
commit_msg=$(cat "$commit_msg_file")
subject=$(head -n1 "$commit_msg_file")

# Check for Claude mentions
if echo "$commit_msg" | grep -iq "claude"; then
    echo "ERROR: Commit message contains 'claude'. Please remove references to Claude."
    echo "Rejected commit message:"
    echo "$commit_msg"
    exit 1
fi

# Valid conventional commit types
valid_types="^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\(.+\))?:"

# Check if subject starts with valid type
if ! echo "$subject" | grep -qE "$valid_types"; then
    echo "ERROR: Commit subject must start with a valid conventional commit type."
    echo "Valid types: feat, fix, docs, style, refactor, perf, test, build, ci, chore, revert"
    echo "Example: feat: add new feature"
    echo "Your subject: $subject"
    exit 1
fi

# Extract subject text after type (e.g., "feat: this is the subject" -> "this is the subject")
subject_text=$(echo "$subject" | sed -E 's/^[a-z]+(\([^)]+\))?:\s*//')

# Check if subject text starts with uppercase letter (sentence-case)
if echo "$subject_text" | grep -qE "^[A-Z]"; then
    echo "ERROR: Commit subject must be lowercase (no sentence-case, start-case, or upper-case)."
    echo "Your subject: $subject"
    echo "Should be: $(echo "$subject" | sed -E 's/^([a-z]+(\([^)]+\))?:\s*)([A-Z])/\1\l\3/')"
    exit 1
fi

exit 0
EOF

chmod +x .git/hooks/commit-msg
```

**Testing the git hooks:**
```bash
# Test commit-msg hook
echo "fix: test commit message" > /tmp/test_msg.txt
bash .git/hooks/commit-msg /tmp/test_msg.txt && echo "Valid" || echo "Invalid"

# Test with invalid uppercase (should fail)
echo "fix: Test commit message" > /tmp/test_msg.txt
bash .git/hooks/commit-msg /tmp/test_msg.txt && echo "Valid" || echo "Invalid"

# The hooks will automatically run on every commit
```

**Note:** Hooks must be set up in each clone of the repository, as `.git/hooks/` is not tracked by git.

---

## Additional Resources

- [TEN Framework Documentation](https://doc.theten.ai)
- Python dependencies: `aiohttp`, `pydantic`, `websockets`
- Agora RTC: Audio/video streaming
- LLM tool pattern: `AsyncLLMToolBaseExtension`

---

**Pro Tip**: Always check the logs first. Most issues show clear error messages in `/tmp/task_run.log` or session logs.
