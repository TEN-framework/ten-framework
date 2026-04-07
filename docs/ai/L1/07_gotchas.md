# 07 Gotchas

> Critical pitfalls, tribal knowledge, and troubleshooting.

## CRITICAL: Property Getters Return Tuples

All `get_property_*()` methods return `(value, error_or_none)`, not the raw value.

```python
# WRONG — causes TypeError
threshold = await ten_env.get_property_float("threshold")
if threshold > 0.5:  # TypeError: '>' not supported between 'float' and 'tuple'

# CORRECT — extract from tuple
threshold_result = await ten_env.get_property_float("threshold")
threshold = threshold_result[0] if isinstance(threshold_result, tuple) else threshold_result
```

This applies to `get_property_string()`, `get_property_int()`, `get_property_float()`,
`get_property_bool()`. Always extract `[0]`.

## CRITICAL: Signal Handlers Forbidden

Extensions run in worker threads. Signal handlers only work in the main thread.

```python
# WRONG — raises ValueError: signal only works in main thread
signal.signal(signal.SIGTERM, handler)
atexit.register(cleanup)

# CORRECT — use extension lifecycle
async def on_stop(self, ten_env):
    await self.cleanup()
```

## CRITICAL: Always Use `task run`

Never start the server with `./bin/api` or `./bin/main` directly.
`task run` sets the correct PYTHONPATH and starts all services together
(API server + playground + TMAN Designer).

## Zombie Worker Processes

Worker processes (`bin/main`) run on the **host machine**, not inside Docker.
They survive container restarts and server restarts.

```bash
# Check for zombies
ps -elf | grep 'bin/main' | grep -v grep

# Kill them
ps -elf | grep 'bin/main' | grep -v grep | awk '{print $4}' | xargs -r sudo kill -9
```

Always kill zombies before restarting the server.

## .env Loaded at Container Startup Only

Editing `.env` while the container is running has **no effect**. You must:

```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d
# Then reinstall Python deps and task run
```

## Node.js Version for Playground

Playground requires Node.js >= 20.9.0. The host machine may have an older version.
Always run playground from **inside the container** (has Node 22):

```bash
# WRONG: running from host with Node 18
cd playground && npm run dev  # Fails

# CORRECT: task run starts playground inside container automatically
```

## Next.js Lock File

After crashes, `.next/dev/lock` becomes stale, preventing restart:

```bash
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"
```

Always use nuclear restart after playground crashes.

## Python Deps Not Persisted

Python dependencies are installed into the container's filesystem and are lost
on container restart. Always reinstall after `docker compose down && up`:

```bash
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && bash scripts/install_python_deps.sh"
```

## tman Install Creates Symlinks

Never manually create symlinks with `ln -s` for extensions.
Always use `tman install` which resolves dependencies and creates correct links:

```bash
docker exec ten_agent_dev bash -c "cd /app/agents/examples/<example>/tenapp && tman install"
```

**Important:** If `tman install` doesn't create a symlink for a new extension (e.g., after
adding it to `manifest.json`), create it manually as a fallback:

```bash
sudo docker exec ten_agent_dev bash -c \
  "ln -sf /app/agents/ten_packages/extension/my_ext \
   /app/agents/examples/<example>/tenapp/ten_packages/extension/my_ext"
```

## docker cp Creates Nested Directories

When using `docker cp` to update extension code in the container, beware of
trailing slashes creating nested directories:

```bash
# WRONG — creates /app/.../deepgram_tts/deepgram_tts/ (nested)
sudo docker cp ./deepgram_tts/ container:/app/.../deepgram_tts/

# CORRECT — copy contents into existing directory
sudo docker cp ./deepgram_tts/. container:/app/.../deepgram_tts/
```

If you see `ModuleNotFoundError: No module named 'ten_packages.extension.X'`
after a `docker cp`, check for nested directories inside the extension folder.

## tman install Can Wipe bin/main

Running `tman install` when system dependencies have newer versions will replace
the runtime packages, which **deletes `bin/main`**. You must run the full
`task install` (not just `tman install`) to rebuild it:

```bash
# This alone can break things if runtime versions changed:
docker exec ten_agent_dev bash -c "cd /app/.../tenapp && tman install"

# This is safe — rebuilds bin/main after tman install:
docker exec ten_agent_dev bash -c "cd /app/agents/examples/<example> && task install"
```

Signs: Worker fails with `bin/main: No such file or directory` in logs.

## Audio Routing: Split at Source Only

When routing audio to multiple destinations, the split must happen at the
source node (e.g., `agora_rtc`), not at intermediate nodes. Splitting from
intermediate nodes can cause crashes.

```json
// CORRECT: agora_rtc sends pcm_frame to both stt AND vad
{"extension": "agora_rtc", "audio_frame": [
  {"name": "pcm_frame", "dest": [{"extension": "stt"}, {"extension": "vad"}]}
]}
```

## Frontend Caches Graph List

The playground caches the `/graphs` API response. When adding or removing graphs
from `property.json`, a nuclear restart is required — simple server restart
is not enough.

## Manifest Module Name Must Match

The `name` field in extension `manifest.json` must exactly match the `addon`
field used in graph nodes in `property.json`. Mismatches cause silent failures.

## Apple Silicon Docker

Docker containers may need Rosetta for x86 images on Apple Silicon Macs.
Enable in Docker Desktop: Settings → General → Use Rosetta for x86_64/amd64 emulation.

## Windows Line Endings

Before cloning on Windows, configure git to preserve Unix line endings:

```bash
git config --global core.autocrlf false
```

## Nuclear Restart Recipe

When in doubt, use the nuclear option. **Must kill `next-server` too** — it
holds port 3000 even after its parent `node` process is killed:

```bash
# 1. Kill EVERYTHING (including next-server which holds port 3000)
sudo docker exec ten_agent_dev bash -c \
  "pkill -9 -f 'bin/api'; pkill -9 -f bun; pkill -9 -f node; pkill -9 -f next-server; pkill -9 -f tman"

# 2. Clean up stale files
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"

# 3. Wait for port 3000 TIME_WAIT to clear (critical!)
# If Next.js can't bind port 3000, it silently starts on 3001/3002 which
# isn't exposed by Docker — the frontend appears down.
sleep 30  # or check: docker exec ten_agent_dev bash -c "cat /proc/net/tcp6 | grep ':0BB8' | wc -l"

# 4. Start
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant && task run > /tmp/task_run.log 2>&1"

# 5. Verify (wait ~12s for startup)
sleep 12
sudo docker exec ten_agent_dev bash -c \
  "curl -s http://localhost:8080/health && curl -s -o /dev/null -w ' Frontend:%{http_code}' http://localhost:3000/"
```

**Verify the logs** — check Next.js started on port 3000 (not 3001+):
```bash
sudo docker exec ten_agent_dev bash -c "strings /tmp/task_run.log | grep -E 'Local:|Port|Ready|Error'"
```

If you see `Port 3000 is in use`, find and kill the process holding it:
```bash
sudo docker exec ten_agent_dev bash -c \
  "for pid in /proc/[0-9]*/fd/*; do \
    link=\$(readlink \$pid 2>/dev/null); \
    echo \"\$link\" | grep -q socket: && \
    inode=\$(echo \$link | grep -oP '\\d+') && \
    grep -q \$inode /proc/net/tcp6 2>/dev/null && \
    grep \$inode /proc/net/tcp6 | grep -q ':0BB8' && \
    echo PID=\$(echo \$pid | cut -d/ -f3) && break; \
  done"
```

## Related Deep Dives

- [Deployment](deep_dives/deployment.md) — Production setup, persistent startup
- [Server Architecture](deep_dives/server_architecture.md) — Worker lifecycle, session management
