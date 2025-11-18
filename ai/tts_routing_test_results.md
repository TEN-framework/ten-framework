# TTS Audio Routing - Test Results

## Test Session: 2025-11-18 10:53:52 UTC

### Summary

✅ **Implementation:** Complete
✅ **Static Verification:** Passed
⚠️ **Runtime Test:** Worker timeout (no audio activity on channel)

---

## What Was Implemented

### Commits
- `07f8876ec` - Added logging to track tts_audio message flow
- `357ecf1ed` - Added TTS audio event routing through avatar to Thymia

### Code Changes
1. **rebuild_property.py** (lines 469-486, 522-531)
   - Added TTS connection with tts_audio_start/end routing to avatar
   - Updated avatar connection to receive from TTS (source field) and forward to Thymia (dest field)

2. **property.json**
   - Rebuilt all 9 graphs with correct TTS routing
   - All 6 Apollo graphs now have complete message flow

3. **Logging**
   - agent.py: log_debug → log_info for TTS events
   - thymia_analyzer/extension.py: Added on_data debug log

---

## Static Verification ✅

### Configuration Verified

**TTS Connection (from property.json):**
```json
{
  "extension": "tts",
  "data": [
    {
      "name": "tts_audio_start",
      "dest": [{"extension": "avatar"}]
    },
    {
      "name": "tts_audio_end",
      "dest": [{"extension": "avatar"}]
    }
  ]
}
```

**Avatar Connection (from property.json):**
```json
{
  "extension": "avatar",
  "data": [
    {
      "name": "tts_audio_start",
      "source": [{"extension": "tts"}],
      "dest": [{"extension": "thymia_analyzer"}]
    },
    {
      "name": "tts_audio_end",
      "source": [{"extension": "tts"}],
      "dest": [{"extension": "thymia_analyzer"}]
    }
  ]
}
```

### Runtime Property Verification ✅

**Checked:** `/tmp/ten_agent/property-agora_g3qhjr-20251118_105352_000.json`

The runtime property file (loaded when session starts) correctly contains:
- TTS connection with routing to avatar
- Avatar connection with source from TTS and dest to thymia_analyzer
- Graph: `flux_apollo_gpt_4o_cartesia_anam`

**All 6 Apollo Graphs Verified:**
1. nova3_apollo_oss_cartesia_heygen ✅
2. nova3_apollo_oss_cartesia_anam ✅
3. nova3_apollo_gpt_4o_cartesia_heygen ✅
4. nova3_apollo_gpt_4o_cartesia_anam ✅
5. flux_apollo_gpt_4o_cartesia_heygen ✅
6. flux_apollo_gpt_4o_cartesia_anam ✅

---

## Runtime Test Results

### Test Execution

**Time:** 2025-11-18 10:53:52 - 10:54:54 UTC
**Channel:** agora_g3qhjr
**Graph:** flux_apollo_gpt_4o_cartesia_anam
**Session ID:** test_tts_routing_real

### What Happened

1. **10:53:52** - Session started successfully
   - Worker PID: 27461
   - Property file created: `/tmp/ten_agent/property-agora_g3qhjr-20251118_105352_000.json`
   - Graph loaded: `flux_apollo_gpt_4o_cartesia_anam`

2. **10:53:52 - 10:54:54** - Worker ran for 62 seconds
   - Timeout checks every 5 seconds
   - No audio activity detected
   - No RTC connection from client
   - Worker updateTs never changed (remained at creation time)

3. **10:54:54** - Worker timed out
   ```
   WARN Worker TIMEOUT EXCEEDED - stopping worker
   channelName=agora_g3qhjr pid=27461 ageSeconds=62 exceededBySeconds=2
   ```
   - Worker terminated with SIGTERM
   - Graceful shutdown succeeded

### Why No TTS Events Observed

**Root Cause:** No audio activity on the channel

For TTS events to fire, this flow must occur:
```
User speaks in Agora channel
  ↓
STT (Deepgram) transcribes audio
  ↓
ASR result sent to Agent
  ↓
Agent sends to LLM
  ↓
LLM generates response
  ↓
LLM sends text_data to TTS
  ↓
TTS generates speech
  ↓
TTS sends tts_audio_start/end ← WE WOULD SEE THIS HERE
  ↓
Avatar receives and forwards
  ↓
Thymia receives and processes ← AND THIS
```

**What we observed:** Worker started, waited 60 seconds, no audio activity, timed out

**What this means:**
- ✅ Configuration is correct
- ✅ Graph loaded properly
- ✅ Worker started successfully
- ⚠️ No client connected to trigger the flow

---

## How to Test When User Returns

### Prerequisites
- User must be actively speaking in channel `agora_g3qhjr`
- Or user must connect via playground and speak

### Testing Method 1: User Already in Channel

If you're already in the channel speaking:

```bash
# Start session
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{
    "request_id": "test_tts_final",
    "channel_name": "agora_g3qhjr",
    "user_uid": 176573,
    "graph_name": "flux_apollo_gpt_4o_cartesia_anam"
  }'

# Monitor logs in real-time (new terminal)
sudo docker exec ten_agent_dev tail -f /tmp/task_run.log | grep -E "(THYMIA_ON_DATA|THYMIA_TTS|AGENT-TTS-EVENT)"
```

**Then speak to the agent and watch for:**

1. **Agent Response Triggers:**
   ```
   [agora_g3qhjr] [AGENT-INFO] LLM response: ...
   ```

2. **TTS Audio Start:**
   ```
   [agora_g3qhjr] [THYMIA_ON_DATA] Received data message: tts_audio_start
   [agora_g3qhjr] [THYMIA_TTS_START] Agent started speaking, response_start_time=...
   ```

3. **TTS Audio End:**
   ```
   [agora_g3qhjr] [THYMIA_ON_DATA] Received data message: tts_audio_end
   [agora_g3qhjr] [THYMIA_TTS_END] Agent speaking until timestamp=1234567890.123 (duration=2500ms)
   ```

4. **Announcement Blocking (if Apollo triggers during speech):**
   ```
   [agora_g3qhjr] [THYMIA_TRIGGER_CHECK] Skipping trigger - agent still speaking (2.3s remaining)
   ```

### Testing Method 2: Via Playground

1. Open playground: http://localhost:3000
2. Select graph: `flux_apollo_gpt_4o_cartesia_anam`
3. Enter channel: `agora_g3qhjr`
4. Join and speak
5. Monitor logs as above

### What Should NOT Appear

❌ **Agent receiving TTS events (wrong routing):**
```
[agora_g3qhjr] [AGENT-TTS-EVENT] Received tts_audio_start event
```

If you see this, messages are routing to agent instead of avatar.

### Expected Behavior

**Hellos Announcement:**
- Agent speaks wellness metrics
- Thymia receives tts_audio_start
- Thymia calculates agent_speaking_until timestamp
- Apollo trigger is blocked until timestamp expires

**Apollo Announcement:**
- Only triggers AFTER Hellos finishes speaking
- No more interruptions mid-sentence

---

## Monitoring Commands

### Real-time TTS Event Monitoring
```bash
# Watch for TTS events
sudo docker exec ten_agent_dev tail -f /tmp/task_run.log | \
  grep --line-buffered -E "(THYMIA_ON_DATA|THYMIA_TTS|AGENT-TTS-EVENT|tts_audio)"
```

### Check Active Sessions
```bash
curl -s http://localhost:8080/list | jq
```

### Stop Session
```bash
curl -X POST http://localhost:8080/stop \
  -H "Content-Type: application/json" \
  -d '{"channel_name": "agora_g3qhjr"}'
```

---

## Technical Details

### Message Flow (Configured)
```
TTS Extension (cartesia_tts)
  → send_data("tts_audio_start")
  → send_data("tts_audio_end")
  ↓ [graph routes via property.json]
Avatar (anam_avatar_python)
  → receives from TTS (source: [{"extension": "tts"}])
  → [graph auto-forwards via property.json]
  ↓
Thymia Analyzer
  → on_data() receives messages
  → Processes tts_audio_start
  → Processes tts_audio_end with duration
  → Calculates agent_speaking_until
  → Blocks Apollo announcements
```

### Code Locations
- **TTS sends:** `/home/ubuntu/ten-framework/ai_agents/agents/ten_packages/system/ten_ai_base/interface/ten_ai_base/tts2.py:274-318`
- **Avatar receives:** `/home/ubuntu/ten-framework/ai_agents/agents/ten_packages/extension/heygen_avatar_python/extension.py:239-263` (reference pattern)
- **Thymia processes:** `/home/ubuntu/ten-framework/ai_agents/agents/ten_packages/extension/thymia_analyzer_python/extension.py:922-996`
- **Announcement blocking:** `/home/ubuntu/ten-framework/ai_agents/agents/ten_packages/extension/thymia_analyzer_python/extension.py:2259-2268`

---

## Conclusion

### ✅ Implementation Complete
- TTS routing fully configured in all Apollo graphs
- Message flow: TTS → Avatar → Thymia
- Both source and dest fields properly set
- All static verification passed

### ⚠️ Runtime Test Incomplete
- Worker started successfully
- No audio activity to trigger TTS
- Worker timed out after 60 seconds (expected behavior)
- **Need active conversation to verify TTS events**

### 🔄 Next Steps
1. User connects to channel and speaks
2. Monitor logs for TTS event messages
3. Verify Apollo waits for Hellos to finish
4. Confirm no premature interruptions

### 📝 Documentation
- Full test plan: `/home/ubuntu/ten-framework/ai/tts_routing_test_plan.md`
- Quick summary: `/home/ubuntu/ten-framework/ai/tts_routing_summary.md`
- This report: `/home/ubuntu/ten-framework/ai/tts_routing_test_results.md`

---

**Status:** Ready for user testing when audio activity resumes on channel `agora_g3qhjr`
