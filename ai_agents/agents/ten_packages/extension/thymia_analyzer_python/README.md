# Thymia Analyzer Extension

Mental wellness analysis extension for TEN Framework that analyzes speech patterns to provide emotional state insights.

## Overview

This extension:
- Analyzes voice patterns to assess emotional/mental wellness states
- Supports two API modes: REST batch processing and real-time WebSocket streaming
- Registers as an LLM tool for wellness metrics retrieval
- Provides metrics: distress, stress, burnout, fatigue, self-esteem, depression, anxiety

## API Modes

### 1. REST Batch Mode (`api_mode: "rest_batch"`) - Default

Traditional batch processing using Thymia Helios API:
- Buffers audio locally (30+ seconds of speech)
- Uses Voice Activity Detection (VAD) to filter silence
- Uploads batch for analysis, polls for results
- Best for: structured interviews, defined collection phases

### 2. Sentinel Mode (`api_mode: "sentinel"`)

**Note:** Sentinel mode requires your Thymia account to have Sentinel access enabled. Contact Thymia support if you don't receive POLICY_RESULT messages after streaming audio.

Real-time streaming via WebSocket (`wss://ws.thymia.ai`):
- Streams ALL audio immediately (no client-side buffering)
- Server handles VAD and decides when to analyze
- Results pushed automatically when ready
- Supports safety classification with alerts
- Best for: natural conversations, real-time feedback

## Configuration

### Required (All Modes)
- `api_key` (string): Thymia API key (env: THYMIA_API_KEY)

### REST Batch Mode Options
- `min_speech_duration` (float, default: 30.0): Minimum seconds of speech before analysis
- `silence_threshold` (float, default: 0.02): RMS threshold for voice activity detection
- `continuous_analysis` (bool, default: true): Continue analyzing throughout session
- `min_interval_seconds` (int, default: 60): Minimum seconds between analyses
- `max_analyses_per_session` (int, default: 10): Limit analyses per session
- `poll_timeout` (int, default: 120): Max seconds to wait for API results
- `poll_interval` (int, default: 5): Seconds between result polling
- `apollo_mood_duration` (float): Duration for Apollo mood phase
- `apollo_read_duration` (float): Duration for Apollo reading phase

### Sentinel Mode Options
- `api_mode` (string): Set to `"sentinel"` to enable
- `ws_url` (string, default: `"wss://ws.thymia.ai"`): WebSocket server URL
- `biomarkers` (string, default: `"helios,apollo"`): Comma-separated list of biomarker providers
- `policies` (string, default: `"passthrough,safety_analysis"`): Analysis policies
- `forward_transcripts` (bool, default: true): Send ASR transcripts to server
- `stream_agent_audio` (bool, default: true): Stream agent audio for concordance analysis
- `auto_reconnect` (bool, default: true): Auto-reconnect on disconnect

## Graph Configuration

### REST Batch Mode (Default)

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "thymia_analyzer",
      "addon": "thymia_analyzer_python",
      "extension_group": "default",
      "property": {
        "api_key": "${env:THYMIA_API_KEY}",
        "min_speech_duration": 30.0
      }
    }
  ]
}
```

### Sentinel Mode (Real-time)

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "thymia_analyzer",
      "addon": "thymia_analyzer_python",
      "extension_group": "default",
      "property": {
        "api_key": "${env:THYMIA_API_KEY}",
        "api_mode": "sentinel",
        "ws_url": "wss://ws.thymia.ai",
        "biomarkers": "helios,apollo",
        "policies": "passthrough,safety_analysis",
        "forward_transcripts": true,
        "stream_agent_audio": true,
        "auto_reconnect": true
      }
    }
  ]
}
```

### Audio Routing

Connect audio stream to both STT and Thymia:

```json
{
  "extension": "streamid_adapter",
  "audio_frame": [{
    "name": "pcm_frame",
    "dest": [
      {"extension": "stt"},
      {"extension": "thymia_analyzer"}
    ]
  }]
}
```

### Tool Registration

Register wellness metrics tool with LLM:

```json
{
  "extension": "main_control",
  "cmd": [{
    "names": ["tool_register"],
    "source": [
      {"extension": "thymia_analyzer"}
    ]
  }]
}
```

## LLM Tool Usage

The extension registers `get_wellness_metrics` and `check_phase_progress` tools with the LLM.

### REST Batch Mode Response

```json
{
  "status": "available",
  "metrics": {
    "distress": 72,
    "stress": 81,
    "burnout": 65,
    "fatigue": 58,
    "low_self_esteem": 43
  },
  "analyzed_seconds_ago": 12,
  "speech_duration": 32.1
}
```

### Sentinel Mode: System Alert Mechanism

In Sentinel mode, the extension proactively notifies the LLM when results arrive:

1. **Server pushes POLICY_RESULT** → Extension receives biomarkers via WebSocket callback
2. **Extension compares to previous values** → Detects what changed (>5% threshold)
3. **Extension sends text_data** → A `[SYSTEM ALERT]` with specific values:
   ```
   # Initial result:
   [SYSTEM ALERT] INITIAL ANALYSIS READY. Wellness: stress=27%, distress=15%...

   # Update (only changed values):
   [SYSTEM ALERT] ANALYSIS UPDATE. Changes detected: stress: 27%→35%, fatigue: 20%→28%.
   ```
4. **LLM sees alert** → Calls `get_wellness_metrics` for full data
5. **LLM announces to user** → "I'm noticing your stress has increased to 35%..."

**Alert Types:**
- `INITIAL ANALYSIS READY` - First results, includes all values
- `INITIAL WELLNESS ANALYSIS READY` - First wellness-only results
- `INITIAL CLINICAL ANALYSIS READY` - First apollo-only results
- `ANALYSIS UPDATE` - Subsequent updates, shows only changed values (before→after)

Updates are skipped if no metric changed by more than 5%.

### Sentinel Mode Response

The LLM receives enhanced data including safety classification:

```json
{
  "status": "available",
  "analysis_mode": "real_time",
  "analysis_type": "initial",
  "metrics": {
    "distress": 27,
    "stress": 15,
    "burnout": 12,
    "fatigue": 34,
    "low_self_esteem": 8
  },
  "clinical_indicators": {
    "depression": { "probability": 23, "severity": "NONE" },
    "anxiety": { "probability": 18, "severity": "NONE" }
  },
  "safety_classification": {
    "level": "low",
    "alert": false,
    "concerns": [],
    "recommended_actions": [],
    "urgency": "routine"
  }
}
```

### Status Values

- `available`: Metrics ready
- `streaming`: (Sentinel) Audio streaming, awaiting results
- `analyzing`: Analysis in progress
- `insufficient_data`: Collecting speech (not enough yet)
- `no_data`: No speech collected
- `error`: Service temporarily unavailable

### Safety Classification Levels (Sentinel Mode)

- `low`: No concerns detected
- `medium`: Minor indicators, continue monitoring
- `high`: Significant concerns, follow recommended_actions
- `critical`: Immediate attention needed

## Example Usage

See `ai_agents/agents/examples/voice-assistant-thymia/` for complete example.

## Privacy & Security

- Uses anonymous user labels by default
- Requires explicit API key configuration
- Audio sent to Thymia API (third-party service)
- Consider user consent requirements for your use case

## API Reference

Thymia Mental Wellness API: https://api.thymia.ai/docs

## License

Copyright (c) 2024 Agora IO. All rights reserved.
