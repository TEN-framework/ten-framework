# Thymia Analyzer - Debug Guide

**Last Updated**: 2025-11-10 (Post text_data + TTS fix)

---

## 🔄 How Thymia → LLM Communication Works

### The Intended Flow

1. **Thymia Extension** (single unified polling thread):
   - Polls Hellos API every 5s until complete
   - Polls Apollo API every 5s until complete
   - When API completes → sends `text_data` hint to LLM

2. **Graph Routing** (`property.json`):
   ```
   thymia_analyzer → text_data → main_control
   ```

3. **Main Control** (`main_python/agent/agent.py`):
   - Receives `text_data`
   - Queues text as LLM input
   - LLM sees hint: "Clinical analysis complete. Please tell me..."

4. **LLM Response**:
   - Immediately calls `get_wellness_metrics` tool
   - Receives results
   - Announces to user

**Expected Timeline**: Analysis ready → hint sent → LLM responds in <5 seconds

---

## 🐛 Bug Found & Fixed (2025-11-10)

### Root Cause

**File**: `main_python/agent/agent.py:140-155`

**Problem**: Agent only handled `asr_result`, dropped `text_data`:
```python
if data.get_name() == "asr_result":
    # handle ASR
else:
    self.ten_env.log_warn(f"Unhandled data: {data.get_name()}")  # ❌ DROPPED
```

**Impact**:
- Thymia sent hints correctly
- Hints were routed correctly
- Agent logged "Unhandled data: text_data" and dropped them
- LLM never received hints
- Eventually called `get_wellness_metrics` on its own after 2-3 minutes

### The Fix

**Added text_data handler** in `agent.py:152-162`:
```python
elif data.get_name() == "text_data":
    # Handle hints from extensions (e.g., Thymia analysis ready)
    text = data.get_property_string("text")
    is_final = data.get_property_bool("end_of_segment")

    if text and is_final:
        self.ten_env.log_info(f"[Agent] Received text_data hint: '{text}'")
        await self.queue_llm_input(text)  # ✅ Queue to LLM
```

**What Changed**:
- This fix was working locally last week
- Got lost (never committed)
- Restored today

**Also Fixed**:
- Updated `min_speech_duration` from 22s to 30s (all 6 graph configs)
- Added retry limits (3 max) and 90s timeout
- Hellos failure now sends immediate notification to LLM

---

## 📊 Key Log Patterns

### Successful Flow

```
[THYMIA_TRIGGER] Sending Apollo announcement to LLM
[THYMIA_TRIGGER_OK] Apollo announcement sent
[Agent] Received text_data hint: 'Clinical analysis complete. Please tell me...'
[llm] Requesting chat completions with: ...get_wellness_metrics...
```

### Broken Flow (Before Fix)

```
[THYMIA_TRIGGER] Sending Apollo announcement to LLM
[THYMIA_TRIGGER_OK] Apollo announcement sent
[main_control] Unhandled data: text_data  ← ❌ DROPPED HERE
[UNIFIED_POLLER] Retrying Apollo announcement (not confirmed by LLM, last sent 31s ago)
```

---

## 🔍 Quick Session Timeline

### Find Recent Test Session

```bash
# 1. Find most recent session
ls -lt /tmp/ten_agent/property-*.json | head -1

# 2. Extract channel name
CHANNEL=$(ls -lt /tmp/ten_agent/property-*.json | head -1 | awk '{print $NF}' | xargs basename | cut -d'-' -f2)
echo "Channel: $CHANNEL"

# 3. Get all THYMIA logs
sudo docker exec ten_agent_dev bash -c "grep '\[$CHANNEL\]' /tmp/task_run.log | grep 'THYMIA'"

# 4. Check for text_data handling
sudo docker exec ten_agent_dev bash -c "grep '\[$CHANNEL\]' /tmp/task_run.log | grep -E 'text_data|Received text_data hint'"
```

### Key THYMIA Log Patterns

**Initialization**:
```
[THYMIA_START] ThymiaAnalyzerExtension starting...
[THYMIA_START] ThymiaAnalyzerExtension started in DEMO_DUAL mode
```

**Phase Completion**:
```
[THYMIA_PHASE] Mood phase complete (30s collected)
[THYMIA_ANALYSIS_START] Starting Hellos analysis (30s mood)
[THYMIA_PHASE] Reading phase complete (60s total collected)
[THYMIA_ANALYSIS_START] Starting Apollo analysis (60s total: 30s mood + 30s reading)
```

**API Results**:
```
[UNIFIED_POLLER] Hellos API FAILED: ERR_RECORDING_TOO_SHORT - Recording supplied is too short
[THYMIA_HELLOS_DONE] stress=X%, distress=Y%, burnout=Z%
[THYMIA_APOLLO_DONE] depression=X%, anxiety=Y%
```

**Announcement Triggers**:
```
[THYMIA_TRIGGER] Sending Hellos announcement to LLM
[THYMIA_TRIGGER_OK] Hellos announcement sent
[Agent] Received text_data hint: 'Wellness analysis complete...'
```

---

## ⏱️ Expected Timeline for Successful Session

### DEMO_DUAL Mode (Hellos + Apollo)

```
00:00 - Session starts, user joins
00:05 - User provides name, DOB, sex → set_user_info called
00:10 - LLM asks about day/feelings (mood phase begins)
00:40 - Mood phase complete (30s audio captured)
00:40 - Hellos API starts (analyzing 30s mood audio)
00:45 - LLM asks user to read text (reading phase begins)
01:00 - Hellos API completes (~20s processing)
01:00 - [SYSTEM ALERT] sent to LLM (if Hellos succeeded)
01:05 - LLM calls get_wellness_metrics, announces 5 wellness metrics
01:15 - Reading phase complete (60s total audio captured)
01:15 - Apollo API starts (analyzing 30s mood + 30s reading)
01:35 - Apollo API completes (~20s processing)
01:35 - [SYSTEM ALERT] sent to LLM
01:40 - LLM calls get_wellness_metrics, announces depression/anxiety
01:45 - Session complete
```

**Key Timings**:
- Mood audio: 30 seconds
- Reading audio: 30 seconds (60s total from start)
- Hellos processing: ~15-25 seconds
- Apollo processing: ~15-25 seconds
- Total session: ~105 seconds (1:45)

### Failure Cases

**Hellos Fails (ERR_RECORDING_TOO_SHORT)**:
```
01:00 - Hellos API fails
01:00 - NO announcement sent (skipped because hellos_success=False)
01:35 - Apollo completes
01:35 - [SYSTEM ALERT] sent for Apollo only
01:40 - LLM calls get_wellness_metrics
01:40 - Tool returns status="partial" (only Apollo available)
01:41 - LLM announces: "Wellness metrics unavailable, but depression: X%, anxiety: Y%"
```

---

## 🔧 TTS Announcement Fix (2025-11-10)

### Problem: Announcements Not Spoken

**Symptom**: `text_data` hints reached LLM and LLM responded, but response was not spoken via TTS.

**Root Cause**: TTS rejected announcements with error:
```
[tts] Received a message for a finished request_id 'tts-request-1' with text_input_end=False
```

**Why**: TTS request_id is based on `turn_id`:
```python
request_id = f"tts-request-{self.turn_id}"
```

But `turn_id` only incremented on **user ASR** (line 96), not on `text_data`. So announcements reused old, already-finished request_ids → TTS rejected them.

### The Fix

**File**: `main_python/extension.py:128-134`

Made `text_data` behave **exactly like ASR**:
```python
async def on_data(self, ten_env: AsyncTenEnv, data: Data):
    # Handle text_data exactly like ASR: interrupt ongoing speech, increment turn, queue to LLM
    if data.get_name() == "text_data":
        await self._interrupt()  # Stop ongoing TTS/LLM, just like ASR does
        self.turn_id += 1
        ten_env.log_info(f"[MainControlExtension] text_data received, interrupted and turn_id incremented to {self.turn_id}")

    await self.agent.on_data(data)
```

**Result**:
- ✅ Announcements get fresh TTS request_id → TTS accepts them
- ✅ Announcements interrupt ongoing agent speech (natural behavior)
- ✅ User can interrupt announcement responses (platform handles naturally)
- ✅ `text_data` behaves **identically** to if user had spoken the text via ASR

### Conditional Hellos Announcement

**File**: `thymia_analyzer_python/extension.py:1786-1803`

**Problem**: Hellos announcement said "Wellness metrics ready" even when Hellos API failed.

**Fix**: Skip announcement entirely if `hellos_success=False`:
```python
async def _trigger_hellos_announcement(self, ten_env: AsyncTenEnv) -> bool:
    if self.hellos_success:
        hint_text = "[SYSTEM ALERT] Wellness metrics ready. IMMEDIATELY call get_wellness_metrics..."
        # Send announcement
        return True
    else:
        # Hellos failed - skip announcement entirely
        ten_env.log_info("[THYMIA_TRIGGER] Skipping Hellos announcement - API failed")
        return False
```

**Result**: LLM only receives "ready" announcements when data is actually available.

---

## 🚨 Current Status

**State**: ✅ FIXED - Ready for full testing

**Changes Made** (2025-11-10):
1. ✅ Restored `text_data` handler in `agent.py`
2. ✅ Updated `min_speech_duration`: 22s → 30s
3. ✅ Added retry limits (3 max) and 90s timeout
4. ✅ Fixed TTS rejection - `turn_id` now increments for text_data
5. ✅ text_data now behaves exactly like ASR (interrupts, fresh request_id)
6. ✅ Conditional Hellos announcement (skip if API failed)
7. ✅ Tested: PINEAPPLE test passed (announcement interrupted agent, was spoken)

**Files Modified**:
- `main_python/extension.py` - text_data interrupt + turn_id increment
- `main_python/agent/agent.py` - Added text_data handling
- `tenapp/property.json` - Updated min_speech_duration (6 graphs)
- `thymia_analyzer_python/extension.py` - Conditional announcements, error handling

**Test Results**:
- ✅ text_data reaches agent
- ✅ LLM receives and responds to announcements
- ✅ TTS accepts and speaks announcements
- ✅ User can interrupt announcements naturally
- ⏳ Full Thymia flow test pending

**Ready to Test**:
- Services restarted with all fixes
- Run full Thymia session
- Expected: Announcements spoken at ~1:00 (Hellos) and ~1:35 (Apollo)
- Monitor for proper interrupt behavior

---

## 🔧 Next Steps

1. **Restart services** to load all fixes
2. **Monitor logs** for text_data flow:
   ```bash
   sudo docker exec ten_agent_dev tail -f /tmp/task_run.log | grep -E "THYMIA|text_data|Agent.*Received"
   ```
3. **Verify timeline**: Analysis complete → hint received → LLM calls tool → results announced (<60s total)

---

*For previous session history, see git log for commits on 2025-11-10*
