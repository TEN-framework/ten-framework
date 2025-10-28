# Avatar Graphs Testing Summary

## Overview

This document summarizes the configuration and testing of avatar graphs in the TEN Framework voice-assistant example.

## Changes Made

### 1. Property Configuration (`tenapp/property.json`)

Added two new predefined graphs to the base voice-assistant:

- **voice_assistant_heygen**: Voice assistant with HeyGen AI avatar
- **voice_assistant_generic_video**: Voice assistant with generic video generation

Both graphs are now available alongside the original `voice_assistant` graph.

### 2. Manifest Dependencies (`tenapp/manifest.json`)

Added dependencies for both avatar extensions:

- `heygen_avatar_python`
- `generic_video_python`

### 3. Graph Configuration

Both avatar graphs include:
- Standard voice pipeline: agora_rtc → STT → LLM → TTS
- Avatar node with appropriate configuration
- TTS audio routed to avatar extension (instead of directly to agora_rtc)
- All required connections and properties

## API Endpoints

### GET /graphs

Returns all available graphs:

```json
[
  {
    "name": "voice_assistant",
    "graph_id": "voice_assistant",
    "auto_start": true
  },
  {
    "name": "voice_assistant_heygen",
    "graph_id": "voice_assistant_heygen",
    "auto_start": false
  },
  {
    "name": "voice_assistant_generic_video",
    "graph_id": "voice_assistant_generic_video",
    "auto_start": false
  }
]
```

### POST /start

Clients can now start sessions with any of the three graphs by specifying the `graph_name` parameter.

## Testing Results

### Configuration Tests ✓

All configuration tests passed:

1. ✓ Graphs Endpoint - All 3 graphs properly defined
2. ✓ voice_assistant_heygen - Structure validated
3. ✓ voice_assistant_generic_video - Structure validated
4. ✓ Manifest Dependencies - Both extensions present

**Test Script**: `tenapp/test_graphs_api.py`

### Start API Tests ✓

All /start API simulation tests passed:

1. ✓ voice_assistant - Baseline graph
2. ✓ voice_assistant_heygen - HeyGen integration
3. ✓ voice_assistant_generic_video - Generic video integration
4. ✓ Error Scenarios - Invalid graph handling

**Test Script**: `tenapp/test_start_api.py`

## Graph Details

### voice_assistant_heygen

**Avatar Extension**: `heygen_avatar_python`

**Properties**:
- `heygen_api_key`: HeyGen API key
- `agora_appid`: Agora application ID
- `agora_appcert`: Agora certificate
- `channel`: Agora channel name (supports dynamic override)
- `agora_avatar_uid`: UID for avatar in Agora (default: 12345)
- `input_audio_sample_rate`: Audio sample rate (16000 Hz)

**Audio Flow**:
```
User → STT → LLM → TTS → HeyGen Avatar → Agora RTC (video)
```

**Channel Override**: ✓ Supported
- Uses standard `channel` property
- Backend API can override dynamically via `/start` endpoint

### voice_assistant_generic_video

**Avatar Extension**: `generic_video_python`

**Properties**:
- `generic_video_api_key`: Video service API key
- `agora_appid`: Agora application ID
- `agora_appcert`: Agora certificate
- `agora_channel_name`: Agora channel name
- `agora_video_uid`: UID for video in Agora (default: 12345)
- `avatar_id`: Avatar identifier
- `quality`: Video quality (low/medium/high)
- `version`: API version
- `video_encoding`: Codec (H264/VP8/AV1)
- `enable_string_uid`: Use string UIDs
- `activity_idle_timeout`: Session timeout (seconds)
- `start_endpoint`: Video service start URL
- `stop_endpoint`: Video service stop URL
- `input_audio_sample_rate`: Audio sample rate (16000 Hz)

**Audio Flow**:
```
User → STT → LLM → TTS → Generic Video → External Service → Agora RTC (video)
```

**Channel Override**: ⚠️ May need backend update
- Uses `agora_channel_name` instead of standard `channel` property
- Backend's `startPropMap` may need update to support override

## Known Issues

### 1. generic_video_python Channel Property

**Issue**: The `generic_video_python` extension uses `agora_channel_name` property instead of the standard `channel` property.

**Impact**: The backend API's `/start` endpoint may not override this property dynamically. Only properties listed in `startPropMap` get overridden.

**Workaround**: Hardcode the channel name in property.json or update backend's `startPropMap` to include `generic_video_python.agora_channel_name`.

**Recommended Fix**: Update `generic_video_python` extension to use `channel` property like `heygen_avatar_python` does.

### 2. Manifest vs Code Inconsistency

**Issue**: The `generic_video_python` manifest.json declares `agora_avatar_uid` but the extension code uses `agora_video_uid`.

**Impact**: Minor - property.json uses `agora_video_uid` which matches the code, so functionality is not affected.

**Recommended Fix**: Align manifest.json with the actual code or update code to match manifest.

## Running Tests

### Configuration Tests

```bash
cd /home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant/tenapp
python3 test_graphs_api.py
```

Expected output: All 4 tests pass

### Start API Tests

```bash
cd /home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant/tenapp
python3 test_start_api.py
```

Expected output: All 4 tests pass

## Client Usage

### Selecting a Graph

When starting a session via the `/start` API, clients can specify which graph to use:

```bash
curl -X POST http://localhost:8080/api/agents/start \
  -H "Content-Type: application/json" \
  -d '{
    "graph_name": "voice_assistant_heygen",
    "channel_name": "my_channel",
    "property": {
      "heygen_api_key": "your_key_here"
    }
  }'
```

Supported graph names:
- `voice_assistant` - Standard voice assistant (no avatar)
- `voice_assistant_heygen` - With HeyGen avatar
- `voice_assistant_generic_video` - With generic video service

### Frontend Integration

The playground frontend fetches available graphs from `/api/agents/graphs` and displays them in a dropdown selector, allowing users to choose which graph to use for their session.

## Next Steps

1. **Deploy to Server**: Deploy the updated voice-assistant to the backend server
2. **Test Live**: Verify both avatar graphs work correctly in production
3. **Fix generic_video Channel**: Consider updating generic_video_python to use standard `channel` property
4. **Update Backend**: Ensure backend's `startPropMap` includes all necessary property overrides

## Conclusion

Both avatar graphs are properly configured and tested. The configuration supports:
- ✓ Multiple graphs in a single tenapp
- ✓ Graph discovery via `/graphs` endpoint
- ✓ Dynamic graph selection via `/start` endpoint
- ✓ Proper audio routing to avatar extensions
- ✓ Complete voice pipeline integration

The graphs are ready for deployment and client testing.
