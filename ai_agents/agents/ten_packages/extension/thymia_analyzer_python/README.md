# Thymia Analyzer Extension

Mental wellness analysis extension for TEN Framework that analyzes speech patterns to provide emotional state insights.

## Overview

This extension:
- Buffers audio from the conversation in real-time
- Uses Voice Activity Detection (VAD) to collect speech
- Sends audio to Thymia API for wellness analysis
- Registers as an LLM tool for on-demand metrics retrieval
- Provides metrics: distress, stress, burnout, fatigue, and self-esteem (0-10 scale)

## Features

- **Background Analysis**: Continuously monitors speech patterns without interrupting conversation flow
- **Voice Activity Detection**: Intelligently filters silence using RMS threshold
- **LLM Tool Integration**: LLM can request wellness metrics when contextually appropriate
- **Graceful Degradation**: Fails gracefully if API unavailable, never breaks conversation
- **Privacy-Conscious**: Anonymous by default, configurable data collection
- **Continuous Monitoring**: Supports multiple analyses throughout a session

## Configuration

### Required
- `api_key` (string): Thymia API key (env: THYMIA_API_KEY)

### Optional
- `min_speech_duration` (float, default: 30.0): Minimum seconds of speech before analysis
- `silence_threshold` (float, default: 0.02): RMS threshold for voice activity detection
- `continuous_analysis` (bool, default: true): Continue analyzing throughout session
- `min_interval_seconds` (int, default: 60): Minimum seconds between analyses
- `max_analyses_per_session` (int, default: 10): Limit analyses per session
- `poll_timeout` (int, default: 120): Max seconds to wait for API results
- `poll_interval` (int, default: 5): Seconds between result polling

## Graph Configuration

Add to your graph's property.json:

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

The extension registers as `get_wellness_metrics` tool. The LLM can call it to retrieve current wellness metrics:

```json
{
  "status": "available",
  "metrics": {
    "distress": 7.2,
    "stress": 8.1,
    "burnout": 6.5,
    "fatigue": 5.8,
    "low_self_esteem": 4.3
  },
  "analyzed_seconds_ago": 12,
  "speech_duration": 32.1
}
```

### Status Values

- `available`: Metrics ready
- `analyzing`: Analysis in progress
- `insufficient_data`: Collecting speech (not enough yet)
- `no_data`: No speech collected
- `error`: Service temporarily unavailable

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
