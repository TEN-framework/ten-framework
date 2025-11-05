# Apollo API Integration Plan - Thymia Extension

**Date**: 2025-11-05
**Status**: Approved - Demo Mode (Dual API)
**Selected Approach**: Option E - Demo Mode

---

## Overview

Integrate Thymia's **Apollo API** (depression/anxiety detection) alongside the existing **Hellos API** (wellness metrics) in the thymia_analyzer extension for a comprehensive mental wellness demo.

### Current State: Hellos API
- **Collects**: Continuous speech audio during conversation
- **Returns**: 5 wellness metrics as percentages (0-100)
  - Stress, Distress, Burnout, Fatigue, Low Self-Esteem
- **Requirements**: 22 seconds of speech audio minimum
- **API**: `POST /v1/models/hellos`

### Proposed: Apollo API
- **Collects**: TWO specific audio samples
  1. **Mood question response**: "How are you feeling today?"
  2. **Read aloud task**: User reads specific text
- **Returns**: Depression and Anxiety metrics
  - Probability (0-1 float)
  - Severity (category)
- **Requirements**: Two distinct audio segments
- **API**: `POST /v1/models/apollo` → upload URLs → poll for results

---

## Key Differences & Challenges

| Aspect | Hellos | Apollo |
|--------|--------|--------|
| **Audio Input** | Continuous natural speech | Two specific prompted tasks |
| **Collection Method** | Passive during conversation | Active prompting required |
| **User Experience** | Seamless, invisible | Requires explicit tasks |
| **Metrics** | 5 wellness dimensions | 2 clinical indicators |
| **Processing** | Single API call | Create → Upload (2x) → Poll |
| **Min Duration** | 22s total | Unknown per audio |

**Challenge**: Apollo requires structured prompts that don't fit naturally into conversational flow.

---

## Integration Options

### Option A: Sequential Mode (Recommended)
Run Hellos first for initial screening, then optionally run Apollo if concerning metrics detected.

**Flow**:
1. User has natural conversation (existing behavior)
2. Hellos API analyzes → returns wellness metrics
3. **IF** metrics indicate concern (e.g., distress > 50%, low_self_esteem > 40%)
   - LLM asks: "I noticed some patterns. Would you like a more detailed assessment?"
   - If user agrees → trigger Apollo flow
4. LLM guides user through Apollo tasks:
   - "How are you feeling today?" (record mood response)
   - "Please read this sentence: [provided text]" (record read aloud)
5. Apollo API analyzes → returns depression/anxiety
6. LLM provides compassionate feedback + resources

**Pros**:
- ✅ Non-intrusive for most users
- ✅ Clinical assessment only when needed
- ✅ Preserves existing UX
- ✅ Both APIs provide complementary data

**Cons**:
- ❌ More complex state management
- ❌ Longer total interaction time
- ❌ Requires conversation flow changes

---

### Option B: Parallel Mode
Run both APIs simultaneously on the same audio stream.

**Flow**:
1. User has natural conversation
2. Identify two segments from conversation:
   - First emotional/mood-related utterance → mood audio
   - Any clear statement → read aloud audio (not ideal)
3. Send to both Hellos and Apollo APIs
4. LLM synthesizes results from both

**Pros**:
- ✅ Single conversation session
- ✅ Fast results
- ✅ No UX changes

**Cons**:
- ❌ Apollo accuracy may suffer (not proper prompts)
- ❌ Unclear which audio segments to use
- ❌ "Read aloud" requirement cannot be met from natural speech

---

### Option C: Apollo Only Mode
Replace Hellos with Apollo entirely.

**Flow**:
1. LLM introduces assessment upfront
2. Guides user through two specific tasks
3. Apollo API analyzes → returns results
4. LLM provides feedback

**Pros**:
- ✅ Simpler implementation (single API)
- ✅ Clinical focus (depression/anxiety)
- ✅ Proper Apollo audio collection

**Cons**:
- ❌ Loses 5 wellness dimensions from Hellos
- ❌ Less natural conversation flow
- ❌ Higher friction for users

---

### Option D: User Choice Mode
Let user choose which assessment at start.

**Flow**:
1. LLM: "I can provide a wellness check or clinical assessment. Which would you prefer?"
2. **Wellness check** → Hellos flow (existing)
3. **Clinical assessment** → Apollo flow (new)

**Pros**:
- ✅ User control
- ✅ Clear expectations
- ✅ Both APIs available

**Cons**:
- ❌ Adds decision friction
- ❌ Users may not know which to choose
- ❌ More complex LLM prompting

---

### Option E: Demo Mode (Dual API) ⭐ **SELECTED**
Run both APIs on the same audio stream with upfront user consent.

**Flow**:
1. **Opening greeting** (agent speaks first):
   ```
   Hi there! I would like to talk to you for a couple of minutes and use
   your voice to predict your mood and energy levels including any
   depression, anxiety, stress, and fatigue.

   Nothing will be recorded and this is purely a demonstration of what is
   possible now that we have trained our models with many hours of
   professionally labelled data.

   Please begin by telling me your name, sex and year of birth.
   ```

2. **User provides info**: Name, sex, year of birth
3. **Mood & interests conversation** (~30 seconds):
   - "How are you feeling today? Please describe your mood and energy level."
   - [User responds - Apollo mood audio captured]
   - "Tell me about your interests and hobbies."
   - [User responds - reaches 22+ seconds total for Hellos]
   - → **Trigger Hellos API** (starts processing in background)
4. **Reading task** (while Hellos processes, ~30 seconds):
   - "Thank you. While I'm analyzing that, please read this text aloud for about 30 seconds: [specific text]"
   - [User reads - Apollo read aloud audio captured]
   - → **Trigger Apollo API** (create, upload mood + read audio, poll)
5. **Parallel processing**:
   - Hellos API: Analyzing 22+ seconds of mood + interests conversation
   - Apollo API: Analyzing mood audio + reading audio
6. **Wait for both results** (~60 seconds total processing time)
6. **Comprehensive results**:
   - Hellos: Stress, Distress, Burnout, Fatigue, Low Self-Esteem (%)
   - Apollo: Depression and Anxiety (probability + severity)
7. **LLM synthesis**: Present all 7 metrics with context

**Pros**:
- ✅ Transparent upfront about what's being measured
- ✅ User gives informed consent before speaking
- ✅ Both APIs provide comprehensive mental wellness picture
- ✅ Single conversation (2-3 minutes total)
- ✅ Clear demo/research context ("nothing will be recorded")
- ✅ All metrics delivered at once

**Cons**:
- ⚠️ Apollo may be less accurate without explicit prompts
- ⚠️ Need to intelligently extract mood/read segments
- ⚠️ Higher API costs (both APIs every session)

**Why This Approach**:
- Demo/research context makes upfront disclosure natural
- Users expect comprehensive analysis after consent
- Single session is faster than sequential approach
- Showcase full capabilities of both models

---

## Selected Approach: **Option E (Demo Mode - Dual API)**

**Rationale**:
- Transparency: Users know what's being measured upfront
- Comprehensive: 7 mental wellness indicators in one session
- Efficient: Single 2-3 minute conversation
- Demo-appropriate: Clear research/demonstration context

**Voice Configuration**:
- **TTS**: Cartesia
- **Voice ID**: `71a7ad14-091c-4e8e-a314-022ece01c121`
- **Greeting**: Delivered as opening message before user speaks

---

## Implementation Plan

### Phase 1: Apollo API Client Implementation

**File**: `agents/ten_packages/extension/thymia_analyzer_python/apollo_api.py`

```python
class ApolloAPI:
    """Client for Thymia Apollo API (depression/anxiety detection)"""

    async def create_model_run(self, user_info: dict, language: str = "en-GB") -> dict:
        """
        Create Apollo model run and get upload URLs.

        Args:
            user_info: {userLabel, dateOfBirth, birthSex}
            language: Language code (default: en-GB)

        Returns:
            {
                'id': model_run_id,
                'uploadUrls': {
                    'moodQuestionUploadUrl': str,
                    'readAloudUploadUrl': str
                }
            }
        """

    async def upload_audio(self, upload_url: str, audio_data: bytes):
        """Upload audio to presigned URL"""

    async def poll_results(self, model_run_id: str, max_attempts: int = 60) -> dict:
        """
        Poll for Apollo results.

        Returns:
            {
                'status': 'COMPLETE_OK' | 'COMPLETE_ERROR' | 'FAILED',
                'results': {
                    'disorders': {
                        'depression': {'probability': float, 'severity': str},
                        'anxiety': {'probability': float, 'severity': str}
                    }
                }
            }
        """
```

### Phase 2: Extension State Management

**Update**: `agents/ten_packages/extension/thymia_analyzer_python/extension.py`

**New States**:
```python
class AnalysisMode(Enum):
    HELLOS_SCREENING = "hellos_screening"      # Continuous speech analysis
    APOLLO_MOOD_PROMPT = "apollo_mood_prompt"  # Waiting for mood response
    APOLLO_MOOD_COLLECT = "apollo_mood_collect" # Recording mood audio
    APOLLO_READ_PROMPT = "apollo_read_prompt"  # Waiting for read aloud
    APOLLO_READ_COLLECT = "apollo_read_collect" # Recording read aloud
    APOLLO_PROCESSING = "apollo_processing"    # API call in progress
```

**New Config**:
```python
apollo_enabled: bool = True
apollo_trigger_threshold: dict = {
    "distress": 50,
    "low_self_esteem": 40,
    "any_metric": 60
}
apollo_read_text: str = "The quick brown fox jumps over the lazy dog"
```

### Phase 3: LLM Tool Updates

**New Tool**: `trigger_apollo_assessment`
```python
{
    "name": "trigger_apollo_assessment",
    "description": "Trigger detailed clinical assessment for depression and anxiety. Call this after Hellos metrics indicate concern. This will guide the user through two short audio tasks.",
    "parameters": []
}
```

**New Tool**: `get_apollo_results`
```python
{
    "name": "get_apollo_results",
    "description": "Get Apollo clinical assessment results (depression and anxiety). Returns probability (0-100%) and severity for each condition.",
    "parameters": []
}
```

### Phase 4: Conversation Flow

**LLM System Prompt Updates**:

```
When wellness metrics show concerning levels:
- If distress > 50% OR low_self_esteem > 40% OR any metric > 60%:
  1. Acknowledge the metrics compassionately
  2. Suggest: "I can provide a more detailed assessment of depression and anxiety indicators. This takes 2 minutes and involves answering a mood question and reading a sentence aloud. Would you like to proceed?"
  3. If user agrees, call trigger_apollo_assessment tool

During Apollo assessment:
- First prompt: "How are you feeling today? Please take a moment to describe your emotional state."
- After mood response: "Thank you. Now, please read this sentence aloud: [read_text]"
- After read aloud: "Processing your assessment... this may take a minute."
- Call get_apollo_results and share findings compassionately
```

### Phase 5: Audio Segmentation

**Challenges**:
- Need to extract specific audio segments from continuous stream
- Must maintain audio quality (WAV format, proper sample rate)
- Need clear start/stop markers for each segment

**Implementation**:
```python
class AudioSegmentCollector:
    """Collects specific audio segments for Apollo API"""

    def __init__(self):
        self.current_segment = None  # 'mood' or 'read'
        self.segment_buffers = {
            'mood': bytearray(),
            'read': bytearray()
        }

    def start_segment(self, segment_type: str):
        """Start collecting audio for segment"""
        self.current_segment = segment_type

    def add_audio_frame(self, frame_data: bytes):
        """Add audio frame to current segment"""
        if self.current_segment:
            self.segment_buffers[self.current_segment].extend(frame_data)

    def stop_segment(self) -> bytes:
        """Stop collecting and return segment as WAV"""
        segment_data = bytes(self.segment_buffers[self.current_segment])
        self.segment_buffers[self.current_segment] = bytearray()
        self.current_segment = None
        return self._convert_to_wav(segment_data)
```

### Phase 6: Testing Plan

**Test Cases**:

1. **Happy Path - Sequential**:
   - User talks naturally → Hellos returns high distress
   - LLM suggests Apollo → User agrees
   - User completes mood + read tasks
   - Apollo returns results → LLM shares compassionately

2. **Happy Path - No Apollo Needed**:
   - User talks naturally → Hellos returns healthy metrics
   - No Apollo trigger → Session ends normally

3. **User Declines Apollo**:
   - Hellos triggers Apollo suggestion
   - User declines → Respect choice, offer resources

4. **Apollo API Errors**:
   - Network failure during upload
   - Timeout during polling
   - Invalid audio format
   - Handle gracefully, fall back to Hellos-only results

5. **Audio Quality Issues**:
   - Background noise
   - Short segments
   - Ensure minimum duration requirements

---

## Configuration Changes

### property.json - New Demo Graph

**Create new graph**: `flux_apollo_cartesia_heygen`

⚠️ **CRITICAL**: Extension changes must be backwards compatible. Existing graphs using thymia (hellos-only) must continue working unchanged.

```json
{
  "name": "flux_apollo_cartesia_heygen",
  "auto_start": false,
  "graph": {
    "nodes": [
      {
        "type": "extension",
        "name": "agora_rtc",
        "addon": "agora_rtc",
        "property": {
          "app_id": "${env:AGORA_APP_ID}",
          "app_certificate": "${env:AGORA_APP_CERTIFICATE|}",
          "channel": "agora_g3qhjr",
          "stream_id": 0,
          "remote_stream_id": 182837,
          "subscribe_audio": true,
          "publish_audio": true,
          "publish_data": true,
          "enable_agora_asr": false
        }
      },
      {
        "type": "extension",
        "name": "stt",
        "addon": "deepgram_ws_asr_python",
        "extension_group": "stt",
        "property": {
          "params": {
            "api_key": "${env:DEEPGRAM_API_KEY}",
            "language": "en-US",
            "model": "nova-3-flux"
          }
        }
      },
      {
        "type": "extension",
        "name": "llm",
        "addon": "openai_llm2_python",
        "extension_group": "chatgpt",
        "property": {
          "base_url": "https://api.openai.com/v1",
          "api_key": "${env:OPENAI_API_KEY}",
          "frequency_penalty": 0.9,
          "model": "${env:OPENAI_MODEL}",
          "max_tokens": 512,
          "proxy_url": "${env:OPENAI_PROXY_URL|}",
          "greeting": "Hi there! I would like to talk to you for a couple of minutes and use your voice to predict your mood and energy levels including any depression, anxiety, stress, and fatigue. Nothing will be recorded and this is purely a demonstration of what is possible now that we have trained our models with many hours of professionally labelled data. Please begin by telling me your name, sex and year of birth.",
          "prompt": "You are a mental wellness research assistant conducting a demonstration. Guide the conversation efficiently:\n\n1. First, collect name, sex, year of birth\n2. Ask: 'How are you feeling today? Please describe your mood and energy level.' (wait for response)\n3. Ask: 'Tell me about your interests and hobbies.' (wait for response - this gets us to 22+ seconds)\n4. Say: 'Thank you. While I analyze that, please read this text aloud for about 30 seconds: The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.'\n5. After reading, say: 'Perfect. I'm processing your responses now, this will take about a minute.'\n6. Wait ~60 seconds for both APIs to complete\n7. Use get_wellness_metrics tool to retrieve all results\n8. Present all 7 metrics (5 from Hellos: stress, distress, burnout, fatigue, low_self_esteem + 2 from Apollo: depression, anxiety) with empathy\n9. Frame as research indicators, not clinical diagnosis\n10. Thank them for participating in the demonstration\n\nTotal session time: ~2-3 minutes (30s user info + 30s mood/interests + 30s reading + 60s processing)",
          "max_memory_length": 10
        }
      },
      {
        "type": "extension",
        "name": "tts",
        "addon": "cartesia_tts2",
        "extension_group": "tts",
        "property": {
          "dump": false,
          "dump_path": "./",
          "params": {
            "api_key": "${env:CARTESIA_API_KEY}",
            "voice_id": "71a7ad14-091c-4e8e-a314-022ece01c121",
            "model_id": "sonic-english",
            "output_format": "pcm_16000"
          }
        }
      },
      {
        "type": "extension",
        "name": "thymia_analyzer",
        "addon": "thymia_analyzer_python",
        "extension_group": "default",
        "property": {
          "api_key": "${env:THYMIA_API_KEY}",
          "analysis_mode": "demo_dual",
          "apollo_enabled": true,
          "apollo_read_text": "The quick brown fox jumps over the lazy dog",
          "min_speech_duration": 22.0,
          "segment_collection": {
            "mood_prompt": "How are you feeling today",
            "read_prompt": "please read this sentence aloud"
          }
        }
      },
      {
        "type": "extension",
        "name": "avatar",
        "addon": "heygen_avatar_python",
        "extension_group": "default",
        "property": {
          "heygen_api_key": "${env:HEYGEN_API_KEY|}",
          "agora_appid": "${env:AGORA_APP_ID}",
          "agora_appcert": "${env:AGORA_APP_CERTIFICATE|}",
          "channel": "agora_g3qhjr",
          "agora_avatar_uid": 12345,
          "input_audio_sample_rate": 16000
        }
      },
      {
        "type": "extension",
        "name": "main_control",
        "addon": "main_python",
        "extension_group": "control",
        "property": {
          "greeting": "Hi there! I would like to talk to you for a couple of minutes and use your voice to predict your mood and energy levels including any depression, anxiety, stress, and fatigue. Nothing will be recorded and this is purely a demonstration of what is possible now that we have trained our models with many hours of professionally labelled data. Please begin by telling me your name, sex and year of birth."
        }
      },
      {
        "type": "extension",
        "name": "message_collector",
        "addon": "message_collector2",
        "extension_group": "transcriber",
        "property": {}
      },
      {
        "type": "extension",
        "name": "streamid_adapter",
        "addon": "streamid_adapter",
        "property": {}
      }
    ]
  }
}
```

**Key Config Points**:
- **Graph name**: `flux_apollo_cartesia_heygen`
- **Channel**: `agora_g3qhjr` (same as other heygen graphs)
- **STT**: Deepgram Flux (nova-3-flux)
- **TTS**: Cartesia voice ID `71a7ad14-091c-4e8e-a314-022ece01c121`
- **Avatar**: HeyGen avatar on channel agora_g3qhjr
- **LLM Prompt**: Structured flow with mood + read aloud prompts
- **Analysis Mode**: `demo_dual` (both APIs, guided prompts)
- **Read Text**: "The quick brown fox jumps over the lazy dog"

**Backwards Compatibility**:
- Existing graphs (`flux_thymia_heygen_cartesia`, `dgv2_flux_thymia_cartesiatts`, etc.) continue using hellos-only
- Default `analysis_mode` in thymia extension must remain hellos-only
- Apollo features only activate when `analysis_mode: "demo_dual"` is explicitly set
- Extension changes must not alter behavior for graphs without apollo config

### Thymia Extension Configuration

**Updated config in `thymia_analyzer_python`** (backwards compatible):
```python
@dataclass
class ThymiaConfig(BaseConfig):
    api_key: str = ""

    # Analysis mode - MUST default to hellos-only for backwards compatibility
    analysis_mode: str = "hellos_only"  # "hellos_only" | "demo_dual"

    # Existing hellos config (unchanged)
    min_speech_duration: float = 22.0

    # Apollo config (optional, only used when analysis_mode = "demo_dual")
    apollo_enabled: bool = False  # Default False for backwards compatibility
    apollo_read_text: str = "The quick brown fox jumps over the lazy dog"
    segment_collection: dict = field(default_factory=lambda: {
        "mood_prompt": "How are you feeling today",
        "read_prompt": "please read this sentence aloud"
    })
```

**Implementation Notes**:
- Default behavior: `analysis_mode = "hellos_only"` (existing graphs work unchanged)
- Apollo only activates when explicitly configured with `analysis_mode = "demo_dual"`
- All Apollo-related code paths must check `analysis_mode` before executing
- Existing graphs without `apollo_enabled` property continue working normally

### LLM Tool Design - Mode-Agnostic

**Key Design Principle**: The LLM doesn't need to know which analysis mode is active. The same tool works for all graphs, returning different data based on extension configuration.

**Tool**: `get_wellness_metrics` (works for both hellos-only and demo_dual)
```python
{
    "name": "get_wellness_metrics",
    "description": "Get mental wellness assessment results. Returns available metrics based on analysis completed. Always includes 5 wellness metrics (stress, distress, burnout, fatigue, low_self_esteem) as percentages 0-100. May also include clinical indicators (depression, anxiety) with probability percentages and severity levels if configured. Call after sufficient audio has been collected. Status values: 'insufficient_data' (need more audio), 'processing' (analysis running), 'complete' (results ready).",
    "parameters": []
}
```

**How it works**:
1. Extension reads `analysis_mode` from property.json config
2. LLM calls `get_wellness_metrics()` (same tool name in all graphs)
3. Extension returns data based on its mode:
   - `hellos_only`: Returns 5 wellness metrics only
   - `demo_dual`: Returns 5 wellness metrics + 2 clinical indicators
4. LLM presents whatever data it receives

**LLM doesn't know or care about the mode** - it just follows its prompt and presents the results it gets.

**Response format**:
```json
{
  "status": "complete",
  "hellos": {
    "stress": 23,
    "distress": 56,
    "burnout": 12,
    "fatigue": 78,
    "low_self_esteem": 34
  },
  "apollo": {
    "depression": {
      "probability": 73,
      "severity": "MODERATE"
    },
    "anxiety": {
      "probability": 45,
      "severity": "MILD"
    }
  },
  "message": "Analysis complete"
}
```

---

## API Endpoints Comparison

### Hellos API (Current)
```
POST https://api.thymia.ai/v1/models/hellos
Headers:
  x-api-key: <THYMIA_API_KEY>
  Content-Type: multipart/form-data

Body:
  audio: <WAV file>
  user: {"userLabel": "...", "dateOfBirth": "...", "birthSex": "...", "locale": "..."}

Response:
  {
    "metrics": {
      "stress": 23, "distress": 56, "burnout": 12,
      "fatigue": 78, "low_self_esteem": 34
    }
  }
```

### Apollo API (New)
```
1. Create Model Run:
POST https://api.thymia.ai/v1/models/apollo
Headers:
  x-api-key: <THYMIA_API_KEY>
  Content-Type: application/json

Body:
  {
    "user": {"userLabel": "...", "dateOfBirth": "...", "birthSex": "..."},
    "language": "en-GB",
    "deleteData": false
  }

Response:
  {
    "id": "<model_run_id>",
    "uploadUrls": {
      "moodQuestionUploadUrl": "https://...",
      "readAloudUploadUrl": "https://..."
    }
  }

2. Upload Audio (2x):
PUT <uploadUrl>
Headers:
  Content-Type: audio/wav
Body:
  <binary WAV data>

3. Poll Results:
GET https://api.thymia.ai/v1/models/apollo/<model_run_id>
Headers:
  x-api-key: <THYMIA_API_KEY>

Response:
  {
    "status": "COMPLETE_OK",
    "results": {
      "disorders": {
        "depression": {"probability": 0.73, "severity": "MODERATE"},
        "anxiety": {"probability": 0.45, "severity": "MILD"}
      }
    }
  }
```

---

## Rollout Plan

### Stage 1: Development (Week 1)
- [ ] Implement ApolloAPI client class
- [ ] Add state management to extension
- [ ] Create audio segmentation logic
- [ ] Unit tests for Apollo client

### Stage 2: Integration (Week 1-2)
- [ ] Add LLM tools (trigger_apollo_assessment, get_apollo_results)
- [ ] Update system prompts for Apollo flow
- [ ] Integrate with existing Hellos flow
- [ ] Add configuration options to property.json

### Stage 3: Testing (Week 2)
- [ ] Test sequential mode with real users
- [ ] Verify audio quality requirements
- [ ] Test error handling and edge cases
- [ ] Collect user feedback on flow

### Stage 4: Refinement (Week 2-3)
- [ ] Adjust trigger thresholds based on data
- [ ] Refine LLM prompts for better UX
- [ ] Add telemetry for Apollo usage
- [ ] Documentation updates

### Stage 5: Deployment (Week 3)
- [ ] Deploy to staging environment
- [ ] A/B test with subset of users
- [ ] Monitor Apollo API usage and costs
- [ ] Full production rollout

---

## Open Questions

1. **Audio Duration Requirements**:
   - What is minimum duration for each Apollo audio segment?
   - Can segments be too long?

2. **Read Aloud Text**:
   - Is there recommended text for read aloud task?
   - Does language matter for the text?
   - Should text change per session?

3. **API Rate Limits**:
   - What are rate limits for Apollo API?
   - Cost per API call?
   - Batch processing options?

4. **User Consent**:
   - Do we need explicit consent before Apollo?
   - Should consent be recorded?
   - Privacy implications for clinical data?

5. **Results Interpretation**:
   - How should we present probability scores to users?
   - What severity thresholds warrant professional referral?
   - Should we provide crisis resources automatically?

6. **Multi-language Support**:
   - Does Apollo support same languages as Hellos?
   - Different read aloud texts per language?

---

## Success Metrics

**Technical**:
- Apollo API success rate > 95%
- Audio upload success rate > 98%
- Average time to results < 60 seconds
- Zero critical errors in production

**User Experience**:
- Task completion rate > 80% when Apollo triggered
- User satisfaction score > 4.0/5.0
- Less than 10% user drop-off during Apollo tasks

**Clinical**:
- Apollo results correlate with Hellos high-concern metrics
- Appropriate referrals increase by 20%
- False positive rate < 15%

---

## Risk Mitigation

**Risk**: Apollo adds friction to user experience
- **Mitigation**: Only trigger when Hellos indicates concern
- **Fallback**: Allow users to decline, provide Hellos-only results

**Risk**: Audio quality issues affect Apollo accuracy
- **Mitigation**: Validate audio before upload, provide user feedback
- **Fallback**: Retry audio collection if quality insufficient

**Risk**: Apollo API downtime
- **Mitigation**: Implement timeout and retry logic
- **Fallback**: Gracefully degrade to Hellos-only mode

**Risk**: Increased API costs
- **Mitigation**: Smart triggering based on thresholds
- **Monitoring**: Track Apollo API usage and cost per session

---

## Conclusion

**Selected Approach**: **Option E (Demo Mode - Dual API)** ✅

### Implementation Summary

**What**: Run both Hellos and Apollo APIs efficiently on a 2-3 minute session with optimized parallel processing.

**Flow** (Optimized):
1. **Opening greeting** (agent speaks first with full disclosure)
2. **Collect user info**: Name, sex, year of birth (~30s)
3. **Mood & interests** (~30s):
   - "How are you feeling today? Describe your mood and energy level." → Apollo mood audio
   - "Tell me about your interests and hobbies." → Reaches 22+ seconds for Hellos
   - ✅ **Hellos API triggered** (starts processing)
4. **Reading task** (~30s, while Hellos processes):
   - "While I analyze that, please read this text for 30 seconds..." → Apollo read audio
   - ✅ **Apollo API triggered** (upload mood + read audio, poll for results)
5. **Parallel processing** (~60s):
   - Hellos: Processing mood + interests audio
   - Apollo: Processing mood + reading audio
6. **Results delivered**: 7 metrics (5 wellness + 2 clinical)

**Total Time**: ~2.5 minutes (30s info + 30s mood/interests + 30s reading + 60s processing)

**Configuration**:
- **Graph name**: `flux_apollo_cartesia_heygen`
- **Channel**: `agora_g3qhjr`
- **STT**: Deepgram Flux
- **TTS**: Cartesia voice ID `71a7ad14-091c-4e8e-a314-022ece01c121`
- **Avatar**: HeyGen
- **Analysis mode**: `demo_dual`
- **Read text**: "The quick brown fox jumps over the lazy dog"

**Benefits**:
- ✅ Transparent consent upfront
- ✅ Comprehensive 7-metric assessment
- ✅ Proper Apollo audio (prompted mood + reading)
- ✅ Single 2-3 minute session
- ✅ Demo-appropriate transparency

**Next Steps**:
1. ✅ Plan approved - Demo Mode selected
2. ✅ Graph name confirmed: `flux_apollo_cartesia_heygen`
3. Implement Apollo API client (`apollo_api.py`)
4. Add audio segmentation logic to thymia extension (backwards compatible)
5. Create `flux_apollo_cartesia_heygen` graph in property.json
6. Update LLM tools and prompts
7. Test with real users
8. Verify existing graphs still work (no regression)
9. Deploy to https://oai.agora.io:453

---

**Document Status**: ✅ Approved - Ready for implementation
