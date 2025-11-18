# Current Task Plan - TTS Audio Routing Fix

## Context Refresh (Read this after context window reset)
**Always read:** `/home/ubuntu/ten-framework/ai/AI_working_with_ten_compact.md`

## Problem
Apollo announcements interrupt Hellos announcements before they finish speaking.

## Root Cause
Thymia analyzer extension has TTS duration tracking code but never receives `tts_audio_start/end` messages because graph routing is misconfigured.

## Current Status
❌ **property.json validation FAILED** - Schema violation in avatar connection

**Error:**
```
{"name":"tts_audio_start","source":[{"extension":"tts"}],"dest":[{"extension":"thymia_analyzer"}]}
is valid under more than one of the schemas listed in the 'oneOf' keyword
```

**Problem:** TEN Framework schema doesn't allow both `source` AND `dest` in the same data entry.

## Solution
Route TTS messages **DIRECTLY** from TTS to thymia_analyzer, NOT through avatar.

### Correct Pattern:
```python
tts_conn = {
    "extension": "tts",
    "data": [
        {"name": "text_data", "source": [{"extension": "llm"}]},
        {
            "name": "tts_audio_start",
            "dest": [{"extension": "thymia_analyzer"}],  # DIRECT routing
        },
        {
            "name": "tts_audio_end",
            "dest": [{"extension": "thymia_analyzer"}],  # DIRECT routing
        },
    ],
}
```

### Remove from avatar connection:
Avatar doesn't need tts_audio entries at all. Only thymia needs these for timing tracking.

## Implementation Steps

### 1. Fix rebuild_property.py
- [x] Add TTS connection with direct routing to thymia
- [ ] Remove tts_audio_start/end from avatar connection
- [ ] Rebuild property.json

### 2. Testing
- [ ] Verify property.json validates (no schema errors)
- [ ] Start test session with user's channel
- [ ] Monitor for `[THYMIA_ON_DATA]` logs
- [ ] Confirm Apollo waits for Hellos to finish

### 3. Verification Commands
```bash
# Check property.json validates
sudo docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && task run > /tmp/task_run.log 2>&1" &
sleep 5 && sudo docker exec ten_agent_dev tail -20 /tmp/task_run.log | grep -i error

# Start test session
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{
    "request_id": "test_tts_final",
    "channel_name": "agora_g3qhjr",
    "user_uid": 133843,
    "graph_name": "nova3_apollo_gpt_4o_cartesia_heygen"
  }'

# Monitor TTS events
sudo docker exec ten_agent_dev tail -f /tmp/task_run.log | \
  grep --line-buffered -E "(THYMIA_ON_DATA|THYMIA_TTS|tts_audio)"
```

## Key Files
- **rebuild_property.py**: `/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp/rebuild_property.py`
- **property.json**: `/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp/property.json`
- **thymia_analyzer**: `/home/ubuntu/ten-framework/ai_agents/agents/ten_packages/extension/thymia_analyzer_python/extension.py`

## Expected Message Flow
```
TTS Extension (cartesia_tts)
  → send_data("tts_audio_start")
  → send_data("tts_audio_end")
  ↓ [graph routes via property.json]
Thymia Analyzer
  → on_data() receives messages
  → Calculates agent_speaking_until
  → Blocks Apollo announcements during speech
```

## Important Notes
- **No server restart needed** after property.json changes (unless adding new graphs)
- Property.json is loaded when each new session starts
- Frontend restart needed only if graph list changes
- TTS messages route DIRECTLY to thymia, NOT through avatar
- Avatar only needs tts_audio messages if it uses them for its own purposes (like HeyGen API)

## Last Updated
2025-11-18 12:16 UTC
