# Latency Optimization & Logging Improvements

**Date**: 2025-11-11
**Goal**: Reduce latency, improve observability, and clean up verbose logs

---

## Current Issues

1. **Log verbosity**: `get_chat_completions@openai.py:261` lines dump entire LLM prompt (system + conversation history + tools) making tail/grep output unreadable
2. **No latency visibility**: Can't measure STT final → LLM inference → TTS output pipeline timing
3. **Heygen latency**: ~500ms+ overhead from HeyGen avatar processing
4. **Deepgram confidence**: Occasional phantom word interruptions - need confidence logging to debug
5. **Excessive debug logs**: Too many logs that aren't useful for production debugging

---

## Optimization Tasks

### 1. Improve tail/grep Experience

**Current command:**
```bash
sudo docker exec ten_agent_dev tail -f /tmp/task_run.log | grep -E "THYMIA|text_data|Agent.*Received"
```

**Problem**: Includes massive `get_chat_completions@openai.py:261` lines with full prompt

**Solution**: Exclude verbose lines
```bash
sudo docker exec ten_agent_dev tail -f /tmp/task_run.log | grep -E "THYMIA|text_data|Agent.*Received" | grep -v "get_chat_completions@openai.py:261"
```

**Recommended alias**:
```bash
alias tail-thymia='sudo docker exec ten_agent_dev tail -f /tmp/task_run.log | grep -E "THYMIA|text_data|Agent.*Received" | grep -v "get_chat_completions@openai.py:261"'
```

### 2. Add Latency Timing Logs

**Objective**: Measure end-to-end latency for voice interaction pipeline

**Key timestamps to log:**
- `[STT_FINAL]` - Deepgram final transcript received (with confidence)
- `[LLM_START]` - LLM inference started (after STT final)
- `[LLM_END]` - LLM response complete
- `[TTS_START]` - TTS generation started
- `[TTS_END]` - TTS audio sent to avatar/user
- `[AVATAR_SPEAKING]` - HeyGen avatar started speaking (if applicable)

**Files to modify:**
- `ai_agents/agents/ten_packages/extension/deepgram_asr_python/` - Add `[STT_FINAL]` log with confidence
- `ai_agents/agents/ten_packages/extension/main_python/agent/llm_exec.py` - Add `[LLM_START]`, `[LLM_END]` timing
- `ai_agents/agents/ten_packages/extension/cartesia_tts/` - Add `[TTS_START]`, `[TTS_END]` timing
- `ai_agents/agents/ten_packages/extension/heygen_avatar_python/` - Add `[AVATAR_SPEAKING]` timing

**Example log format:**
```
[STT_FINAL] t=1731311234.567 text="Hello world" confidence=0.98 duration_ms=1234
[LLM_START] t=1731311234.570 prompt_tokens=512
[LLM_END] t=1731311235.890 completion_tokens=128 duration_ms=1320
[TTS_START] t=1731311235.895 text="Hello! How are you?" chars=20
[TTS_END] t=1731311236.234 audio_bytes=48000 duration_ms=339
```

**Latency calculations:**
- STT → LLM: `LLM_START - STT_FINAL`
- LLM inference: `LLM_END - LLM_START`
- TTS generation: `TTS_END - TTS_START`
- Total pipeline: `TTS_END - STT_FINAL`

### 3. Log Deepgram Confidence Scores

**Problem**: Deepgram occasionally sends phantom words (false positives) that interrupt conversation

**Solution**: Log confidence for both interim and final transcripts

**Files to modify:**
- `ai_agents/agents/ten_packages/extension/deepgram_asr_python/extension.py`

**Example logs:**
```
[STT_INTERIM] text="Hello" confidence=0.85 is_final=False
[STT_FINAL] text="Hello world" confidence=0.98 is_final=True
[STT_PHANTOM?] text="um" confidence=0.34 is_final=True  # Low confidence = likely phantom
```

**Use case**: Filter logs for `confidence < 0.5` to identify phantom interruptions

### 4. Create flux_apollo_cartesia Graph

**Objective**: Test pipeline without HeyGen avatar latency overhead

**New graph name**: `flux_apollo_cartesia`

**Components**:
- STT: Deepgram Flux
- LLM: OpenAI GPT-4o (main_python agent with Thymia extension)
- TTS: Cartesia (direct audio output, no avatar)
- Audio: Direct RTC audio output (no HeyGen WebSocket)

**Benefits**:
- Removes ~500ms+ HeyGen latency
- Simpler debugging (fewer moving parts)
- Baseline for latency comparison

**Files to create/modify**:
- Copy `property.json` graph "heygen_flux_openai_cartesia_thymia_deepgram_v2"
- Rename to "flux_apollo_cartesia"
- Remove heygen_avatar extension
- Route TTS audio directly to RTC output

### 5. Logging Cleanup

**Logs to REMOVE** (production clutter):
- Debug logs in hot paths (e.g., per-audio-frame processing)
- Redundant "entering function X" logs
- Full prompt dumps in `get_chat_completions`

**Logs to COMMENT OUT** (keep for dev debugging):
- Verbose tool call arguments
- Full API responses (summarize instead)
- Per-chunk TTS streaming logs

**Logs to KEEP** (essential for debugging):
- `[THYMIA_*]` prefixed logs (phase tracking, API status, announcements)
- Error logs with stack traces
- Timing/latency logs (new)
- User input/output logs

**Files to review**:
- `ai_agents/agents/ten_packages/extension/openai_chatgpt_python/openai.py` - Remove full prompt dump at line 261
- `ai_agents/agents/ten_packages/extension/thymia_analyzer_python/extension.py` - Review debug logs
- `ai_agents/agents/ten_packages/extension/cartesia_tts/*.py` - Reduce TTS chunk logs
- `ai_agents/agents/ten_packages/extension/deepgram_asr_python/*.py` - Keep STT logs, add confidence

---

## Implementation Order

1. ✅ Document optimization plan (this file) - commit f61984230
2. ✅ Commit current working state - commit f61984230
3. ✅ Remove verbose LLM logging - commit 4a02de45b
4. ✅ Add Deepgram confidence logging - commit 4a02de45b
5. ⏳ Add latency timing logs (STT/LLM/TTS)
6. ⏳ Create flux_apollo_cartesia graph
7. ⏳ Clean up additional verbose logs
8. ⏳ Test latency measurements
9. ⏳ Update ai/status.md with findings

---

## Success Metrics

- **Baseline latency** (with HeyGen): STT final → user hears response = ???ms
- **Target latency** (without HeyGen): STT final → user hears response < 2000ms
- **Phantom word detection**: Confidence < 0.5 threshold identifies false positives
- **Log readability**: tail/grep shows only relevant events, no prompt dumps

---

## Notes

- Heygen latency includes: WebSocket RTT, avatar render time, audio playback buffer
- Cartesia TTS typically generates audio in ~300-500ms for short responses
- Deepgram Flux STT finality can vary 100-500ms depending on pause detection
- LLM inference for tool calls adds significant latency vs text-only responses
