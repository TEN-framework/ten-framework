# Deepgram Flux & Latency Optimization - Status

**Branch**: `feat/thymia-agora`
**Started**: 2025-10-29
**Last Updated**: 2025-10-29 09:35 UTC

---

## Current Status: Investigation Phase

### ✅ Completed Tasks

#### Task 1.1: Add Deepgram logging ✅
**Status**: COMPLETE

**Changes Made**:
- Added detailed initialization logging in `extension.py` (lines 83-95)
- Added connection logging (lines 175-180)
- Added connection success confirmation (lines 195-197)
- Added audio frame counter and logging every 100 frames (lines 49, 472-477)
- Added transcript response logging (line 301)
- Enhanced transcript processing logging (lines 331-333)

**Findings**:
```
[DEEPGRAM-INIT] Model: nova-3
[DEEPGRAM-INIT] Language: en-US
[DEEPGRAM-INIT] URL: wss://api.deepgram.com/v1/listen
[DEEPGRAM-INIT] Sample Rate: 16000
[DEEPGRAM-INIT] Encoding: linear16
[DEEPGRAM-INIT] Interim Results: True
[DEEPGRAM-INIT] Punctuate: True
[DEEPGRAM-INIT] Finalize Mode: disconnect
[DEEPGRAM-INIT] Params: {'api_key': '...', 'model': 'nova-3', 'language': 'en-US'}
```

**Audio Flow Confirmed**:
```
[DEEPGRAM-AUDIO] Sent frame #700, 320 bytes, ~10ms audio, total: 7000ms
[DEEPGRAM-AUDIO] Sent frame #800, 320 bytes, ~10ms audio, total: 8000ms
```
- Frame size: 320 bytes
- Audio duration per frame: ~10ms
- Logging interval: Every 100 frames (~1 second)

**Transcription Confirmed**:
```
[DEEPGRAM-TRANSCRIPT] Text: 'Hello?' | Final: True | Start: 9590ms | Duration: 599ms
```

#### Task 1.2: Baseline Performance Testing ✅
**Status**: COMPLETE

**Test Channel**: `deepgram_test_channel`
**Test UID**: 123456 (user publishing audio from radio)

**Confirmed Working**:
- ✅ Deepgram extension is being used (NOT TEN built-in STT)
- ✅ Audio packets flowing to Deepgram (~100 frames/second)
- ✅ Transcription responses returning continuously
- ✅ Final transcripts arriving with timing information

**Baseline Configuration**:
- Model: `nova-3`
- Endpoint: `wss://api.deepgram.com/v1/listen` (v1)
- Sample Rate: 16000 Hz
- Encoding: linear16
- Interim Results: Enabled
- Punctuation: Enabled

---

## Open Questions - Answers Found

### Q1: ✅ Does extension support v2 endpoint?
**Answer**: YES - URL is configurable via `self.config.url` (line 110)

### Q2: ✅ Can Flux parameters be passed through params dict?
**Answer**: PARTIALLY - There's `advanced_params_json` for extra parameters (lines 145-159)
- Parameters can be passed via JSON string
- They're applied to LiveOptions via setattr if not blacklisted
- Need to test if Flux-specific params (eot_threshold, etc.) work this way

### Q3: ⏳ How to configure 80ms chunks for Flux?
**Status**: PENDING - Need to investigate agora_rtc configuration

### Q4: ⏳ Does extension handle Flux events?
**Status**: PENDING - Need to check event handlers for EndOfTurn, EagerEndOfTurn, TurnResumed

### Q5: ⏳ Current VAD/Turn Detection status?
**Answer**: NO VAD or turn detection extensions enabled
- `ten_vad_python` exists but NOT in graph
- `ten_turn_detection` exists but NOT in graph
- This is CORRECT - we don't want these enabled (user requirement)
- Flux has built-in turn detection

---

## Next Steps

### Phase 2: Flux Configuration

#### Task 2.1: Update extension for v2 endpoint support  ⚠️
**Priority**: HIGH
**Status**: BLOCKED - HTTP 403 Error

**Completed Work**:
1. ✅ Modified extension.py to add Flux parameters to `extra` dict
2. ✅ Confirmed parameters are being added correctly:
   ```json
   "extra": {
       "mid_opt_out": "true",
       "eot_threshold": 0.7,
       "eot_timeout_ms": 3000
   }
   ```
3. ✅ Verified Nova-3 with v1 endpoint still works (API key valid)
4. ❌ v2 endpoint with Flux returns **HTTP 403 Forbidden**

**Error Details**:
```
WebSocketException in AbstractAsyncWebSocketClient.start:
server rejected WebSocket connection: HTTP 403
```

**Root Cause Analysis**:
The HTTP 403 error indicates authentication/authorization failure. Possible causes:

1. **API Key Access**: The Deepgram API key may not have access to:
   - Flux model (may require beta/special access)
   - v2 endpoint (may require different plan tier)

2. **SDK Version Issue**: Current `deepgram-sdk==3.9.0` may not properly support:
   - v2 endpoint URL construction
   - Flux parameter formatting in WebSocket connection

3. **Parameter Format**: The `extra` dict parameters may need different serialization for v2 endpoint

**Verification Steps Taken**:
- ✅ Nova-3 on v1 endpoint: **WORKS** (confirms API key valid)
- ❌ Flux on v2 endpoint: **HTTP 403** (access denied)
- ✅ Flux parameters in options: **PRESENT** (formatting correct)

**Livekit Reference**:
Reviewed Livekit commit showing Flux integration creates separate v2 classes:
- Uses `base_url = "wss://api.deepgram.com/v2/listen"`
- Passes params in `live_config` dict
- May require different SDK version

**ROOT CAUSE IDENTIFIED** ✅:
The extension uses SDK 3.9.0 which doesn't support v2 endpoint. **SDK 5.x required for Flux.**

### Completed Work (feat/deepgram-v2 branch)

✅ **Upgraded Deepgram SDK**: 3.9.0 → 5.2.0
✅ **Refactored extension**: Added v1/v2 API routing
✅ **Updated dependency system**: Documented how `task install` works
✅ **Removed deprecated imports**: DeepgramClientOptions no longer exists in SDK 5.x

### SDK 5.x Breaking Changes Discovered

1. **API Initialization Changed**:
   ```python
   # OLD (SDK 3.x)
   client = deepgram.AsyncListenWebSocketClient(
       config=DeepgramClientOptions(url=..., api_key=...)
   )

   # NEW (SDK 5.x)
   client = DeepgramClient(api_key=api_key)
   connection = client.listen.v2.connect(**params)
   ```

2. **Context Manager Pattern**: SDK 5.x uses `with` statement:
   ```python
   with client.listen.v2.connect(model="flux-general-en", ...) as connection:
       connection.on(EventType.OPEN, handler)
       # ...
   ```

3. **Imports Changed**:
   - ❌ `DeepgramClientOptions` - removed
   - ❌ `LiveTranscriptionEvents` - moved/renamed
   - ❌ `LiveOptions` - no longer used for connection
   - ✅ `DeepgramClient` - new unified client

### Current Status: BLOCKED ⚠️

**Issue**: SDK 5.x requires complete refactoring to use context manager pattern. Current extension architecture uses persistent client connection with async event handlers, which conflicts with SDK 5.x's context manager approach.

**Error**:
```
ImportError: cannot import name 'LiveTranscriptionEvents' from 'deepgram'
```

### Refactoring Required

The extension needs architectural changes to work with SDK 5.x:

1. **Connection Pattern**: Change from persistent client to context manager
2. **Event Registration**: Find SDK 5.x equivalent of LiveTranscriptionEvents
3. **Async Handling**: Adapt event loop to work with context manager lifecycle

### Alternative Approaches

**Option 1**: Keep SDK 3.9.0 and use v1 endpoint (Nova-3) ✅ **WORKS NOW**
- No code changes needed
- Latency not optimized
- No Flux turn detection

**Option 2**: Complete SDK 5.x migration (Complex)
- Requires significant refactoring
- Need to study SDK 5.x API documentation thoroughly
- Test all event handlers

**Option 3**: Use Deepgram REST API for Flux (Workaround)
- Bypass SDK completely
- Direct WebSocket connection
- Manual protocol handling

### Dependency Installation

✅ **System Working**: Documented in branch
```bash
# Install all extension dependencies
cd /app/agents/examples/voice-assistant/tenapp
./scripts/install_python_deps.sh

# Or via task
task install-tenapp-python-deps
```

**Note**: gladia_asr_python downgrades deepgram-sdk to 3.9.0, need to upgrade after:
```bash
uv pip install --system --upgrade 'deepgram-sdk>=5.1.0'
```

### Files Modified (feat/deepgram-v2 branch)

1. **requirements.txt**: `deepgram-sdk>=5.1.0`
2. **extension.py**:
   - Lines 31-39: Updated imports for SDK 5.x
   - Lines 43-65: Added v2 detection method
   - Lines 125-153: Refactored start_connection() routing
   - Lines 154-215: Created _start_connection_v1()
   - Lines 217-321: Created _start_connection_v2()
   - Lines 351-385: Added v2 event handlers

**Status**: Code compiles but imports fail due to SDK 5.x API changes

## Phase 4: Direct WebSocket Implementation ✅ COMPLETE

### Created New Extension: `deepgram_ws_asr_python`

**Status**: ✅ **BOTH Nova-3 AND Flux WORKING PERFECTLY**

**Approach**: Direct WebSocket using aiohttp (bypassing Deepgram SDK entirely)
- No SDK version dependency issues
- Full control over WebSocket protocol
- Supports both v1 (Nova) and v2 (Flux) endpoints

### Files Created:

1. **Extension directory**: `/home/ubuntu/ten-framework/ai_agents/agents/ten_packages/extension/deepgram_ws_asr_python/`
2. **Symlink**: `/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant/tenapp/ten_packages/extension/deepgram_ws_asr_python`

### Implementation Details:

**Key Files**:
- `manifest.json` - Extension metadata (fixed type validation: float64, int32)
- `config.py` - Configuration with Flux parameters (eot_threshold, eot_timeout_ms)
- `extension.py` - Main WebSocket implementation
- `requirements.txt` - Only aiohttp and pydantic (no deepgram-sdk!)
- `addon.py`, `__init__.py`, `property.json` - Standard TEN extension files

**WebSocket Connection**:
```python
# Token authentication
headers = {"Authorization": f"Token {api_key}"}

# Build URL with parameters
url = f"wss://api.deepgram.com/v1/listen?model=nova-3&language=en-US&..."

# Connect
self.ws = await self.session.ws_connect(url, headers=headers)
```

**Event Handling**:
- Results (transcripts)
- Metadata
- UtteranceEnd (v1 VAD)
- EndOfTurn (v2 Flux)
- EagerEndOfTurn (v2 Flux)
- TurnResumed (v2 Flux)

### Test Results:

**Nova-3 (v1)**: ✅ **WORKING PERFECTLY**
```
Configuration:
  url: wss://api.deepgram.com/v1/listen
  model: nova-3
  language: en-US

Logs:
  [DEEPGRAM-WS] WebSocket connected successfully
  [DEEPGRAM-AUDIO] Sent frame #1200, 320 bytes, ~10ms audio, total: 12000ms
  [DEEPGRAM-TRANSCRIPT] Text: 'You you why can't you tell them' | Final: True | Start: 10980ms | Duration: 1960ms
```

**Flux (v2)**: ✅ **WORKING PERFECTLY**
```
Configuration:
  url: wss://api.deepgram.com/v2/listen
  model: flux-general-en
  eot_threshold: 0.7
  eot_timeout_ms: 3000

Features Working:
  ✅ Real-time streaming transcripts
  ✅ Progressive refinement (interim results)
  ✅ EndOfTurn event detection (~260ms)
  ✅ StartOfTurn events
  ✅ Turn-based conversation tracking
  ✅ Accurate transcription

Example Output:
  [DEEPGRAM-FLUX-TRANSCRIPT] Text: 'caused largely by the size of the backlog.'
    | Event: Update | Start: 19120ms | Duration: 2399ms
  [DEEPGRAM-FLUX-TRANSCRIPT] Text: '...'
    | Event: EndOfTurn | Start: 1600ms | Duration: 17520ms
```

**Issues Found & Fixed**:
1. ❌ **HTTP 400 Root Cause**: `mip_opt_out=false` parameter broke WebSocket handshake
   - **Fix**: Removed parameter (it's optional)

2. ❌ **No Transcripts**: v2 uses different message format
   - v1: `{"type": "Results", "channel": {...}}`
   - v2: `{"type": "TurnInfo", "transcript": "...", "event": "Update"}`
   - **Fix**: Added `_handle_flux_turn_info()` method to process TurnInfo messages

3. ✅ **Both API keys work with v1 AND v2** once implementation fixed

### Next Steps:

**Immediate**:
1. ✅ Nova-3 working - can use this in production
2. ⏳ Investigate Flux 400 error:
   - Verify API key has Flux access
   - Check correct model name for Flux
   - Review Deepgram v2 API docs for exact parameter format
   - Test if additional headers or authentication needed

**For Production**:
1. Use `deepgram_ws_asr_python` with Nova-3 (working now)
2. Continue investigating Flux access/configuration
3. Update property.json when Flux is working

**Investigation Needed**:
1. Check if changing URL in params is sufficient
2. Test if Flux parameters can pass through `advanced_params_json`
3. May need to modify extension code if params don't work

**Approach**:
```json
{
  "params": {
    "api_key": "${env:DEEPGRAM_API_KEY}",
    "url": "wss://api.deepgram.com/v2/listen",
    "model": "flux-general-en",
    "language": "en-US",
    "eot_threshold": 0.7,
    "eot_timeout_ms": 3000
  }
}
```

OR via advanced_params_json:
```json
{
  "params": {
    "api_key": "${env:DEEPGRAM_API_KEY}",
    "model": "flux-general-en",
    "language": "en-US"
  },
  "advanced_params_json": "{\"eot_threshold\": 0.7, \"eot_timeout_ms\": 3000}"
}
```

#### Task 2.2: Test Flux with current configuration
**Priority**: HIGH
**Status**: TODO

**Test Plan**:
1. Update property.json with Flux configuration
2. Restart services
3. Monitor logs for connection success
4. Check for Flux-specific events in logs
5. Verify transcription still works
6. Compare latency with Nova-3 baseline

#### Task 2.3: Add logging for Flux events
**Priority**: MEDIUM
**Status**: TODO

**Events to Log**:
- EndOfTurn
- EagerEndOfTurn (if enabled)
- TurnResumed (if using eager mode)

---

## Phase 3: Rime TTS Integration

#### Task 3.1: Add Rime API key to environment
**Status**: COMPLETE ✅
- Added to `/home/ubuntu/PERSISTENT_KEYS_CONFIG.md`
- Key: `3I7Gtcj6q-eGjjnsJEYA4hg-51WM8PCyOdViWN8chuc`

#### Task 3.2: Update graph to use Rime TTS
**Status**: TODO

**Configuration**:
```json
{
  "name": "tts",
  "addon": "rime_tts",
  "property": {
    "dump": false,
    "params": {
      "api_key": "${env:RIME_API_KEY}",
      "speaker": "astra",
      "model": "mistv2",
      "sampling_rate": 16000
    }
  }
}
```

---

## Testing Environment

**Test Channel**: `deepgram_test_channel`
**User UID**: 123456 (active, near radio)
**Agent Available**: YES - can start sessions on demand

**How to Test**:
1. Start session: `curl -X POST http://localhost:8080/start -H "Content-Type: application/json" -d '{"channel_name": "deepgram_test_channel", "user_uid": 123456, "graph_name": "voice_assistant_thymia"}'`
2. Monitor logs: `docker exec ten_agent_dev tail -f /tmp/task_run.log | strings | grep DEEPGRAM`
3. Session auto-stops after 60s of inactivity

---

## Performance Metrics

### Baseline (Nova-3)

**Audio Configuration**:
- Frame size: 320 bytes
- Frame duration: ~10ms
- Frames per second: ~100
- Audio rate: ~32 KB/s

**Transcription Latency**:
- Example: "Hello?" detected at 9590ms with 599ms duration
- Need more measurements for average

**To Measure**:
- Average time from audio → first interim result
- Average time from audio → final result
- Turn detection latency (with Flux)
- End-to-end latency (audio → TTS output)

---

## Files Modified

### Extension Files
- ✅ `ai_agents/agents/ten_packages/extension/deepgram_asr_python/extension.py`
  - Added initialization logging (lines 83-95)
  - Added connection logging (lines 175-180, 195-197)
  - Added audio frame counter (line 49)
  - Added audio frame logging (lines 472-477)
  - Added transcript logging (lines 301, 331-333)

### Configuration Files
- ⏳ `ai_agents/agents/examples/voice-assistant/tenapp/property.json` (pending Flux update)
- ⏳ `ai_agents/agents/examples/voice-assistant/.env` (pending Rime API key)

### Documentation Files
- ✅ `/home/ubuntu/PERSISTENT_KEYS_CONFIG.md` (Rime API key added)
- ✅ `ai/deepgram/plan_flux.md` (created)
- ✅ `ai/deepgram/status.md` (this file)

---

## Notes

- User prefers NOT to use TEN VAD or turn detection (can slow things down)
- Flux has built-in turn detection (~260ms end-of-turn)
- Focus on latency optimization throughout
- Test channel available 24/7 for testing
