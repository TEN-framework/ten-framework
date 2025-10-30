# Logging Investigation Plan - TEN Framework

**Date**: 2025-10-30
**Status**: Thymia working ✅ but logs not visible ❌

---

## Current Situation

### What's Working
- ✅ Thymia extension produces wellness results
- ✅ Voice assistant functionality works end-to-end
- ✅ Server API responds correctly
- ✅ Extensions load and execute

### What's Broken
- ❌ Worker log files are 0 bytes (`app-{channel}-*.log`)
- ❌ Extension logs don't appear in `/tmp/task_run.log`
- ❌ `Log2Stdout:true` has no effect
- ❌ Python `print()` and `ten_env.log_info()` produce no output

---

## Code Analysis

### Go Server (worker.go)
**Lines 145-174**: Correctly sets up stdout/stderr capture
```go
if w.Log2Stdout {
    stdoutWriter = os.Stdout
    stderrWriter = os.Stderr
} else {
    logFile, err = os.OpenFile(w.LogFile, ...)
    stdoutWriter = logFile
    stderrWriter = logFile
}
cmd.Stdout = stdoutPrefixWriter
cmd.Stderr = stderrPrefixWriter
```

**Conclusion**: Go server code is correct. Problem is downstream.

### Worker Process Chain
```
Go Server → sh -c "tman run start --property ..."
         → tman (wrapper)
         → bin/main (actual app)
         → Python extensions (embedded via ten_runtime_python)
```

**Hypothesis**: `tman` or `bin/main` is not outputting anything to stdout/stderr.

---

## Investigation Steps

### Phase 1: Understand the Logging Infrastructure (30 min)

#### 1.1 Find TEN Framework Logging Configuration
```bash
# Search for log level/verbosity settings
docker exec ten_agent_dev find /app/agents -name "*.json" -o -name "*.yaml" | xargs grep -l "log"

# Check if there's a TEN_LOG_LEVEL environment variable
docker exec ten_agent_dev env | grep -i log

# Look for logging configuration in manifest files
docker exec ten_agent_dev find /app/agents/examples/voice-assistant-advanced/tenapp -name "manifest.json" | xargs cat | grep -i log
```

#### 1.2 Examine tman Behavior
```bash
# Find tman executable
docker exec ten_agent_dev which tman
docker exec ten_agent_dev file /usr/local/bin/tman

# Check if tman has help/verbose flags
docker exec ten_agent_dev tman --help 2>&1 | head -50
docker exec ten_agent_dev tman run --help 2>&1 | head -50

# Try running tman with verbose flags if available
docker exec ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant-advanced/tenapp && tman run start --verbose --help 2>&1 || tman run start --help 2>&1"
```

#### 1.3 Examine bin/main Directly
```bash
# Check what bin/main is
docker exec ten_agent_dev file /app/agents/examples/voice-assistant-advanced/tenapp/bin/main

# Try running it directly with verbose flags
docker exec ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant-advanced/tenapp && ./bin/main --help 2>&1"

# Look for environment variables it might use
docker exec ten_agent_dev strings /app/agents/examples/voice-assistant-advanced/tenapp/bin/main | grep -E "TEN_|LOG_|DEBUG|VERBOSE" | head -30
```

---

### Phase 2: Test Log Output Directly (20 min)

#### 2.1 Create Minimal Test Script
Create `/tmp/test_worker_logs.sh`:
```bash
#!/bin/bash
# Test if worker process produces ANY output

cd /app/agents/examples/voice-assistant-advanced/tenapp

# Test 1: Direct execution with timeout
echo "=== Test 1: Running bin/main directly ==="
timeout 5s ./bin/main --property=/tmp/ten_agent/property-test.json 2>&1 | head -20 || echo "Process exited with code $?"

# Test 2: Through tman
echo "=== Test 2: Running through tman ==="
timeout 5s tman run start --property=/tmp/ten_agent/property-test.json 2>&1 | head -20 || echo "Process exited with code $?"

# Test 3: Check if Python is embedded
echo "=== Test 3: Check Python embedding ==="
ldd ./bin/main | grep python
```

```bash
docker exec ten_agent_dev bash -c "cat > /tmp/test_worker_logs.sh" < test_script
docker exec ten_agent_dev bash -c "chmod +x /tmp/test_worker_logs.sh && /tmp/test_worker_logs.sh"
```

#### 2.2 Test with Strace
```bash
# Capture syscalls to see if write() calls are made
docker exec ten_agent_dev bash -c "cd /app/agents/examples/voice-assistant-advanced/tenapp && timeout 3s strace -e write,openat -f ./bin/main 2>&1 | grep -E '(write|stdout|stderr)' | head -30"
```

#### 2.3 Test Python Output Directly
```bash
# Create a test extension that just prints
docker exec ten_agent_dev bash -c "python3 -c 'import sys; print(\"STDOUT TEST\", flush=True); sys.stderr.write(\"STDERR TEST\n\"); sys.stderr.flush()'"
```

---

### Phase 3: Check Property.json Configuration (15 min)

#### 3.1 Inspect Log Settings in property.json
```bash
# Search for any log-related configuration
docker exec ten_agent_dev bash -c "cat /app/agents/examples/voice-assistant-advanced/tenapp/property.json" | jq . | grep -i -C 3 "log"

# Check addon configurations
docker exec ten_agent_dev bash -c "cat /app/agents/examples/voice-assistant-advanced/tenapp/property.json" | jq '.predefined_graphs[0].graph.nodes[] | select(.type == "extension") | {name: .name, addon: .addon, property: .property}'
```

#### 3.2 Check Extension Manifests
```bash
# Find all manifest.json files
docker exec ten_agent_dev find /app/agents/ten_packages -name "manifest.json" -type f

# Check for log_level or similar settings
docker exec ten_agent_dev find /app/agents/ten_packages -name "manifest.json" -exec grep -l "log" {} \;
```

---

### Phase 4: Test Environment Variables (10 min)

#### 4.1 Try Different Log Level Settings
```bash
# Stop current server
docker exec ten_agent_dev bash -c "pkill -f 'task run'"

# Test with different environment variables
docker exec -d ten_agent_dev bash -c "
  export TEN_LOG_LEVEL=DEBUG
  export LOG_LEVEL=DEBUG
  export PYTHONUNBUFFERED=1
  export TEN_LOG_TO_STDOUT=1
  cd /app/agents/examples/voice-assistant-advanced &&
  task run 2>&1 | tee /tmp/task_run.log
"

# Wait and check
sleep 5
docker exec ten_agent_dev tail -50 /tmp/task_run.log
```

#### 4.2 Check .env File Settings
```bash
# Check both .env files for log-related settings
docker exec ten_agent_dev cat /app/.env | grep -i log
docker exec ten_agent_dev cat /app/agents/examples/voice-assistant-advanced/tenapp/.env 2>/dev/null | grep -i log || echo "No tenapp/.env"
```

---

### Phase 5: Make Logs Visible on Host (15 min)

#### 5.1 Mount Volume for Logs
Edit `/home/ubuntu/ten-framework/ai_agents/docker-compose.yml`:
```yaml
volumes:
  - ../core:/app/core
  - ../agents:/app/agents
  - /tmp/ten_logs:/tmp/ten_agent  # ADD THIS LINE
```

Then restart:
```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d
```

#### 5.2 Alternative: Use docker cp in Monitoring Script
Update `/home/ubuntu/ten-framework/ai/monitor_logs.sh`:
```bash
#!/bin/bash
# Auto-refresh logs from container

while true; do
    docker cp ten_agent_dev:/tmp/task_run.log /tmp/ten_task_run.log 2>/dev/null
    docker cp ten_agent_dev:/tmp/ten_agent /tmp/ten_logs/ 2>/dev/null
    sleep 2
done &

MONITOR_PID=$!
echo "Background sync started (PID: $MONITOR_PID)"

tail -f /tmp/ten_task_run.log
```

#### 5.3 Create Real-time Sync Script
```bash
# /home/ubuntu/ten-framework/ai/sync_logs.sh
#!/bin/bash
docker exec ten_agent_dev tail -f /tmp/task_run.log 2>&1 | tee /tmp/ten_task_run_live.log
```

---

### Phase 6: Deep Dive into TEN Runtime (if needed) (30 min)

#### 6.1 Find TEN Runtime Python Library
```bash
# Locate ten_runtime_python
docker exec ten_agent_dev find /app -name "ten_runtime*" -type d

# Check its logging configuration
docker exec ten_agent_dev find /app/agents/ten_packages/system/ten_runtime_python -name "*.py" | xargs grep -l "log"
```

#### 6.2 Check TEN Framework Source
```bash
# Look for logging configuration in TEN C++ code
docker exec ten_agent_dev find /app -name "*.h" -o -name "*.cpp" | xargs grep -l "LOG_LEVEL" | head -10

# Check if there's a config file
docker exec ten_agent_dev find /app -name "ten_runtime.conf" -o -name "*.ini" -o -name "*.cfg"
```

---

## Success Criteria

After completing this investigation, we should be able to:

1. ✅ **See Python extension logs in real-time** from host machine
2. ✅ **Understand why logs are not appearing** in worker log files
3. ✅ **Know what log level settings exist** and how to control them
4. ✅ **Have working monitoring tools** on the host machine
5. ✅ **Document the proper way** to enable verbose logging

---

## Expected Outcomes

### Best Case
- Find a TEN_LOG_LEVEL or similar environment variable
- Set it to DEBUG/VERBOSE
- Logs start appearing in expected locations

### Most Likely Case
- TEN framework has its own logging system separate from stdout
- Need to enable it via config file or environment variable
- Logs may be going to a different location

### Worst Case
- TEN framework binary doesn't output logs by default
- Need to recompile with logging enabled
- Or use alternative debugging methods (file-based logging in extensions)

---

## Notes

- The Go server code (worker.go) is correctly implemented
- The issue is with `tman` or `bin/main` not producing output
- Python extensions ARE running (Thymia works!)
- The logging is being suppressed somewhere in the TEN runtime

---

## Next Steps After Investigation

Once we understand the logging system:
1. Update AI_working_with_ten_compact.md with correct logging instructions
2. Create permanent monitoring scripts
3. Set up proper log level configuration
4. Add volume mount for persistent log access from host
