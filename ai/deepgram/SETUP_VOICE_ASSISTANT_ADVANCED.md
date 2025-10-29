# Setting Up voice-assistant-advanced with Deepgram Flux

**Date**: 2025-10-29
**Purpose**: Document how to run and test voice-assistant-advanced with the new Deepgram Flux integration

---

## What Was Created

### 1. New Extension: `deepgram_ws_asr_python`
**Location**: `ai_agents/agents/ten_packages/extension/deepgram_ws_asr_python/`

**Features**:
- Direct WebSocket connection to Deepgram (no SDK dependency issues)
- Supports both v1 (Nova-3) and v2 (Flux) APIs
- Automatic API version detection based on URL or model name
- Flux turn detection with EndOfTurn/StartOfTurn events
- Configurable EOT thresholds and timeouts

**Key Files**:
- `extension.py` - Main WebSocket implementation
- `config.py` - Configuration with Flux parameters
- `manifest.json` - Extension metadata (uses float64/int32 types)
- `requirements.txt` - Only aiohttp and pydantic (no deepgram-sdk)

### 2. New Example: `voice-assistant-advanced`
**Location**: `ai_agents/agents/examples/voice-assistant-advanced/`

**Contains 3 Graphs**:
1. **voice_assistant_thymia** (auto_start: true)
   - Mental wellness analysis
   - Uses Deepgram Flux with turn detection
   - Thymia analyzer integration

2. **voice_assistant_heygen**
   - HeyGen avatar integration
   - Standard Deepgram ASR

3. **voice_assistant_generic_video**
   - Generic video avatar protocol
   - Standard Deepgram ASR

---

## How the Architecture Works

### Directory Structure
```
ai_agents/
├── server/
│   └── bin/api          # Go API server binary
├── agents/
│   ├── ten_packages/
│   │   └── extension/   # Shared extension library
│   └── examples/
│       ├── voice-assistant/         # Basic example
│       │   └── tenapp/
│       │       └── property.json    # 1 graph (voice_assistant)
│       └── voice-assistant-advanced/ # Advanced example
│           └── tenapp/
│               └── property.json     # 3 graphs (thymia, heygen, generic_video)
```

### Extension Symlinks
Each example's `tenapp/ten_packages/extension/` contains symlinks to the shared extension library:
```bash
# Example symlinks in voice-assistant-advanced
deepgram_ws_asr_python -> /app/agents/ten_packages/extension/deepgram_ws_asr_python
thymia_analyzer_python -> /app/agents/ten_packages/extension/thymia_analyzer_python
heygen_avatar_python -> /app/agents/ten_packages/extension/heygen_avatar_python
```

These are automatically created when copying from voice-assistant.

---

## How to Run voice-assistant-advanced

### Method 1: Restart API Server (Recommended)

The API server must be pointed to the correct tenapp directory.

**Step 1: Stop Current Server**
```bash
# As root (the API server runs as root)
sudo pkill -9 -f "bin/api"

# Verify stopped
ps aux | grep "bin/api"
```

**Step 2: Start Server for voice-assistant-advanced**
```bash
cd /home/ubuntu/ten-framework/ai_agents/server

# Start API server pointing to voice-assistant-advanced
sudo ./bin/api -tenapp_dir=/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp &

# Or using Task from the example directory
cd /home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced
task run-api-server &
```

**Step 3: Verify Graphs Loaded**
```bash
# Check health
curl http://localhost:8080/health

# List graphs (should show 3 graphs)
curl http://localhost:8080/graphs | python3 -m json.tool

# Expected output:
# {
#   "code": "0",
#   "data": [
#     {"name": "voice_assistant_thymia", "auto_start": true},
#     {"name": "voice_assistant_heygen", "auto_start": false},
#     {"name": "voice_assistant_generic_video", "auto_start": false}
#   ]
# }
```

### Method 2: Use Different Port

If you can't stop the existing server, run on a different port:

```bash
cd /home/ubuntu/ten-framework/ai_agents/server

# Run on port 8081
./bin/api -tenapp_dir=/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp -port 8081 &

# Test
curl http://localhost:8081/health
curl http://localhost:8081/graphs
```

---

## Testing Deepgram Flux

### Configuration

The Thymia graph uses this Deepgram configuration:

```json
{
  "addon": "deepgram_ws_asr_python",
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

### Testing Flux Turn Detection

**Start a Session**:
```bash
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{
    "graph_name": "voice_assistant_thymia",
    "channel_name": "test_channel",
    "remote_stream_id": 123
  }'
```

**Monitor Logs**:
```bash
# Look for Flux-specific logs
tail -f /tmp/worker_*.log | grep -i "FLUX\|EndOfTurn\|StartOfTurn"

# Expected log patterns:
[DEEPGRAM-WS] Using v2 API for Flux
[DEEPGRAM-WS] WebSocket connected successfully
[DEEPGRAM-FLUX-TRANSCRIPT] Text: '...' | Event: Update | Start: 0ms | Duration: 500ms
[DEEPGRAM-FLUX] EndOfTurn event received
```

### What to Look For

**Successful Flux Connection**:
1. `[DEEPGRAM-WS] Using v2 API for Flux` - Extension detected Flux model
2. `[DEEPGRAM-WS] WebSocket connected successfully` - Connection established
3. No HTTP 400 errors

**Flux Turn Detection Working**:
1. `[DEEPGRAM-FLUX-TRANSCRIPT]` - Receiving TurnInfo messages
2. `EndOfTurn` events after ~260ms of silence
3. `StartOfTurn` events when user resumes speaking
4. Progressive transcript updates

---

## Switching Between Nova and Flux

### To Use Nova-3 (v1 API)

Edit `voice-assistant-advanced/tenapp/property.json`:

```json
{
  "addon": "deepgram_ws_asr_python",
  "params": {
    "api_key": "${env:DEEPGRAM_API_KEY}",
    "url": "wss://api.deepgram.com/v1/listen",
    "model": "nova-3",
    "language": "en-US",
    "interim_results": true,
    "punctuate": true
  }
}
```

### To Use Flux (v2 API)

```json
{
  "addon": "deepgram_ws_asr_python",
  "params": {
    "api_key": "${env:DEEPGRAM_API_KEY}",
    "url": "wss://api.deepgram.com/v2/listen",
    "model": "flux-general-en",
    "language": "en-US",
    "eot_threshold": 0.7,
    "eot_timeout_ms": 3000,
    "eager_eot_threshold": 0.9
  }
}
```

**After editing**: Restart the API server and worker processes.

---

## Troubleshooting

### Issue: Graphs Not Showing

**Symptom**: `curl http://localhost:8080/graphs` only shows `voice_assistant`

**Cause**: API server is pointing to wrong tenapp directory

**Fix**:
```bash
# Check current server
ps aux | grep "bin/api"

# Should show:
# ./bin/api -tenapp_dir=.../voice-assistant-advanced/tenapp

# If not, restart with correct path
sudo pkill -9 -f "bin/api"
cd /home/ubuntu/ten-framework/ai_agents/server
sudo ./bin/api -tenapp_dir=/home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp &
```

### Issue: Extension Not Found

**Symptom**: `deepgram_ws_asr_python extension not found`

**Cause**: Missing symlink or extension not installed

**Fix**:
```bash
# Check symlink exists
ls -la ai_agents/agents/examples/voice-assistant-advanced/tenapp/ten_packages/extension/deepgram_ws_asr_python

# Should point to: /app/agents/ten_packages/extension/deepgram_ws_asr_python

# If missing, create symlink
cd ai_agents/agents/examples/voice-assistant-advanced/tenapp/ten_packages/extension/
ln -s /app/agents/ten_packages/extension/deepgram_ws_asr_python deepgram_ws_asr_python
```

### Issue: HTTP 400 from Deepgram

**Symptom**: `WebSocket handshake failed: HTTP 400`

**Cause**: Invalid parameters for v2 endpoint

**Fix**: The extension automatically handles this, but if you see this error:
1. Check logs for which parameters are being sent
2. Ensure v2 endpoint only uses: model, sample_rate, encoding, eot_* parameters
3. Do NOT include v1 parameters like: language, channels, interim_results, punctuate

The extension's `_build_websocket_url()` method handles this automatically based on `is_v2_endpoint()` detection.

### Issue: No Transcripts

**Symptom**: Connected but no transcript output

**For v1 (Nova)**: Check for `[DEEPGRAM-TRANSCRIPT]` logs
**For v2 (Flux)**: Check for `[DEEPGRAM-FLUX-TRANSCRIPT]` logs

**Cause**: Message format mismatch

**Fix**: Already handled in extension:
- v1 uses `_handle_transcript()` for "Results" messages
- v2 uses `_handle_flux_turn_info()` for "TurnInfo" messages

---

## Environment Variables

Ensure these are set in `.env`:

```bash
# Required
DEEPGRAM_API_KEY=your_key_here
AGORA_APP_ID=your_agora_app_id
OPENAI_API_KEY=your_openai_key
ELEVENLABS_TTS_KEY=your_elevenlabs_key

# For HeyGen graph
HEYGEN_API_KEY=your_heygen_key

# For Generic Video graph
GENERIC_VIDEO_API_KEY=your_key
```

**IMPORTANT**: After changing `.env`, restart the Docker container OR source it before starting processes:
```bash
set -a
source .env
set +a
```

---

## Next Steps After This Works

1. **Test All 3 Graphs**:
   - Test Thymia with Flux
   - Test HeyGen avatar
   - Test Generic Video avatar

2. **Performance Tuning**:
   - Adjust `eot_threshold` (0.5-0.9) for turn detection sensitivity
   - Adjust `eot_timeout_ms` (1000-5000) for silence duration
   - Try `eager_eot_threshold` for faster interruptions

3. **Production Deployment**:
   - Build Docker image with voice-assistant-advanced
   - Configure health checks
   - Set up logging and monitoring
   - Document API endpoints for frontend

---

## Files Modified in Git

**Branch**: `feat/deepgram-v2`
**Commit**: `20cf9dbaf`

**New Files**:
- `ai_agents/agents/ten_packages/extension/deepgram_ws_asr_python/` (entire extension)
- `ai_agents/agents/examples/voice-assistant-advanced/` (entire example)
- `ai/deepgram/status.md` (testing documentation)
- `ai/deepgram/plan_flux.md` (implementation plan)

**Modified Files**:
- `ai_agents/agents/examples/voice-assistant/tenapp/property.json` (removed advanced graphs)

**Review Links**:
- Commit: https://github.com/TEN-framework/ten-framework/commit/20cf9dbaf
- Compare: https://github.com/TEN-framework/ten-framework/compare/main...feat/deepgram-v2

---

## Key Learnings for Future

### 1. Extension Symlinks
Every example needs symlinks to shared extensions. These are created automatically when copying directory structure.

### 2. API Server Architecture
The Go API server (`server/bin/api`) loads graphs from a specific tenapp directory specified via `-tenapp_dir` flag. Only ONE API server can run per port.

### 3. Environment Variables
`.env` files must be loaded BEFORE starting processes. Changes to `.env` require process restart.

### 4. Deepgram API Versions
- **v1**: Uses "Results" messages with `channel.alternatives[]` structure
- **v2**: Uses "TurnInfo" messages with flat `transcript` field
- The extension auto-detects version based on URL or model name

### 5. Testing Without Frontend
Use `curl` to test graphs via API:
```bash
# Start session
curl -X POST http://localhost:8080/start -H "Content-Type: application/json" -d '{"graph_name":"voice_assistant_thymia","channel_name":"test"}'

# Stop session
curl -X POST http://localhost:8080/stop -H "Content-Type: application/json" -d '{"channel_name":"test"}'
```

Monitor worker logs in `/tmp/worker_*.log` for extension output.

---

**Status**: ✅ Code complete and committed
**Next Action**: Restart API server pointing to voice-assistant-advanced tenapp directory to test
