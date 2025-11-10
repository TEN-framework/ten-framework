# Thymia Analyzer - Quick Debug Guide

**Last Updated**: 2025-11-10

---

## 🔍 Quick Session Timeline

### Find Recent Test Session

```bash
# 1. Find most recent session
ls -lt /tmp/ten_agent/property-*.json | head -1

# 2. Extract channel name from property file
CHANNEL=$(ls -lt /tmp/ten_agent/property-*.json | head -1 | awk '{print $NF}' | xargs basename | cut -d'-' -f2)
echo "Channel: $CHANNEL"

# 3. Get all THYMIA logs for that session
sudo docker exec ten_agent_dev bash -c "grep '\[$CHANNEL\]' /tmp/task_run.log | grep 'THYMIA'"
```

### Key THYMIA Log Patterns

All important Thymia logs contain `[thymia_analyzer]` and one of these patterns:

**Initialization**:
```
[THYMIA] Thymia analyzer initialized
[THYMIA] Mode: demo_dual (hellos + apollo)
```

**Phase Triggers**:
```
[HELLOS PHASE 1/2] Starting Hellos API workflow
[APOLLO PHASE 2/2] Starting Apollo API workflow
```

**Audio Status**:
```
[thymia_analyzer] Speech buffer status: X.Xs / 22.0s (hellos target)
[thymia_analyzer] Speech buffer status: X.Xs / 44.0s (apollo target)
```

**API Results**:
```
[HELLOS COMPLETE] Results: {stress: X%, distress: Y%, ...}
[APOLLO COMPLETE] Results: {depression: X%, anxiety: Y%, ...}
```

**Errors**:
```
[THYMIA_ERROR] Error message here
```

### Extract Full Session Timeline

```bash
# Get complete timeline for channel
CHANNEL="agora_XXXXX"  # Replace with actual channel
sudo docker exec ten_agent_dev bash -c "grep '\[$CHANNEL\]' /tmp/task_run.log | grep -E 'THYMIA|HELLOS|APOLLO|Phase'" | less
```

---

## 🚨 Last Test Session (2025-11-10 08:14)

**Status**: ❌ FAILED - Deque bug incomplete

**Session**:
- Channel: `agora_g3qhjr`
- Graph: `flux_apollo_cartesia_heygen`
- Start: 08:14:03 UTC
- End: 08:31:56 UTC (SIGKILL after errors)

**Error**:
```
AttributeError: 'list' object has no attribute 'popleft'
File extension.py, line 93, in add_frame
    self.circular_buffer.popleft()
```

**Root Cause**: Deque fix incomplete - Line 146 resets buffer to `list` instead of `deque`

---

## 🐛 CRITICAL BUG: Incomplete Deque Fix

### The Problem

**Commit `44b505ce1`** fixed line 60 but missed line 146:

```python
# Line 60 - ✅ FIXED
self.circular_buffer = deque()

# Line 146 - ❌ STILL BROKEN
self.circular_buffer = [pcm_data]  # Should be deque([pcm_data])
```

**When line 146 executes** (buffer reset during speech), it replaces the deque with a list. Then line 93 tries `popleft()` on a list → crash.

### The Fix Needed

```python
# Line 146 - Change from:
self.circular_buffer = [pcm_data]

# To:
self.circular_buffer = deque([pcm_data])
```

---

## ⚠️ Python Code Changes Require Service Restart

### Critical Documentation Gap Found

**Issue**: Documentation mentions Python reloads on restart (AI_working_with_ten.md:132) but doesn't emphasize this enough.

**The Rule**:
1. Python extension code is loaded into memory when worker starts
2. Code changes on disk are NOT picked up by running workers
3. **MUST restart services after any Python code changes**

**After Making Python Changes**:
```bash
# Nuclear restart to load new code
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'; pkill -9 node; pkill -9 bun"
sudo docker exec ten_agent_dev bash -c "rm -f /app/playground/.next/dev/lock"
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
sleep 12
```

### What Triggers Automatic Reload

| Change Type | Container Restart | Service Restart | No Restart |
|-------------|------------------|-----------------|------------|
| **Python code (.py)** | ✅ Yes | ✅ Yes | ❌ No |
| **Property.json** | ❌ No | ❌ No | ✅ Yes (runtime copy) |
| **Graph config** | ❌ No | ✅ Yes | ❌ No |
| **Go server** | ❌ No | ✅ Yes | ❌ No |

**Key Point**: Python extensions are **NOT** hot-reloaded. Always restart after code changes.

---

## 📋 Documentation Updates Needed

### AI_working_with_ten.md - Add Warning Box

After line 132 (Python reload mention), add:

```markdown
### ⚠️ CRITICAL: Python Code Changes

**Python extensions are NOT hot-reloaded!**

After modifying any Python extension code:
1. ✅ **MUST restart services** (use nuclear restart)
2. ❌ **Changes will NOT be picked up** by running workers
3. ⚠️ Old code stays in memory until process exits

**Quick Check - Is My Code Running?**
```bash
# Find when worker started
ps aux | grep "bin/api" | grep -v grep

# Compare to your last git commit
git log -1 --format="%ai"

# If commit is AFTER worker start → restart needed!
```
```

### AI_working_with_ten_compact.md - Add Section

```markdown
## Python Code Changes

⚠️ **ALWAYS restart after Python changes**

```bash
# After editing any .py file in extensions:
sudo docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'"
sudo docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task run > /tmp/task_run.log 2>&1"
```

Python extensions are loaded at worker startup. Changes on disk won't be picked up until restart.
```

---

## 🔧 Immediate Actions

1. **Fix the deque bug** (line 146)
2. **Restart services** to load the fix
3. **Update documentation** with Python reload warnings
4. **Test with new session**

---

## 📝 Testing Checklist

After fixing and restarting:

- [ ] Nuclear restart completed
- [ ] Wait 12 seconds for full startup
- [ ] Check system health: `curl http://localhost:8080/health`
- [ ] Verify graphs available: `curl http://localhost:8080/graphs | jq`
- [ ] Start test session
- [ ] Monitor logs: `sudo docker exec ten_agent_dev tail -f /tmp/task_run.log | grep THYMIA`
- [ ] Verify no `popleft` errors
- [ ] Confirm Hellos phase completes
- [ ] Confirm Apollo phase triggers (if enough speech)

---

*For full session history and detailed timeline, see previous status.md version or check /tmp/ten_agent/ logs*
