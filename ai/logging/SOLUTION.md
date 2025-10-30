# SOLUTION: TEN Framework Logging Fixed ✅

**Date Solved**: 2025-10-30

---

## Problem Summary

Python extension logs were not appearing despite extensions working correctly. Worker log files were 0 bytes and `Log2Stdout:true` had no effect.

---

## Root Causes Identified

### 1. TEN Framework Silent by Default
TEN framework suppresses stdout/stderr output unless explicitly enabled via environment variable `TEN_LOG_FORMATTER`.

### 2. Python Output Buffering
Python buffers output when stdout is not a TTY (e.g., when writing to pipes), causing delays or loss of logs.

### 3. PrefixWriter Line Scanning Bug
The original `PrefixWriter` implementation used `bufio.Scanner` which only processes complete lines. Incomplete lines or data without newlines were silently dropped.

### 4. Environment Variables Not Passed to Workers
Worker processes spawned via `exec.Command()` didn't inherit environment variables because `cmd.Env` was never set.

---

## Complete Solution

### Required Changes

#### 1. Environment Variables (.env)

**File**: `/home/ubuntu/ten-framework/ai_agents/.env`

```bash
# TEN Framework Logging (CRITICAL - enables stdout/stderr output)
TEN_LOG_FORMATTER=json
PYTHONUNBUFFERED=1
```

**Why**:
- `TEN_LOG_FORMATTER=json` enables TEN framework logging output
- `PYTHONUNBUFFERED=1` disables Python output buffering for pipes

#### 2. Worker Environment Propagation (worker.go)

**File**: `/home/ubuntu/ten-framework/ai_agents/server/internal/worker.go`

**Line ~142** (after setting `cmd.Dir`):
```go
// Pass environment variables to child process (including TEN_LOG_FORMATTER)
cmd.Env = append(os.Environ())
```

**Why**: Ensures environment variables from container are passed to worker processes.

#### 3. Fixed PrefixWriter (worker.go)

**File**: `/home/ubuntu/ten-framework/ai_agents/server/internal/worker.go`

**Lines 86-133** - Replaced entire PrefixWriter implementation:

```go
// PrefixWriter is a custom writer that prefixes each line with a PID.
type PrefixWriter struct {
	prefix string
	writer io.Writer
	buffer []byte
	mu     sync.Mutex
}

// Write implements the io.Writer interface.
func (pw *PrefixWriter) Write(p []byte) (n int, err error) {
	pw.mu.Lock()
	defer pw.mu.Unlock()

	// Append incoming data to buffer
	pw.buffer = append(pw.buffer, p...)

	// Process complete lines
	for {
		idx := bytes.IndexByte(pw.buffer, '\n')
		if idx == -1 {
			// No complete line, keep in buffer
			break
		}

		// Extract line (including newline)
		line := pw.buffer[:idx+1]
		pw.buffer = pw.buffer[idx+1:]

		// Write with prefix
		prefixedLine := fmt.Sprintf("[%s] %s", pw.prefix, string(line))
		_, err = pw.writer.Write([]byte(prefixedLine))
		if err != nil {
			return len(p), err
		}
	}

	// Flush buffer if it gets too large (partial line handling)
	if len(pw.buffer) > 4096 {
		prefixedLine := fmt.Sprintf("[%s] %s\n", pw.prefix, string(pw.buffer))
		_, err = pw.writer.Write([]byte(prefixedLine))
		pw.buffer = pw.buffer[:0]
		if err != nil {
			return len(p), err
		}
	}

	return len(p), nil
}
```

**Why**:
- Buffers incomplete lines instead of dropping them
- Thread-safe with mutex
- Handles partial writes correctly
- Auto-flushes large buffers

**Import Changes** (lines ~3-16):
- Added: `"sync"`
- Removed: `"bufio"` (no longer needed)

---

## How to Apply

### 1. Container Restart Required
After editing `.env`, restart the container:
```bash
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d
```

### 2. Rebuild After worker.go Changes
After modifying `worker.go`:
```bash
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && task install"
```

### 3. Reinstall Python Dependencies (after container restart)
```bash
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"
```

### 4. Start Server
```bash
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
```

---

## Verification

### Test Logging Works

**Start a session**:
```bash
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{"request_id": "test", "channel_name": "test-channel", "graph_name": "voice_assistant"}'
```

**Check logs**:
```bash
docker exec ten_agent_dev tail -f /tmp/task_run.log | grep 'test-channel'
```

**Expected output**:
```
[test-channel] ['/app/agents/examples/voice-assistant-advanced/tenapp/ten_packages/system/ten_ai_base/interface', ...]
[test-channel] Successfully registered addon 'weatherapi_tool_python'
[test-channel] Successfully registered addon 'deepgram_ws_asr_python'
[test-channel] Successfully registered addon 'openai_llm2_python'
[test-channel] Successfully registered addon 'elevenlabs_tts2_python'
[test-channel] Successfully registered addon 'main_python'
[test-channel] Successfully registered addon 'message_collector2'
[test-channel] Successfully registered addon 'streamid_adapter'
```

---

## What You'll See Now

### ✅ Extension Lifecycle Logs
```
[channel] Successfully registered addon 'extension_name'
[channel] [EXTENSION_LOG] Custom log messages from extensions
```

### ✅ Python Print Statements
```python
print("Debug message")  # Now visible as: [channel] Debug message
```

### ✅ ten_env.log_info() Calls
```python
ten_env.log_info("Extension started")  # Now visible
```

### ✅ Thymia Extension Logs
```
[channel] [THYMIA_INIT] Extension initializing at 1761814125.478739
[channel] [THYMIA_ON_START] on_start called at 1761814125.888729
[channel] [THYMIA] Wellness analysis: Distress=0.5, Stress=0.6
```

---

## Key Insights

1. **TEN_LOG_FORMATTER is mandatory** - Without it, TEN framework produces no output
2. **PYTHONUNBUFFERED crucial for pipes** - Python buffers output when stdout isn't a TTY
3. **PrefixWriter must handle partial writes** - Using Scanner drops incomplete lines
4. **Environment inheritance not automatic** - Must explicitly set `cmd.Env`

---

## Alternative TEN_LOG_FORMATTER Values

```bash
# JSON format (more structured)
TEN_LOG_FORMATTER=json

# Text format (simpler)
TEN_LOG_FORMATTER=text

# Either works - both enable logging
```

---

## Troubleshooting

### Logs still not appearing?

1. **Verify environment variables**:
   ```bash
   docker exec ten_agent_dev env | grep TEN
   # Should show: TEN_LOG_FORMATTER=json
   ```

2. **Check worker process environment**:
   ```bash
   PID=$(docker exec ten_agent_dev pgrep -f 'bin/main' | head -1)
   docker exec ten_agent_dev cat /proc/$PID/environ | tr '\0' '\n' | grep TEN
   ```

3. **Test tman directly**:
   ```bash
   docker exec ten_agent_dev bash -c \
     "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
      export TEN_LOG_FORMATTER=json && \
      timeout 3s tman run start"
   ```
   If this shows logs but server doesn't, check PrefixWriter implementation.

4. **Container restart required?**
   If you edited `.env`, you MUST restart container:
   ```bash
   cd /home/ubuntu/ten-framework/ai_agents
   docker compose down && docker compose up -d
   ```

---

## Files Modified

1. `/home/ubuntu/ten-framework/ai_agents/.env` - Added TEN_LOG_FORMATTER and PYTHONUNBUFFERED
2. `/home/ubuntu/ten-framework/ai_agents/server/internal/worker.go` - Fixed PrefixWriter and added cmd.Env

---

## Success! 🎉

Logs are now visible with channel prefixes:
- Python extension logs appear in real-time
- `ten_env.log_info()` calls work
- Python `print()` statements visible
- Lifecycle events (registration, start, stop) logged
- Thymia and all other extensions produce visible output

**Monitoring logs**:
```bash
# All logs
docker exec ten_agent_dev tail -f /tmp/task_run.log

# Specific channel
docker exec ten_agent_dev tail -f /tmp/task_run.log | grep '\[channel-name\]'

# From host machine
/home/ubuntu/ten-framework/ai/monitor_logs.sh
```
