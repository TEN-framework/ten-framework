# Operations and Restarts

> **When to Read This:** Load this document when you need to restart services,
> debug port conflicts, recover from crashes, or clean up zombie processes.

## When to Do a Full Restart

| What Changed                    | Action                                               |
| ------------------------------- | ---------------------------------------------------- |
| `property.json` (graphs added)  | Full restart (frontend caches graph list)            |
| `property.json` (config only)   | No restart needed (loaded per session)               |
| `.env`                          | `docker compose down && docker compose up -d` + deps |
| Python code                     | Restart server only                                  |
| Go code                         | `task install` then restart server                   |
| Container restart               | Reinstall Python deps, then `task run`               |

## Full Restart Procedure

Must kill `next-server` too — it holds port 3000 even after `node`/`bun` die:

```bash
# 1. Kill EVERYTHING
sudo docker exec ten_agent_dev bash -c \
  "pkill -9 -f 'bin/api'; pkill -9 -f bun; pkill -9 -f node; \
   pkill -9 -f next-server; pkill -9 -f tman"

# 2. Clean up stale files
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"

# 3. Wait for port 3000 TIME_WAIT to clear
# If Next.js can't bind port 3000, it silently starts on 3001/3002
# which isn't exposed by Docker — the frontend appears down.
sleep 30

# 4. Start
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/<example> && task run > /tmp/task_run.log 2>&1"

# 5. Verify (wait ~12s for startup)
sleep 12
sudo docker exec ten_agent_dev bash -c \
  "curl -s http://localhost:8080/health && \
   curl -s -o /dev/null -w ' Frontend:%{http_code}' http://localhost:3000/"
```

## Verification

Check Next.js started on port 3000 (not 3001+):

```bash
sudo docker exec ten_agent_dev bash -c \
  "strings /tmp/task_run.log | grep -E 'Local:|Port|Ready|Error'"
```

Expected output:
```
   - Local:         http://localhost:3000
 Ready in 2.1s
```

If you see `Port 3000 is in use`, the frontend is on the wrong port.

## Zombie Worker Cleanup

In the standard `ai_agents` Docker workflow, worker processes are spawned from the
runtime inside the container and can survive API/frontend restarts. If your team
launches TEN differently, confirm the process boundary first, then inspect the
correct namespace:

```bash
# Check for zombies
sudo docker exec ten_agent_dev bash -c \
  "ps aux | grep 'bin/worker --property' | grep -v grep"

# Kill them
sudo docker exec ten_agent_dev bash -c \
  "pkill -9 -f 'bin/worker --property'"
```

Always kill zombies before restarting the server.

## Stale Lock Cleanup

After crashes, `.next/dev/lock` becomes stale:

```bash
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"
```

Also clear the Next.js cache if React version errors appear:

```bash
sudo docker exec ten_agent_dev bash -c "rm -rf /app/playground/.next"
```

## Port 3000 Conflict Debugging

If Next.js reports "Port 3000 is in use", find the process holding it:

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

Kill the PID, wait for TIME_WAIT to clear (~30s), then restart.

If no PID is found but port is still busy, it's in TIME_WAIT state. Check:

```bash
sudo docker exec ten_agent_dev bash -c \
  "cat /proc/net/tcp6 | grep ':0BB8'"
```

State `06` = TIME_WAIT. Wait 30-60 seconds for it to clear.

## .env and Container Restart Recovery

`.env` is loaded at container startup only. After editing:

```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d
```

Then reinstall everything (Python deps are not persisted):

```bash
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/<example> && task install"
```

## Copying Extension Code to Running Container

When iterating on extension code locally:

```bash
# Option 1: docker cp with /. suffix (avoids nested dirs)
sudo docker cp ./agents/ten_packages/extension/my_ext/. \
  ten_agent_dev:/app/agents/ten_packages/extension/my_ext/

# Option 2: tar with cache exclusion (recommended — avoids
# __pycache__ and .pytest_cache causing import errors)
tar --exclude='__pycache__' --exclude='.pytest_cache' \
  -C ai_agents/agents/ten_packages/extension/my_ext -cf - . | \
  sudo docker exec -i ten_agent_dev tar \
  -C /app/agents/ten_packages/extension/my_ext -xf -

# Verify symlink exists in the example's tenapp
sudo docker exec ten_agent_dev bash -c \
  "ls -la /app/agents/examples/<example>/tenapp/ten_packages/extension/my_ext"

# If missing, create it manually
sudo docker exec ten_agent_dev bash -c \
  "ln -sf /app/agents/ten_packages/extension/my_ext \
   /app/agents/examples/<example>/tenapp/ten_packages/extension/my_ext"
```

Then do a full restart.

**Common pitfall:** If `docker cp` copies `__pycache__` or `.pytest_cache`
from your local machine into the container, it can cause `ImportError` or
stale bytecode during test collection. Use the tar method above or clean
the container directory before copying:

```bash
sudo docker exec ten_agent_dev bash -c \
  "find /app/agents/ten_packages/extension/my_ext \
   -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null; \
   find /app/agents/ten_packages/extension/my_ext \
   -type d -name .pytest_cache -exec rm -rf {} + 2>/dev/null"
```

## After Container Restart Checklist

1. Reinstall Python dependencies
2. Rebuild Go binary (`task install`)
3. Kill any zombie workers
4. Remove stale `.next/dev/lock`
5. Start with `task run`
6. Verify health endpoint and frontend status code

## See Also

- [Back to Gotchas](../07_gotchas.md)
- [Back to Workflows](../05_workflows.md)
- [Deployment](deployment.md) — Production setup, Cloudflare, Nginx
- [Server Architecture](server_architecture.md) — Worker lifecycle
