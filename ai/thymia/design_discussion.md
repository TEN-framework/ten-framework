# Thymia Integration - Design Discussion

## Questions & Decisions

### ✅ 1. Audio Routing: STT (live) + Thymia (batched)

**Question**: Will audio go to both STT for live transcription AND to Thymia in batches?

**Answer**: **YES**, exactly as planned. The `streamid_adapter` can send audio to multiple destinations:

```json
{
  "extension": "streamid_adapter",
  "audio_frame": [{
    "name": "pcm_frame",
    "dest": [
      {"extension": "stt"},           // ✓ Real-time transcription (Deepgram)
      {"extension": "thymia_analyzer"} // ✓ Batched wellness analysis
    ]
  }]
}
```

**Benefits**:
- STT provides immediate conversation transcription
- Thymia analyzes speech patterns in background (no latency impact on conversation)
- Both operate independently on same audio stream

---

### ✅ 2. Use Deepgram for STT

**Confirmed**: Already specified in plan as `deepgram_asr_python`.

**Configuration**:
```json
{
  "type": "extension",
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

---

### 🔑 3. How LLM Gets Thymia Results (CRITICAL DECISION)

After reviewing existing TEN extensions (weatherapi, bing_search), I found the **tool/function call pattern**.

#### Option A: Tool/Function Call ⭐ **RECOMMENDED**

**Pattern** (from weatherapi_tool_python):
```python
class ThymiaToolExtension(AsyncLLMToolBaseExtension):
    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        return [
            LLMToolMetadata(
                name="get_wellness_metrics",
                description="Get user's current mental wellness metrics from voice analysis",
                parameters=[]  # No parameters needed
            )
        ]

    async def run_tool(self, ten_env: AsyncTenEnv, name: str, args: dict) -> LLMToolResult:
        # Return latest available metrics
        if self.latest_results:
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps({
                    "distress": self.latest_results.distress,
                    "stress": self.latest_results.stress,
                    "burnout": self.latest_results.burnout,
                    "status": "available",
                    "analyzed_at": self.analysis_timestamp
                })
            )
        else:
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps({
                    "status": "in_progress",
                    "message": "Analyzing speech, results will be available soon"
                })
            )
```

**How it works**:
1. Extension registers tool on startup via `tool_register` command
2. LLM can call tool whenever it wants wellness context
3. Extension returns latest available metrics
4. LLM incorporates into conversation naturally

**Advantages**:
- ✅ Follows established TEN Framework pattern
- ✅ LLM controls when wellness info is relevant
- ✅ Doesn't bloat system prompt with unused data
- ✅ Can be called multiple times during conversation
- ✅ Gracefully handles "analysis not ready yet" case
- ✅ User explicitly suggested this approach

**Example LLM Usage**:
```
User: "I'm feeling really overwhelmed lately"
LLM: <calls get_wellness_metrics()>
Result: {distress: 7.2, stress: 8.1, burnout: 6.5}
LLM: "I can sense from your voice that you're experiencing
      elevated stress levels. Would you like to talk about
      what's causing you to feel overwhelmed?"
```

#### Option B: Data Message / System Prompt Injection

**Pattern**:
```python
# In thymia_analyzer extension
async def _send_results(self, ten_env: AsyncTenEnv, results: WellnessResults):
    wellness_data = Data.create("wellness_analysis")
    wellness_data.set_property_float("distress", results.distress)
    # ... set other properties
    await ten_env.send_data(wellness_data)

# In main_control extension
async def on_data(self, ten_env: AsyncTenEnv, data: Data):
    if data.get_name() == "wellness_analysis":
        # Inject into LLM system prompt
        context = f"User wellness: distress={distress}, stress={stress}..."
        await self.update_llm_context(ten_env, context)
```

**Advantages**:
- ✅ Proactive - LLM always has latest context
- ✅ No need for LLM to "know" to call a tool
- ✅ Good for passive monitoring

**Disadvantages**:
- ❌ Bloats system prompt even when not relevant
- ❌ Requires modifying main_control extension
- ❌ Less flexible than function call
- ❌ Doesn't follow TEN tool pattern

#### **DECISION: Use Option A (Tool/Function Call)**

**Rationale**:
1. Matches existing TEN Framework patterns (weatherapi, bing_search)
2. Gives LLM agency to request wellness data when appropriate
3. More maintainable - follows established extension interface
4. Better for privacy - data only retrieved when needed
5. User explicitly suggested this approach

---

### 🤔 4. Other Design Considerations

#### A. Buffering Strategy

**Question**: Should we use a sliding window or clear-and-restart after each analysis?

**Options**:
1. **Simple**: Buffer 30s → analyze → clear → start fresh
2. **Sliding Window**: Continuously buffer, analyze every 30s with overlap
3. **Triggered**: Only analyze when LLM calls the tool

**Recommendation**: **Option 1 (Simple)** for MVP
- Easier to implement and reason about
- Sufficient for most use cases
- Can enhance to sliding window later if needed

**Future Enhancement**: Add property to control mode:
```json
{
  "buffer_mode": "simple|sliding|triggered",
  "overlap_seconds": 10  // For sliding window mode
}
```

#### B. What if Analysis Not Ready Yet?

**Scenarios**:
1. **Before first analysis**: User just joined, not enough speech yet
2. **During analysis**: API call in progress (30-120 seconds)
3. **After analysis**: Results available

**Tool Response Strategy**:
```json
// Scenario 1: Not enough speech
{
  "status": "insufficient_data",
  "message": "Need 30 seconds of speech for analysis",
  "speech_collected": "12.3 seconds"
}

// Scenario 2: Analysis in progress
{
  "status": "analyzing",
  "message": "Analysis in progress, started 45 seconds ago",
  "last_known": {
    "distress": 6.2,  // From previous analysis if available
    "analyzed_at": "2024-10-28T14:30:00Z"
  }
}

// Scenario 3: Results available
{
  "status": "available",
  "metrics": {
    "distress": 7.2,
    "stress": 8.1,
    "burnout": 6.5,
    "fatigue": 5.8,
    "low_self_esteem": 4.3
  },
  "analyzed_at": "2024-10-28T14:35:23Z",
  "speech_analyzed": "32.1 seconds"
}
```

**LLM Behavior**:
- If insufficient data: Continue conversation normally, don't mention wellness
- If analyzing: Can acknowledge monitoring ("I'm listening carefully...")
- If available: Use metrics to inform empathetic responses

#### C. Continuous Analysis Support

**Question**: Should multiple audio segments be analyzed per session?

**Answer**: **YES**, with configurable behavior:

```json
{
  "continuous_analysis": true,  // Keep analyzing throughout session
  "min_interval_seconds": 60,   // Min time between analyses
  "max_analyses_per_session": 10  // Prevent API quota exhaustion
}
```

**Use Cases**:
- **Single analysis**: Quick wellness check at start of conversation
- **Continuous**: Monitor wellness changes during longer session
- **Triggered**: Only analyze when LLM requests it (future enhancement)

#### D. Audio Quality & VAD (Voice Activity Detection)

**Question**: How to ensure we only buffer actual speech, not silence?

**Current Plan**: Simple RMS (Root Mean Square) threshold
```python
volume = calculate_rms(pcm_data)
if volume > self.silence_threshold:
    self.speech_buffer.append(pcm_data)
```

**Considerations**:
- RMS threshold: `0.02` (configurable)
- May include some background noise
- Thymia API should handle this robustly

**Future Enhancement**: Use proper VAD library (webrtcvad, silero-vad)

#### E. User Privacy & Consent

**Critical Concerns**:
1. **Audio upload**: User speech sent to third-party (Thymia)
2. **Data retention**: Thymia may retain audio/transcripts
3. **Sensitive information**: Mental health data is highly sensitive

**Recommendations**:
1. **Explicit opt-in**: Require user consent before enabling
2. **Clear disclosure**: Explain what data is sent and retained
3. **Easy opt-out**: Allow disabling mid-session
4. **Anonymous by default**: Use generic user labels
5. **No PHI**: Don't send identifiable information

**Configuration**:
```json
{
  "user_consent_required": true,
  "anonymous_user_labels": true,
  "user_label": "anonymous",
  "date_of_birth": "1990-01-01",  // Generic date
  "birth_sex": "UNSPECIFIED"
}
```

#### F. Error Handling & Graceful Degradation

**Failure Scenarios**:
1. **Thymia API down**: Network error, service outage
2. **API key invalid**: Authentication failure
3. **Audio too short**: Analysis fails, need more speech
4. **Polling timeout**: Analysis takes too long
5. **Rate limiting**: API quota exceeded

**Strategy**: Fail gracefully, don't break conversation
```python
async def run_tool(self, ten_env: AsyncTenEnv, name: str, args: dict) -> LLMToolResult:
    try:
        if not self.config.api_key:
            return self._unavailable_response("API key not configured")

        if not self.latest_results:
            return self._insufficient_data_response()

        return self._success_response(self.latest_results)

    except ThymiaAPIError as e:
        ten_env.log_error(f"Thymia API error: {e}")
        return self._unavailable_response(f"Service temporarily unavailable: {e}")

    except Exception as e:
        ten_env.log_error(f"Unexpected error: {e}", exc_info=True)
        return self._unavailable_response("Analysis service error")
```

**LLM Behavior**: If tool returns error, continue conversation without wellness context

---

## Revised Architecture

### Extension Design: Hybrid Approach

**Combine background analysis + on-demand querying**:

```python
class ThymiaAnalyzerExtension(AsyncLLMToolBaseExtension):
    """
    - Continuously buffers audio in background
    - Automatically triggers analysis when 30s of speech collected
    - Registers as LLM tool for on-demand metrics retrieval
    - Supports multiple analyses per session
    """

    async def on_audio_frame(self, ten_env: AsyncTenEnv, audio_frame: AudioFrame):
        """Background: Buffer audio, auto-trigger analysis"""
        # Buffer audio with VAD
        # When threshold reached, start analysis task
        # Store results internally

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        """Register as LLM tool"""
        return [
            LLMToolMetadata(
                name="get_wellness_metrics",
                description="Get user's mental wellness metrics from voice analysis. "
                           "Returns stress, distress, burnout, fatigue, and self-esteem levels.",
                parameters=[]
            )
        ]

    async def run_tool(self, ten_env: AsyncTenEnv, name: str, args: dict) -> LLMToolResult:
        """On-demand: Return latest metrics when LLM requests"""
        # Return latest available results
        # Include status (available/analyzing/insufficient_data)
```

### Graph Connections

```json
{
  "connections": [
    {
      "extension": "main_control",
      "cmd": [{
        "names": ["tool_register"],
        "source": [
          {"extension": "weatherapi_tool_python"},
          {"extension": "thymia_analyzer"}  // Register wellness tool
        ]
      }]
    },
    {
      "extension": "streamid_adapter",
      "audio_frame": [{
        "name": "pcm_frame",
        "dest": [
          {"extension": "stt"},  // Real-time transcription
          {"extension": "thymia_analyzer"}  // Background analysis
        ]
      }]
    }
  ]
}
```

---

## Recommended Implementation Order

### Phase 1: Core Audio Analysis (Week 1)
- [ ] Audio buffering with VAD
- [ ] PCM to WAV conversion
- [ ] Thymia API client (create session, upload, poll)
- [ ] Background analysis task
- [ ] Store results internally

### Phase 2: Tool Integration (Week 1)
- [ ] Implement `AsyncLLMToolBaseExtension`
- [ ] Register `get_wellness_metrics` tool
- [ ] Implement `run_tool()` to return latest metrics
- [ ] Handle all status states (available/analyzing/insufficient)

### Phase 3: Graph Integration (Week 1-2)
- [ ] Create voice-assistant-thymia example
- [ ] Configure audio routing (streamid_adapter → thymia_analyzer)
- [ ] Configure tool registration (thymia_analyzer → main_control)
- [ ] Test end-to-end flow

### Phase 4: Polish (Week 2)
- [ ] Error handling for API failures
- [ ] Graceful degradation when service unavailable
- [ ] Metrics and logging
- [ ] Privacy controls (opt-in/opt-out)
- [ ] Documentation

---

## Open Questions

1. **Should we support real-time streaming analysis?**
   - Thymia API appears to be batch-only (upload → poll)
   - Could we send partial results as they become available?
   - **Decision**: Start with batch, investigate streaming API if available

2. **How to handle multi-language support?**
   - Thymia supports multiple languages
   - Should match STT language configuration?
   - **Decision**: Make `language` property configurable, default to match STT

3. **Should wellness metrics influence LLM system prompt?**
   - In addition to tool calls, proactively inject high-risk indicators?
   - Example: "ALERT: User showing signs of severe distress (9.2/10)"
   - **Decision**: Keep tool-only for now, add alerts as enhancement if needed

4. **Rate limiting strategy?**
   - Each analysis costs API quota
   - Should we limit analyses per session?
   - **Decision**: Add `max_analyses_per_session` property (default: 10)

5. **Local caching of results?**
   - Should we persist wellness metrics across sessions?
   - Could enable trend analysis over time
   - **Decision**: Out of scope for MVP, add as future enhancement

---

## Testing Strategy

### Unit Tests
- Audio buffer with VAD
- PCM to WAV conversion
- Thymia API client (mocked)
- Tool metadata registration
- Tool result formatting

### Integration Tests
- Audio flow: agora_rtc → streamid_adapter → thymia_analyzer
- Tool registration: thymia_analyzer → main_control
- Tool invocation: LLM calls get_wellness_metrics
- Error scenarios: API down, invalid key, insufficient data

### Manual Testing
- [ ] Audio buffering works (30s speech threshold)
- [ ] VAD correctly filters silence
- [ ] Analysis completes successfully
- [ ] LLM can call tool and receive results
- [ ] Tool returns appropriate status before analysis ready
- [ ] Continuous analysis works (multiple uploads)
- [ ] Error handling graceful (API failures)

---

## Summary of Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Audio routing** | Parallel: STT + Thymia | Independent processing, no latency impact |
| **STT provider** | Deepgram | Already specified, works well |
| **LLM integration** | Tool/Function call | Follows TEN patterns, LLM-controlled |
| **Buffering** | Simple (buffer → analyze → clear) | Easier MVP, can enhance later |
| **Continuous analysis** | Supported, configurable | Flexible for different use cases |
| **VAD method** | RMS threshold | Simple, works for MVP |
| **Error handling** | Graceful degradation | Never break conversation flow |
| **Privacy** | Opt-in, anonymous by default | Protect user data |

---

**Next Step**: Begin Phase 1 implementation with tool/function call architecture.
