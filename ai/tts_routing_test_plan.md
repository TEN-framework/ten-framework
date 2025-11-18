# TTS Audio Routing Fix - Test Plan

## Problem Statement

Apollo announcements were interrupting Hellos announcements before they finished speaking. This was happening because Thymia analyzer was not receiving TTS audio duration information, so it couldn't calculate when the agent would finish speaking and properly delay subsequent announcements.

## Root Cause

The TEN Framework requires explicit graph routing for Data messages sent via `send_data()`. The TTS extension was sending `tts_audio_start` and `tts_audio_end` messages, but there was no graph routing configured to deliver these messages to Thymia analyzer.

## Solution Implemented

### Changes Made

1. **Modified `/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp/rebuild_property.py`:**
   - Added TTS connection (lines 469-486) with `tts_audio_start` and `tts_audio_end` routing to avatar
   - Updated avatar connection (lines 522-531) to add `source: [{"extension": "tts"}]` fields

2. **Rebuilt `property.json`:**
   - All 9 graphs now have correct TTS routing
   - 6 Apollo graphs with avatars now receive TTS events

3. **Added logging (commit 07f8876ec):**
   - `agent.py`: Changed `log_debug` to `log_info` for TTS events (line 173)
   - `thymia_analyzer/extension.py`: Added debug log for all `on_data` messages (line 926)

### Message Flow

```
TTS Extension (cartesia_tts)
  ↓ send_data("tts_audio_start")
  ↓ send_data("tts_audio_end")
  ↓ [graph routes to avatar]
Avatar (heygen/anam)
  ↓ receives from TTS (source: [{"extension": "tts"}])
  ↓ [graph auto-forwards to thymia]
Thymia Analyzer
  ✓ Processes tts_audio_start → sets agent_currently_speaking = True
  ✓ Processes tts_audio_end → calculates agent_speaking_until timestamp
  ✓ Blocks Apollo trigger if current_time < agent_speaking_until
```

## Verification Completed

### ✅ Static Verification

1. **Property.json Configuration:**
   - All 6 Apollo graphs have TTS routing:
     - nova3_apollo_oss_cartesia_heygen
     - nova3_apollo_oss_cartesia_anam
     - nova3_apollo_gpt_4o_cartesia_heygen
     - nova3_apollo_gpt_4o_cartesia_anam
     - flux_apollo_gpt_4o_cartesia_heygen
     - flux_apollo_gpt_4o_cartesia_anam

2. **TTS Connection Verified:**
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

3. **Avatar Connection Verified:**
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

4. **Runtime Property Verified:**
   - Checked `/tmp/ten_agent/property-agora_g3qhjr-20251118_082717_000.json`
   - Confirmed TTS routing is correctly applied at runtime
   - Graph loaded: flux_apollo_gpt_4o_cartesia_anam

## Runtime Testing Required

### Test Steps

1. **Start an agent session** with an Apollo + Avatar graph:
   ```bash
   curl -X POST http://localhost:8080/start \
     -H "Content-Type: application/json" \
     -d '{
       "request_id": "test_apollo_timing",
       "channel_name": "agora_g3qhjr",
       "user_uid": 176573,
       "graph_name": "flux_apollo_gpt_4o_cartesia_anam"
     }'
   ```

2. **Connect to the Agora channel** and speak to trigger conversation

3. **Monitor logs** for TTS routing:
   ```bash
   /tmp/monitor_tts_logs.sh
   ```
   Or manually:
   ```bash
   sudo docker logs -f ten_agent_dev 2>&1 | grep -E "(THYMIA_ON_DATA|THYMIA_TTS|AGENT-TTS-EVENT)"
   ```

### Expected Log Output

When the agent responds with speech, you should see:

1. **TTS Audio Start:**
   ```
   [THYMIA_ON_DATA] Received data message: tts_audio_start
   [THYMIA_TTS_START] Agent started speaking, response_start_time=...
   ```

2. **TTS Audio End:**
   ```
   [THYMIA_ON_DATA] Received data message: tts_audio_end
   [THYMIA_TTS_END] Agent speaking until timestamp=... (duration=...ms)
   ```

3. **Announcement Blocking (if triggered during speech):**
   ```
   [THYMIA_TRIGGER_CHECK] Skipping trigger - agent still speaking (X.Xs remaining)
   ```

4. **What NOT to see:**
   ```
   [AGENT-TTS-EVENT] Received tts_audio_start event
   ```
   (If you see this, messages are routing to agent instead of avatar - something is wrong)

### Success Criteria

- ✅ Thymia receives tts_audio_start and tts_audio_end messages
- ✅ `agent_speaking_until` timestamp is calculated correctly
- ✅ Apollo announcements wait for Hellos to finish before triggering
- ✅ No premature interruptions between announcements
- ❌ Agent does NOT receive TTS events (messages route through avatar)

## Rollback Plan

If issues occur, revert commits:
```bash
git revert 357ecf1ed  # TTS routing fix
git revert 07f8876ec  # Logging changes
```

## Related Files

- `/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp/rebuild_property.py`
- `/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp/property.json`
- `/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp/ten_packages/extension/main_python/agent/agent.py`
- `/home/ubuntu/ten-framework/ai_agents/agents/ten_packages/extension/thymia_analyzer_python/extension.py`
- `/home/ubuntu/ten-framework/ai_agents/agents/ten_packages/system/ten_ai_base/interface/ten_ai_base/tts2.py`

## Commits

- `357ecf1ed` - fix: add TTS audio event routing through avatar to Thymia analyzer
- `07f8876ec` - debug: add logging to track tts_audio message flow

## Server Status

- ✅ Docker container: ten_agent_dev (running)
- ✅ Server health: http://localhost:8080/health (OK)
- ✅ Test session started: agora_test_tts_routing
- 📝 Monitoring script: /tmp/monitor_tts_logs.sh
