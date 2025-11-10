# TTS Audio End Event Investigation Plan

**Date**: 2025-11-10
**Issue**: Determining when TTS audio playback actually finishes (not just generation)

---

## Problem Statement

Currently, the Thymia extension tracks agent speaking state using `tts_audio_start` and `tts_audio_end` events. However, **`tts_audio_end` fires when TTS generation completes, not when audio playback finishes**.

### Evidence from Logs

```
16:12:04.069 - tts_audio_start (request-6)
16:12:09.759 - tts_audio_end (request-6, request_total_audio_duration_ms: 12074, reason: 2)
                                         ↑ Audio is 12.074 seconds long
         ↑ Only 5.69 seconds elapsed from start
```

**Gap**: TTS took 5.69s to generate audio that will play for 12.074s. The `tts_audio_end` event fires ~6.3s before audio finishes playing.

### Impact

If we set `agent_currently_speaking = False` immediately when `tts_audio_end` fires:
- Announcements could be sent while audio is still playing
- User hears agent speech interrupted mid-sentence
- Defeats the purpose of tracking agent speaking state

---

## Current Implementation (Incomplete)

**File**: `thymia_analyzer_python/extension.py:922-939`

```python
async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
    data_name = data.get_name()

    if data_name == "tts_audio_start":
        self.agent_currently_speaking = True
        ten_env.log_debug("[THYMIA_TTS] Agent started speaking")

    elif data_name == "tts_audio_end":
        # ❌ WRONG: Audio still playing for duration_ms more!
        self.agent_currently_speaking = False
        self.last_agent_speech_end_time = time.time()
        ten_env.log_debug("[THYMIA_TTS] Agent stopped speaking")
```

---

## Investigation Questions

### 1. What do the `reason` values mean in `tts_audio_end`?

From logs, we see:
```
16:12:09.759 - tts_audio_end (request-6, reason=2)
16:12:21.083 - tts_audio_end (request-7, reason=1)
16:12:45.764 - tts_audio_end (request-7, reason=2)
```

**Observations**:
- Some requests have TWO `tts_audio_end` events
- First has `reason=1`, second has `reason=2`
- Time gap between them approximately matches audio duration

**Hypothesis**:
- `reason=1` = TTSAudioEndReason.REQUEST_END (generation complete)
- `reason=2` = TTSAudioEndReason.INTERRUPTED (playback interrupted OR playback actually finished?)

**To verify**: Check `TTSAudioEndReason` enum definition in `ten_ai_base/message.py`

### 2. Is there a second `tts_audio_end` event for playback completion?

**From logs**: request-7 has two `tts_audio_end` events:
```
16:12:21.083 - reason=1, duration=16579ms
16:12:45.764 - reason=2, duration=16579ms (24.7s later!)
```

**Math check**: 24.7s ≈ 16.6s audio + overhead → **This might be the playback completion event!**

**To verify**:
- Run test session
- Check if every TTS request gets TWO `tts_audio_end` events
- Check if second event timing matches `request_total_audio_duration_ms`
- If yes, use `reason=2` event as actual playback completion

### 3. Are there RTC/Agora playback completion events?

**Files to investigate**:
- `agora_rtc` extension - might emit events when audio frame transmission completes
- Graph routing - check what events RTC sends

**Search commands**:
```bash
grep -r "audio.*complete\|playback.*end\|frame.*sent" agents/ten_packages/extension/agora_rtc/
grep -r "audio_frame.*end" agents/examples/voice-assistant-advanced/tenapp/
```

---

## Solution Options

### Option 1: Use Second `tts_audio_end` Event (If Available)

**If hypothesis is correct:**
```python
async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
    data_name = data.get_name()

    if data_name == "tts_audio_start":
        self.agent_currently_speaking = True

    elif data_name == "tts_audio_end":
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str)
        reason = payload.get("reason")

        if reason == 1:
            # Generation complete - keep speaking
            ten_env.log_debug("[THYMIA_TTS] TTS generation complete, audio still playing")
        elif reason == 2:
            # Playback complete (or interrupted)
            self.agent_currently_speaking = False
            self.last_agent_speech_end_time = time.time()
            ten_env.log_debug("[THYMIA_TTS] Agent stopped speaking (playback finished)")
```

**Pros**:
- Uses real events
- Accurate timing
- Handles interruptions correctly

**Cons**:
- Assumes hypothesis is correct (needs verification)

### Option 2: Calculate Playback End Time

**When `tts_audio_end` (reason=1) received:**
```python
elif data_name == "tts_audio_end":
    json_str, _ = data.get_property_to_json(None)
    payload = json.loads(json_str)

    duration_ms = payload.get("request_total_audio_duration_ms", 0)
    reason = payload.get("reason")

    if reason == 1:  # Generation complete
        # Calculate when audio will finish playing
        self.agent_speaking_until = time.time() + (duration_ms / 1000.0)
        ten_env.log_debug(f"[THYMIA_TTS] Audio will play for {duration_ms}ms more")
    elif reason == 2:  # Interrupted or playback complete
        self.agent_speaking_until = 0
        self.agent_currently_speaking = False
```

**In trigger check:**
```python
# Check if agent is still speaking
if self.agent_speaking_until and time.time() < self.agent_speaking_until:
    ten_env.log_info("[THYMIA_TRIGGER_CHECK] Skipping - agent still playing audio")
    return
```

**Pros**:
- Works even if only one `tts_audio_end` per request
- Simple timestamp comparison
- No async timers needed

**Cons**:
- Assumes audio starts playing immediately (may have buffering delay)
- Less accurate than real events

### Option 3: Hybrid Approach

Use Option 2 (timestamp) but verify with logs that second `tts_audio_end` events exist. If they do, migrate to Option 1 later.

---

## Next Steps

1. **Verify `TTSAudioEndReason` enum values**
   - Check `ten_ai_base/message.py` for enum definition
   - Confirm reason=1 vs reason=2 meaning

2. **Run test session and analyze logs**
   - Look for pattern of TWO `tts_audio_end` per request
   - Measure timing between reason=1 and reason=2
   - Confirm reason=2 timing matches audio duration

3. **Implement solution**
   - If TWO events confirmed → Use Option 1
   - If only ONE event → Use Option 2
   - Test with session to verify announcements don't interrupt

4. **Edge cases to handle**
   - What if audio is interrupted mid-playback? (user speaks)
   - What if multiple announcements queue up?
   - What about overlapping TTS requests?

---

## Testing Plan

### Test Case 1: Normal Flow
1. Start session, trigger Hellos + Apollo announcements
2. Monitor logs for `tts_audio_start` and `tts_audio_end` events
3. Verify announcements don't interrupt each other
4. Check timing: `reason=2` event should come ~12-16s after `reason=1`

### Test Case 2: User Interruption
1. Trigger announcement
2. User speaks during TTS playback
3. Verify `agent_currently_speaking` resets correctly
4. Next announcement should work without issues

### Test Case 3: Multiple Announcements
1. Trigger both Hellos and Apollo (if APIs complete separately)
2. Verify second announcement waits for first to finish
3. No overlapping or interruptions

---

## References

**Key Files**:
- `ten_ai_base/tts2.py:293-319` - `send_tts_audio_end()` definition
- `cartesia_tts2/extension.py:286-374` - Where `send_tts_audio_end()` is called
- `thymia_analyzer_python/extension.py:922-939` - Current (incomplete) implementation
- `property.json` - TTS event routing to Thymia extension

**Log Patterns to Monitor**:
```bash
# Check TTS event sequence
sudo docker exec ten_agent_dev bash -c "grep 'tts_audio_start\|tts_audio_end' /tmp/task_run.log"

# Check timing and reason codes
sudo docker exec ten_agent_dev bash -c "grep 'tts_audio_end' /tmp/task_run.log | grep 'request-'"
```

---

## Status

- ✅ Problem identified
- ✅ Current implementation tracks generation end, not playback end
- ⏳ Need to verify `reason=1` vs `reason=2` hypothesis
- ⏳ Need to test if second event exists consistently
- ⏳ Implementation pending verification
