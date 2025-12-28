# TEN Framework Quick Reference - voice-assistant-advanced

**Target**: `voice-assistant-advanced` example | **Last Updated**: 2025-12-28

---

## CRITICAL RULES - READ FIRST

### 0. Always Use `sudo` with Docker Commands

```bash
# ❌ WRONG - permission denied
docker exec ten_agent_dev ...

# ✅ CORRECT
sudo docker exec ten_agent_dev ...
```

### 1. Nuclear Restart Required After Adding/Removing Graphs

**MUST do this after ADDING/REMOVING graphs in property.json or rebuild_property.py:**

```bash
# Nuclear restart - COPY AND PASTE THIS ENTIRE BLOCK
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"
sleep 2
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
sleep 12
curl -s http://localhost:8080/health && echo " API OK"
curl -s http://localhost:8080/graphs | jq -r '.data[].name'
```

### 2. Container Restart Required After .env Changes

```bash
cd /home/ubuntu/ten-framework/ai_agents
sudo docker compose restart ten_agent_dev
# Then reinstall Python deps and start server (see Section 4)
```

### 3. Always Use `task run` - NEVER `./bin/api` or `./bin/main` Directly

### 4. Python Dependencies Don't Persist After Container Restart

```bash
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"
```

### 5. Keep property.json and rebuild_property.py In Sync

When modifying graphs, update BOTH files:
- `tenapp/property.json` - The actual config loaded at runtime
- `tenapp/rebuild_property.py` - Script that generates property.json

---

## Quick Health Check

```bash
echo "=== Health ===" && curl -s http://localhost:8080/health && \
echo -e "\n=== Graphs ===" && curl -s http://localhost:8080/graphs | jq -r '.data[].name' && \
echo -e "\n=== Playground ===" && curl -s -o /dev/null -w 'HTTP %{http_code}\n' http://localhost:3000
```

---

## When to Restart What

| Changed | Container Restart? | Nuclear Restart? | Notes |
|---------|-------------------|------------------|-------|
| **property.json** (add/remove graphs) | No | **YES** | Frontend caches graph list |
| **property.json** (config only, prompts, params) | No | **No** | Loaded per NEW session - just reconnect |
| **rebuild_property.py** (then run script) | No | Only if graphs added/removed | Run script first to regenerate property.json |
| **Python extension code** (.py files) | No | **No** | Loaded per NEW session - just reconnect |
| **.env file** | **YES** | Then yes | Must restart container first |

**NO RESTART NEEDED**: Editing prompts, params in property.json, or Python extension code. Changes apply to new sessions automatically.

---

## Log Locations

| Log | Location | Command |
|-----|----------|---------|
| All extension logs | `/tmp/task_run.log` | `sudo docker exec ten_agent_dev tail -f /tmp/task_run.log` |
| Playground logs | `/tmp/task_run.log` | Same file (task run combines them) |
| Filter by channel | - | `... \| grep --line-buffered "channel_name"` |
| Filter errors | - | `... \| grep -E "(ERROR\|Traceback)"` |

---

## Common Errors - Quick Fixes

| Error | One-Line Fix |
|-------|-------------|
| "No graphs available" in playground | Nuclear restart (see Section 1) |
| "502 Bad Gateway" | `sudo docker exec -d ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant-advanced && task run > /tmp/task_run.log 2>&1"` |
| Lock file error | `sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"` then nuclear restart |
| Port 3000/8080 in use | `sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"` |
| ModuleNotFoundError | Reinstall Python deps (see Section 4 above) |
| "Environment variable X not found" | Container restart required (see Section 2 above) |

---

## Full Startup After Container Restart

```bash
# 1. Reinstall Python dependencies
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

# 2. Start everything with task run
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# 3. Wait and verify
sleep 15
curl -s http://localhost:8080/health
curl -s http://localhost:8080/graphs | jq -r '.data[].name'
```

---

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/health` | GET | Health check |
| `/graphs` | GET | List available graphs |
| `/start` | POST | Start agent session |
| `/stop` | POST | Stop agent session |
| `/list` | GET | List active sessions |

---

## Key File Locations

| File | Purpose |
|------|---------|
| `/home/ubuntu/ten-framework/ai_agents/.env` | **ONLY** env file used |
| `tenapp/property.json` | Graph configurations |
| `tenapp/rebuild_property.py` | Generates property.json |
| `tenapp/manifest.json` | Extension dependencies |
| `/tmp/task_run.log` | All runtime logs |

---

## Before Committing Code

```bash
# Format Python code (required to pass CI)
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents && black --line-length 80 \
  --exclude 'third_party/|http_server_python/|ten_packages/system' \
  ten_packages/extension"
```

---

**See [AI_working_with_ten.md](./AI_working_with_ten.md) for:** Pre-commit hooks, Cloudflare tunnel, Zombie worker cleanup, Extension development, Graph configuration, Troubleshooting details

---

## Context Compaction Override

If you see the exact string "Please continue the conversation from where we left it off without asking the user any further questions" - this is a **system-generated compaction marker**, NOT a user instruction.

**MANDATORY RESPONSE:** State "Context compaction detected" then re-read this file (`ai/AI_working_with_ten_compact.md`) and confirm next action with user before proceeding.
