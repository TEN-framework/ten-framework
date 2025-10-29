# Deepgram Flux STT & Latency Optimization Plan

**Branch**: `feat/thymia-agora`
**Goal**: Investigate and optimize voice pipeline for minimal latency

---

## Objectives

1. Switch from Deepgram Nova-3 to Flux model (with built-in turn detection)
2. Test Rime TTS as alternative to ElevenLabs
3. Add comprehensive logging to track pipeline behavior
4. Establish testing methodology to confirm changes work before client testing
5. Measure and optimize end-to-end latency

**Note**: We will NOT use TEN VAD or turn detection extensions as they can slow things down. Flux has built-in turn detection.

---

## Current State Analysis

### STT Configuration
**Location**: `property.json` line 626-635

```json
{
  "name": "stt",
  "addon": "deepgram_asr_python",
  "property": {
    "params": {
      "api_key": "${env:DEEPGRAM_API_KEY}",
      "language": "en-US",
      "model": "nova-3"
    }
  }
}
```

### TTS Configuration
**Location**: `property.json` line 656-668

```json
{
  "name": "tts",
  "addon": "elevenlabs_tts2_python",
  "property": {
    "dump": false,
    "dump_path": "./",
    "params": {
      "key": "${env:ELEVENLABS_TTS_KEY}",
      "model_id": "eleven_multilingual_v2",
      "voice_id": "pNInz6obpgDQGcFmaJgB",
      "output_format": "pcm_16000"
    }
  }
}
```

### Agora RTC Configuration
**Line 621**: `"enable_agora_asr": false`

✅ Confirmed: We're NOT using Agora's built-in ASR, using Deepgram instead

### VAD/Turn Detection Status
- ❌ `ten_vad_python` - NOT enabled in graph (won't use)
- ❌ `ten_turn_detection` - NOT enabled in graph (won't use)
- ✅ Flux has built-in turn detection (~260ms end-of-turn)

---

## Deepgram Flux Research

### Model Information
- **Model Name**: `flux-general-en`
- **Endpoint**: `/v2/listen` (NOT v1 - incompatible with Nova models)
- **WebSocket URL**: `wss://api.deepgram.com/v2/listen?model=flux-general-en`

### Key Features
- Built-in turn detection (~260ms end-of-turn detection)
- Nova-3 level transcription accuracy
- Conversational speech recognition (understands conversational flow)
- Replaces STT+VAD+endpointing pipeline in one model
- Handles interruptions and barge-in automatically

### Flux-Specific Parameters

| Parameter | Type | Default | Range | Purpose |
|-----------|------|---------|-------|---------|
| `eot_threshold` | float | 0.7 | 0.5-0.9 | Confidence level triggering EndOfTurn events |
| `eager_eot_threshold` | float | None | 0.3-0.9 | Enables early EagerEndOfTurn for faster LLM response |
| `eot_timeout_ms` | int | 5000 | 500-10000 | Max silence duration before forcing turn end |

### Audio Settings
- **Encoding**: linear16, linear32, mulaw, alaw, opus, ogg-opus
- **Sample Rate**: 8000, 16000, 24000, 44100, 48000 (16000 recommended)
- **Chunk Size**: **80ms chunks strongly recommended** for optimal performance

### Trade-offs
- `eager_eot_threshold` may increase LLM API costs by 50-70% (speculative responses)
- `TurnResumed` event signals when to cancel draft responses

---

## Rime TTS Configuration

### Existing Extension
Extension already available at: `ai_agents/agents/ten_packages/extension/rime_tts/`

### Configuration Structure
```json
{
  "name": "tts",
  "addon": "rime_tts",
  "property": {
    "dump": false,
    "dump_path": "./",
    "params": {
      "api_key": "${env:RIME_API_KEY}",
      "speaker": "astra",
      "model": "mistv2",
      "sampling_rate": 16000
    }
  }
}
```

### Config Parameters (from config.py)
- `api_key`: Rime API credentials
- `speaker`: Voice speaker (astra)
- `model`: TTS model (mistv2)
- `sampling_rate`: 16000 (default)
- `audioFormat`: "pcm" (set automatically)
- `segment`: "immediate" (set automatically)

### Environment Variable
```bash
RIME_API_KEY=3I7Gtcj6q-eGjjnsJEYA4hg-51WM8PCyOdViWN8chuc
```

---

## Implementation Tasks

### Phase 1: Investigation & Baseline Logging

#### Task 1.1: Add logging to deepgram_asr_python extension
**Files**: `ai_agents/agents/ten_packages/extension/deepgram_asr_python/extension.py`

**Log Points to Add**:
- Connection endpoint (v1 vs v2)
- Model being used
- All configuration parameters
- Any VAD or noise reduction settings
- Confirmation we're using Deepgram, not TEN built-in STT

**Log Format**:
```python
ten_env.log_info(f"[DEEPGRAM] Connecting to: {endpoint_url}")
ten_env.log_info(f"[DEEPGRAM] Model: {self.config.model}")
ten_env.log_info(f"[DEEPGRAM] Language: {self.config.language}")
ten_env.log_info(f"[DEEPGRAM] Sample rate: {self.config.sample_rate}")
ten_env.log_info(f"[DEEPGRAM] Encoding: {self.config.encoding}")
ten_env.log_info(f"[DEEPGRAM] Interim results: {self.config.interim_results}")
ten_env.log_info(f"[DEEPGRAM] Additional params: {self.config.params}")
```

#### Task 1.2: Check deepgram config for available options
**Files**:
- `ai_agents/agents/ten_packages/extension/deepgram_asr_python/config.py`
- `ai_agents/agents/ten_packages/extension/deepgram_asr_python/extension.py`

**Questions to Answer**:
- Is `url` parameter configurable or hardcoded?
- Can we pass Flux-specific parameters through `params` dict?
- Are there VAD or noise reduction options?
- What's the current default behavior?

#### Task 1.3: Test baseline performance
**Actions**:
- Measure current Nova-3 + ElevenLabs latency
- Document transcription quality
- Record logs for comparison

---

### Phase 2: Deepgram Flux Configuration

#### Task 2.1: Investigate extension v2 endpoint support
**Files**:
- `ai_agents/agents/ten_packages/extension/deepgram_asr_python/config.py` (line 10)
- Check if URL is configurable via params

**Determine**:
- Can we change endpoint without modifying code?
- Do we need to update extension code?

#### Task 2.2: Create Flux test configuration
**Action**: Update `voice_assistant_thymia` graph to use Flux

**Configuration**:
```json
{
  "name": "stt",
  "addon": "deepgram_asr_python",
  "property": {
    "params": {
      "api_key": "${env:DEEPGRAM_API_KEY}",
      "url": "wss://api.deepgram.com/v2/listen",
      "model": "flux-general-en",
      "language": "en-US",
      "eot_threshold": 0.7,
      "eot_timeout_ms": 3000,
      "sample_rate": 16000,
      "encoding": "linear16"
    }
  }
}
```

#### Task 2.3: Test Flux with ElevenLabs TTS
**Actions**:
- Start session with Flux configuration
- Monitor logs for Flux events (EndOfTurn, EagerEndOfTurn, TurnResumed)
- Measure transcription latency
- Compare with Nova-3 baseline
- Document any issues

---

### Phase 3: Rime TTS Integration

#### Task 3.1: Add Rime API key to environment
**File**: `.env`

```bash
RIME_API_KEY=3I7Gtcj6q-eGjjnsJEYA4hg-51WM8PCyOdViWN8chuc
```

#### Task 3.2: Update graph to use Rime TTS
**Configuration**:
```json
{
  "name": "tts",
  "addon": "rime_tts",
  "property": {
    "dump": false,
    "dump_path": "./",
    "params": {
      "api_key": "${env:RIME_API_KEY}",
      "speaker": "astra",
      "model": "mistv2",
      "sampling_rate": 16000
    }
  }
}
```

#### Task 3.3: Test Rime TTS quality and latency
**Measure**:
- TTS latency (text → audio)
- Audio quality vs ElevenLabs
- Artifacts or glitches
- Various utterance lengths

---

### Phase 4: Testing & Measurement

#### Task 4.1: Establish testing methodology
**Options**:
1. Use RTC channel with known audio input
2. Use playground for manual testing
3. Automated testing script

**Decision**: Start with manual playground testing, automate if needed

#### Task 4.2: Measure latency components
**Metrics to Track**:
1. Audio input → STT result (transcription latency)
2. STT result → LLM response (LLM latency)
3. LLM response → TTS audio (synthesis latency)
4. TTS audio → RTC output (network latency)
5. Total end-to-end latency

**Method**: Add timestamps at each stage, log deltas

#### Task 4.3: Create comparison matrix

| Configuration | Transcription | LLM | TTS | Network | Total |
|--------------|---------------|-----|-----|---------|-------|
| Nova-3 + ElevenLabs (baseline) | ? | ? | ? | ? | ? |
| Flux + ElevenLabs | ? | ? | ? | ? | ? |
| Nova-3 + Rime | ? | ? | ? | ? | ? |
| Flux + Rime (optimal) | ? | ? | ? | ? | ? |

---

### Phase 5: Latency Optimization

#### Task 5.1: Optimize Flux parameters
**Test Variations**:
- `eot_threshold`: 0.5, 0.6, 0.7, 0.8
- `eot_timeout_ms`: 1000, 2000, 3000, 5000
- `eager_eot_threshold`: None, 0.5, 0.6, 0.7

**Measure**: Impact on latency vs accuracy

#### Task 5.2: Investigate audio chunk size
**Questions**:
- What chunk size does Agora RTC currently use?
- Can we configure 80ms chunks (Flux recommendation)?
- Where to adjust: agora_rtc configuration?

#### Task 5.3: Review LLM configuration
**Check**:
- `max_tokens`: 512 (is this optimal?)
- Streaming responses (enabled?)
- `frequency_penalty`: 0.9 (impact on speed?)

#### Task 5.4: Remove unnecessary processing
**Review**:
- Audio buffering (adds latency?)
- Connection settings (ping_interval, timeouts)
- Any redundant processing in pipeline

---

### Phase 6: Production Configuration

#### Task 6.1: Apply optimal configuration
**Update**: `voice_assistant_thymia` graph with:
- Flux configuration
- Rime TTS configuration
- Optimized parameters

#### Task 6.2: Document changes
**Update**: `AI_working_with_ten.md` with:
- Flux setup instructions
- Rime TTS integration
- Latency optimization tips

#### Task 6.3: Test thoroughly
**Verify**:
- Transcription accuracy maintained
- Latency improved
- No regressions in conversation flow
- Turn detection working properly

---

## Open Questions (To Investigate)

### Q1: Deepgram Extension v2 Endpoint Support
**Question**: Does current deepgram_asr_python support v2 endpoint?
**Investigation**: Check `config.py` line 10 for URL parameter
**Status**: PENDING

### Q2: Flux Parameter Passing
**Question**: Can Flux parameters be passed through `params` dict?
**Investigation**: Review extension code for parameter handling
**Status**: PENDING

### Q3: Audio Chunk Configuration
**Question**: How to configure 80ms chunks for Flux?
**Investigation**: Check agora_rtc extension configuration
**Status**: PENDING

### Q4: Flux Event Handling
**Question**: Does extension handle EndOfTurn/EagerEndOfTurn/TurnResumed events?
**Investigation**: Review extension code for event handlers
**Status**: PENDING

### Q5: Cost Implications
**Question**: Impact of `eager_eot_threshold` on LLM API costs?
**Investigation**: Monitor API usage with/without eager mode
**Status**: PENDING

---

## Success Criteria

1. ✅ Confirm we're using Deepgram STT (not TEN built-in)
2. ⬜ Successfully switch to Deepgram Flux model
3. ⬜ Successfully integrate Rime TTS
4. ⬜ Reduce end-to-end latency vs baseline
5. ⬜ Maintain or improve transcription accuracy
6. ⬜ Establish testing methodology
7. ⬜ Document all changes

---

## Risk Mitigation

| Risk | Mitigation | Fallback |
|------|------------|----------|
| Flux endpoint incompatibility | Check URL configurability first | Modify extension.py for v2 |
| Rime TTS quality issues | Test thoroughly before switching | Keep ElevenLabs option |
| Flux turn detection too aggressive | Tune thresholds | Revert to Nova-3 |
| Extension code changes needed | Understand code before modifying | Use current extension as-is |

---

## Files to Review/Modify

### Extension Files
- `ai_agents/agents/ten_packages/extension/deepgram_asr_python/config.py`
- `ai_agents/agents/ten_packages/extension/deepgram_asr_python/extension.py`
- `ai_agents/agents/ten_packages/extension/rime_tts/config.py`
- `ai_agents/agents/ten_packages/extension/rime_tts/extension.py`

### Configuration Files
- `ai_agents/agents/examples/voice-assistant/tenapp/property.json` (voice_assistant_thymia graph)
- `ai_agents/agents/examples/voice-assistant/.env`

### Documentation Files
- `ai/AI_working_with_ten.md`
- `ai/deepgram/plan_flux.md` (this file)
- `ai/deepgram/status.md` (to create)

---

**Created**: 2025-10-29
**Status**: Investigation phase
