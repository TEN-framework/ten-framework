# TEN Framework Quick Reference - voice-assistant-advanced

**Target**: `voice-assistant-advanced` example | **Last Updated**: 2025-12-12

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
docker compose restart ten_agent_dev
# Then reinstall Python deps and start server (see Section 4)
```

### 3. Always Use `task run` - NEVER `./bin/api` or `./bin/main` Directly

### 4. Python Dependencies Don't Persist After Container Restart

```bash
docker exec ten_agent_dev bash -c \
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
| All extension logs | `/tmp/task_run.log` | `docker exec ten_agent_dev tail -f /tmp/task_run.log` |
| Playground logs | `/tmp/task_run.log` | Same file (task run combines them) |
| Filter by channel | - | `... \| grep --line-buffered "channel_name"` |
| Filter errors | - | `... \| grep -E "(ERROR\|Traceback)"` |

---

## Common Errors - Quick Fixes

| Error | One-Line Fix |
|-------|-------------|
| "No graphs available" in playground | Nuclear restart (see Section 1) |
| "502 Bad Gateway" | `docker exec -d ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant-advanced && task run > /tmp/task_run.log 2>&1"` |
| Lock file error | `sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"` then nuclear restart |
| Port 3000/8080 in use | `sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"` |
| ModuleNotFoundError | Reinstall Python deps (see Section 4 above) |
| "Environment variable X not found" | Container restart required (see Section 2 above) |

---

## Full Startup After Container Restart

```bash
# 1. Reinstall Python dependencies
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

# 2. Start everything with task run
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"

# 3. Wait and verify
sleep 15
curl -s http://localhost:8080/health
curl -s http://localhost:8080/graphs | jq -r '.data[].name'
```

---

## Cloudflare Tunnel (Quick HTTPS Access)

```bash
pkill cloudflared
nohup cloudflared tunnel --url http://localhost:3000 > /tmp/cloudflare_tunnel.log 2>&1 &
sleep 5
grep -o 'https://[^[:space:]]*\.trycloudflare\.com' /tmp/cloudflare_tunnel.log | head -1
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

---

## Zombie Worker Cleanup

If old sessions persist after restart:

```bash
# Check for zombie workers (run on HOST, not in container)
ps -elf | grep 'bin/main' | grep -v grep

# Kill them
ps -elf | grep 'bin/main' | grep -v grep | awk '{print $4}' | xargs -r sudo kill -9
```

---

**For detailed explanations, troubleshooting, and extension development, see [AI_working_with_ten.md](./AI_working_with_ten.md)**
