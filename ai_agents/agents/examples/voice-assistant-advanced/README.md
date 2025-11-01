# Voice Assistant Advanced

Advanced voice assistant configurations featuring avatar integration (HeyGen, Generic Video) and mental wellness analysis (Thymia) with various Deepgram STT models and TTS providers.

## Available Graphs

This example includes **7 voice assistant graphs** with different STT/TTS/feature combinations:

| Graph Name | STT | TTS | Special Features |
|------------|-----|-----|------------------|
| **voice_assistant** | Deepgram WS Flux (v2) | ElevenLabs | Basic voice assistant with Flux |
| **voice_assistant_heygen** | Deepgram ASR Nova-3 | ElevenLabs | HeyGen avatar integration |
| **voice_assistant_generic_video** | Deepgram ASR Nova-3 | ElevenLabs | Generic video avatar |
| **dgv1_nova3_rimetts** | Deepgram WS Nova-3 (v1) | Rime TTS | Basic assistant with Rime |
| **dgv2_nova3_thymia_rimetts** | Deepgram WS Nova-3 (v1) | Rime TTS | Thymia wellness analysis |
| **dgv2_flux_thymia_rimetts** | Deepgram WS Flux (v2) | Rime TTS | Thymia + Flux turn detection |
| **dgv2_flux_thymia_cartesiatts** | Deepgram WS Flux (v2) | Cartesia TTS | Thymia + Flux + Cartesia |

### STT Models Explained

- **Deepgram ASR Nova-3**: Standard HTTP-based ASR (older method)
- **Deepgram WS Nova-3 (v1)**: WebSocket streaming with Nova-3 model
- **Deepgram WS Flux (v2)**: WebSocket with Flux model featuring built-in turn detection (~260ms latency)

### Feature Highlights

**Thymia Wellness Analysis** (3 graphs):
- `dgv2_nova3_thymia_rimetts` - Nova-3 + Rime TTS
- `dgv2_flux_thymia_rimetts` - Flux + Rime TTS
- `dgv2_flux_thymia_cartesiatts` - Flux + Cartesia TTS

**Avatar Integration** (2 graphs):
- `voice_assistant_heygen` - HeyGen streaming avatar
- `voice_assistant_generic_video` - Generic video protocol

**Flux Turn Detection** (3 graphs):
- `voice_assistant` - Basic Flux + ElevenLabs
- `dgv2_flux_thymia_rimetts` - Flux + Thymia + Rime
- `dgv2_flux_thymia_cartesiatts` - Flux + Thymia + Cartesia

## Prerequisites

### Required Environment Variables (Core)

All graphs require these base credentials:

1. **Agora RTC** - Audio/video streaming platform
   - `AGORA_APP_ID` - Get from [Agora Console](https://console.agora.io/) (required)
   - `AGORA_APP_CERTIFICATE` - Optional for token authentication

2. **Deepgram STT** - Speech-to-text provider
   - `DEEPGRAM_API_KEY` - Get from [Deepgram Console](https://console.deepgram.com/) (required)

3. **OpenAI LLM** - Language model
   - `OPENAI_API_KEY` - Get from [OpenAI Platform](https://platform.openai.com/) (required)
   - `OPENAI_MODEL` - Model name (e.g., `gpt-4o`, `gpt-4o-mini`)

### TTS Provider Keys (Choose based on graph)

| TTS Provider | Environment Variable | Required For Graphs |
|--------------|---------------------|---------------------|
| **ElevenLabs** | `ELEVENLABS_TTS_KEY` | `voice_assistant`, `voice_assistant_heygen`, `voice_assistant_generic_video` |
| **Rime TTS** | `RIME_TTS_API_KEY` | `voice_assistant_thymia`, `dgv1_nova3_rimetts`, `dgv2_nova3_thymia_rimetts`, `dgv2_flux_thymia_rimetts` |
| **Cartesia TTS** | `CARTESIA_TTS_KEY` | `dgv2_flux_thymia_cartesiatts` |

### Optional Features

| Feature | Environment Variable | Required For |
|---------|---------------------|--------------|
| **Thymia Analysis** | `THYMIA_API_KEY` | Graphs with `thymia` in name (optional) |
| **HeyGen Avatar** | `HEYGEN_API_KEY` | `voice_assistant_heygen` |
| **Generic Video** | `GENERIC_VIDEO_API_KEY` | `voice_assistant_generic_video` |
| **Weather Tool** | `WEATHERAPI_API_KEY` | All graphs (optional feature) |

## Setup

### 1. Set Environment Variables

**Location**: `/home/ubuntu/ten-framework/ai_agents/.env` (only one .env file is used)

Add these variables based on which graphs you want to use:

```bash
# Core - Required for all graphs
AGORA_APP_ID=your_agora_app_id_here
AGORA_APP_CERTIFICATE=  # Optional
DEEPGRAM_API_KEY=your_deepgram_api_key_here
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# TTS Providers - Choose based on graph
ELEVENLABS_TTS_KEY=your_elevenlabs_key_here  # For voice_assistant, heygen, generic_video
RIME_TTS_API_KEY=your_rime_key_here          # For thymia graphs with Rime
CARTESIA_TTS_KEY=your_cartesia_key_here      # For dgv2_flux_thymia_cartesiatts

# Optional Features
THYMIA_API_KEY=your_thymia_key_here          # For wellness analysis (optional)
HEYGEN_API_KEY=your_heygen_key_here          # For HeyGen avatar
GENERIC_VIDEO_API_KEY=your_generic_key_here  # For generic video avatar
WEATHERAPI_API_KEY=your_weather_key_here     # For weather tool (optional)
```

**After editing .env**, restart the server:
```bash
# Option 1: Source .env and restart server (faster)
docker exec ten_agent_dev bash -c "pkill -9 -f 'bin/api'"
docker exec -d ten_agent_dev bash -c \
  "set -a && source /app/.env && set +a && \
   cd /app/server && ./bin/api -tenapp_dir=/app/agents/examples/voice-assistant-advanced/tenapp > /tmp/task_run.log 2>&1"

# Option 2: Restart container (slower but guaranteed)
cd /home/ubuntu/ten-framework/ai_agents
docker compose down && docker compose up -d
```

### 2. Install Dependencies (Inside Docker Container)

```bash
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && \
   bash scripts/install_python_deps.sh"

docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced && \
   task install"
```

### 3. Run the Voice Assistant

```bash
docker exec -d ten_agent_dev bash -c \
  "cd /app/server && \
   ./bin/api -tenapp_dir=/app/agents/examples/voice-assistant-advanced/tenapp > /tmp/task_run.log 2>&1"
```

### 4. Access the Application

- **API Server**: http://localhost:8080
- **Health Check**: `curl http://localhost:8080/health`
- **List Graphs**: `curl http://localhost:8080/graphs | jq '.data[].name'`

### 5. Test with Playground (Optional)

Start the frontend:
```bash
docker exec -d ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/playground && \
   bun run dev"
```

Access at http://localhost:3000 (or port shown in logs)

## Selecting a Graph

When starting a session via API, specify the graph name:

```bash
curl -X POST http://localhost:8080/start \
  -H "Content-Type: application/json" \
  -d '{
    "graph_name": "dgv2_flux_thymia_rimetts",
    "channel_name": "my_channel",
    "remote_stream_id": 123
  }'
```

Available graph names: see table above in "Available Graphs" section.

## Deepgram Models Configuration

### Nova-3 vs Flux

**Nova-3** (4 graphs use this):
- Uses Deepgram v1 WebSocket API: `wss://api.deepgram.com/v1/listen`
- Reliable, proven model
- **Configuration**:
  ```json
  {
    "addon": "deepgram_ws_asr_python",
    "property": {
      "params": {
        "url": "wss://api.deepgram.com/v1/listen",
        "model": "nova-3",
        "language": "en-US"
      }
    }
  }
  ```
- **No EOT parameters** - Nova-3 does not support `eot_threshold` or `eot_timeout_ms`

**Flux** (3 graphs use this):
- Uses Deepgram v2 WebSocket API: `wss://api.deepgram.com/v2/listen`
- Built-in turn detection (~260ms latency)
- EndOfTurn/StartOfTurn events
- Progressive transcript refinement
- **Configuration**:
  ```json
  {
    "addon": "deepgram_ws_asr_python",
    "property": {
      "params": {
        "url": "wss://api.deepgram.com/v2/listen",
        "model": "flux-general-en",
        "language": "en-US",
        "interim_results": true,
        "eot_threshold": 0.8,
        "eot_timeout_ms": 5000
      }
    }
  }
  ```

### Important: API Version Compatibility

⚠️ **Critical**: Nova-3 requires v1 API, Flux requires v2 API
- Nova-3 + v2 API = connection fails ❌
- Flux + v1 API = no turn detection ❌
- Nova-3 with EOT params = ignored/error ❌

## What is dgv2_nova3_thymia_rimetts?

`dgv2_nova3_thymia_rimetts` is a mental wellness voice assistant comprised of:

**Core Components:**
- **STT**: Deepgram WebSocket Nova-3 (v1 API)
- **LLM**: OpenAI GPT-4o with wellness-focused prompts
- **TTS**: Rime TTS (speaker: "cove", model: "mistv2")
- **Analyzer**: Thymia mental wellness analyzer

**Specialized Prompt:**
The LLM uses a workflow-based prompt to:
1. Collect user info (name, DOB, birth sex)
2. Engage with short questions to gather 22+ seconds of speech
3. Analyze wellness metrics via Thymia
4. Present results on 0-1 scale (0=low, 0.5=moderate, 1.0=high)

**Audio Flow:**
```
User Speech → Agora RTC → [split]
                           ↓
                           ├→ streamid_adapter → Deepgram STT → LLM
                           └→ Thymia Analyzer (parallel)
                                    ↓
                              Wellness Metrics → LLM
```

The audio is split at the source (agora_rtc) to both STT and Thymia analyzer simultaneously.

## Configuration

All graphs are configured in `tenapp/property.json`. To modify:

1. **Edit property.json** - Changes apply to new sessions automatically (no restart needed)
2. **Edit .env** - Requires server restart (see Setup section)

**Changing graph configuration**:
```bash
# Edit the file
vim /home/ubuntu/ten-framework/ai_agents/agents/examples/voice-assistant-advanced/tenapp/property.json

# No restart needed - new sessions will use updated config
# To apply to existing session, stop and restart that session
curl -X POST http://localhost:8080/stop -H "Content-Type: application/json" -d '{"channel_name": "your_channel"}'
```

## Release as Docker image

**Note**: The following commands need to be executed outside of any Docker container.

### Build image

```bash
cd ai_agents
docker build -f agents/examples/voice-assistant-advanced/Dockerfile -t voice-assistant-advanced-app .
```

### Run

```bash
docker run --rm -it --env-file .env -p 8080:8080 -p 3000:3000 voice-assistant-advanced-app
```

### Access

- Frontend: http://localhost:3000
- API Server: http://localhost:8080

## Quick Reference

### Check Server Status
```bash
curl http://localhost:8080/health
curl http://localhost:8080/graphs | jq '.data[].name'
```

### View Logs
```bash
# Real-time logs
docker exec ten_agent_dev tail -f /tmp/task_run.log

# Filter by channel
docker exec ten_agent_dev tail -f /tmp/task_run.log | grep --line-buffered "channel_name"

# Check for errors
docker exec ten_agent_dev tail -200 /tmp/task_run.log | grep -E "(ERROR|Traceback)"
```

### Manage Sessions
```bash
# List active sessions
curl http://localhost:8080/list | jq

# Stop a session
curl -X POST http://localhost:8080/stop \
  -H "Content-Type: application/json" \
  -d '{"channel_name": "your_channel"}'
```

## Learn More

- [TEN Framework Documentation](https://doc.theten.ai)
- [Deepgram API Documentation](https://developers.deepgram.com/)
  - [Deepgram Flux](https://developers.deepgram.com/docs/flux)
  - [Nova-3 Model](https://developers.deepgram.com/docs/nova-3)
- [OpenAI API Documentation](https://platform.openai.com/docs)
- [Agora RTC Documentation](https://docs.agora.io/en/rtc/overview/product-overview)
- [ElevenLabs API](https://docs.elevenlabs.io/)
- [Rime TTS API](https://docs.rime.ai/)
- [Cartesia TTS API](https://docs.cartesia.ai/)
- [HeyGen API](https://docs.heygen.com/)
