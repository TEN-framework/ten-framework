# Graph Configuration

> **When to Read This:** Load this document when you are modifying graph definitions
> in property.json, adding extensions to agent pipelines, or debugging data flow issues.

## Overview

Graphs define which extensions run and how they communicate. They are declared in
`property.json` under the `predefined_graphs` array within the `ten` namespace.

## Property.json Structure

```json
{
  "ten": {
    "log": {
      "handlers": [...]
    },
    "predefined_graphs": [
      {
        "name": "voice_assistant",
        "auto_start": true,
        "graph": {
          "nodes": [...],
          "connections": [...]
        }
      }
    ]
  }
}
```

- `name` — graph identifier, used in `/start` request's `graph_name` field
- `auto_start` — set to `true` by the server for the selected graph at runtime
- `graph.nodes` — extension instances
- `graph.connections` — data flow wiring

## Node Schema

```json
{
  "type": "extension",
  "name": "stt",
  "addon": "deepgram_asr_python",
  "extension_group": "stt",
  "property": {
    "params": {
      "api_key": "${env:DEEPGRAM_API_KEY}",
      "model": "nova-2",
      "language": "en-US",
      "sample_rate": 16000
    }
  }
}
```

| Field             | Required | Purpose                                           |
| ----------------- | -------- | ------------------------------------------------- |
| `type`            | Yes      | Always `"extension"`                              |
| `name`            | Yes      | Instance name (used in connections)               |
| `addon`           | Yes      | Extension package name (must match manifest.json) |
| `extension_group` | No       | Thread grouping for extensions                    |
| `property`        | No       | Config overrides merged with extension defaults   |

## Connection Schema

Connections define how messages flow between extensions:

```json
{
  "extension": "main_control",
  "cmd": [
    {"names": ["flush"], "dest": [{"extension": "llm"}, {"extension": "tts"}]},
    {"names": ["on_user_joined"], "source": [{"extension": "agora_rtc"}]}
  ],
  "data": [
    {"name": "text_data", "source": [{"extension": "llm"}]},
    {"name": "text_data", "dest": [{"extension": "tts"}]}
  ]
}
```

Each connection block is **from the perspective of the named extension**:
- `source` — "this extension receives X from these sources"
- `dest` — "this extension sends X to these destinations"

## Full Graph Example

A minimal `voice_assistant`-style graph skeleton based on the shipped example.
It shows the explicit node set and external connections that appear in
`agents/examples/voice-assistant/tenapp/property.json`; higher-level routing inside
`main_control` is not expanded here.

```json
{
  "name": "voice_assistant",
  "auto_start": false,
  "graph": {
    "nodes": [
      {
        "type": "extension", "name": "agora_rtc", "addon": "agora_rtc",
        "extension_group": "default",
        "property": {"app_id": "${env:AGORA_APP_ID}", "channel": "default"}
      },
      {
        "type": "extension", "name": "stt", "addon": "deepgram_asr_python",
        "extension_group": "stt",
        "property": {"params": {"api_key": "${env:DEEPGRAM_API_KEY}", "model": "nova-2"}}
      },
      {
        "type": "extension", "name": "llm", "addon": "openai_llm2_python",
        "extension_group": "chatgpt",
        "property": {"api_key": "${env:OPENAI_API_KEY}", "model": "${env:OPENAI_MODEL}"}
      },
      {
        "type": "extension", "name": "tts", "addon": "elevenlabs_tts2_python",
        "extension_group": "tts",
        "property": {"params": {"api_key": "${env:ELEVENLABS_TTS_KEY}"}}
      },
      {
        "type": "extension", "name": "main_control", "addon": "main_python",
        "extension_group": "control",
        "property": {}
      },
      {
        "type": "extension", "name": "message_collector", "addon": "message_collector2",
        "extension_group": "transcriber",
        "property": {}
      },
      {
        "type": "extension", "name": "streamid_adapter", "addon": "streamid_adapter",
        "property": {}
      }
    ],
    "connections": [
      {
        "extension": "main_control",
        "cmd": [
          {"names": ["on_user_joined", "on_user_left"], "source": [{"extension": "agora_rtc"}]}
        ],
        "data": [
          {"name": "asr_result", "source": [{"extension": "stt"}]}
        ]
      },
      {
        "extension": "agora_rtc",
        "audio_frame": [
          {"name": "pcm_frame", "dest": [{"extension": "streamid_adapter"}]},
          {"name": "pcm_frame", "source": [{"extension": "tts"}]}
        ],
        "data": [
          {"name": "data", "source": [{"extension": "message_collector"}]}
        ]
      },
      {
        "extension": "streamid_adapter",
        "audio_frame": [
          {"name": "pcm_frame", "dest": [{"extension": "stt"}]}
        ]
      }
    ]
  }
}
```

This example intentionally matches the real explicit graph wiring. If you are adding
a new TTS vendor, copy a confirmed working graph first, then change only the TTS node,
manifest dependency, and any vendor-specific properties. Do not infer missing LLM/TTS
routes from a simplified diagram.

## Connection Types Reference

| Type          | Direction | Payload            | Example Names                       |
| ------------- | --------- | ------------------ | ----------------------------------- |
| `cmd`         | Both      | Named commands     | `flush`, `tool_register`, `on_user_joined`, `chat_completion_call`, `update_configs` |
| `data`        | Both      | Named data msgs    | `asr_result`, `text_data`, `tts_text_input`, `tts_audio_start`, `tts_audio_end`, `error` |
| `audio_frame` | Both      | PCM audio streams  | `pcm_frame`                         |
| `video_frame` | Both      | Video streams      | `video_frame`                       |

## Parallel Audio Routing

When sending audio to multiple destinations, split at the **source node**:

```json
// CORRECT — split at agora_rtc (source)
{
  "extension": "agora_rtc",
  "audio_frame": [
    {"name": "pcm_frame", "dest": [
      {"extension": "stt"},
      {"extension": "vad"}
    ]}
  ]
}
```

Do NOT split from intermediate nodes — this causes runtime crashes.

## Property Injection

When the server processes a `/start` request, it dynamically modifies the graph:

1. **Graph selection**: Filters `predefined_graphs` to match `graph_name`, sets `auto_start: true`
2. **Channel injection**: Scans all nodes — any node with a `"channel"` property gets `channel_name` injected
3. **Start params**: Injects `remote_stream_id`, `bot_stream_id`, `token` via `startPropMap`
4. **Extension overrides**: Merges `req.Properties[extensionName]` into matching node properties
5. **Env var validation**: Resolves all `${env:VAR}` references

This is why `agora_rtc` and any custom extension with a `"channel"` property automatically
receive the dynamic channel name without code changes.

## Adding a New Graph

1. Add a new entry to `predefined_graphs[]` in the example's `tenapp/property.json`
2. Ensure all referenced extensions are listed in `tenapp/manifest.json`
3. Run `task install` (not bare `tman install` — it can wipe `bin/worker`)
4. **Nuclear restart** required (frontend caches the graph list)

## Generating property.json with rebuild_property.py

For complex deployments with many graph variants, hand-editing property.json is
error-prone. The `voice-assistant-advanced` example uses a Python script to generate
it programmatically:

**Location**: `agents/examples/voice-assistant-advanced/tenapp/rebuild_property.py`

**Usage**:
```bash
docker exec ten_agent_dev bash -c \
  "cd /app/agents/examples/voice-assistant-advanced/tenapp && python3 rebuild_property.py"
```

### How It Works

The script defines reusable **node configs** as Python dicts, then assembles them
into graphs with helper functions:

```python
# 1. Define reusable node configs
nova3_stt_100ms = {
    "type": "extension", "name": "stt", "addon": "deepgram_ws_asr_python",
    "extension_group": "stt",
    "property": {
        "params": {
            "api_key": "${env:DEEPGRAM_API_KEY}",
            "model": "nova-3", "language": "en-US",
            "interim_results": True, "endpointing": 100,
        }
    },
}

cartesia_tts_sonic3 = {
    "type": "extension", "name": "tts", "addon": "cartesia_tts",
    "extension_group": "tts",
    "property": {
        "dump": False, "dump_path": "./",
        "params": {
            "api_key": "${env:CARTESIA_TTS_KEY}",
            "model_id": "sonic-3",
            "output_format": {"container": "raw", "sample_rate": 44100},
        },
    },
}

gpt51_llm = {
    "type": "extension", "name": "llm", "addon": "openai_llm2_python",
    "extension_group": "chatgpt",
    "property": {
        "base_url": "https://api.openai.com/v1",
        "api_key": "${env:OPENAI_API_KEY}",
        "model": "gpt-5.1", "max_tokens": 1000,
        "prompt": "...", "greeting": "...",
    },
}

# 2. Define reusable connection templates
basic_connections = [
    {"extension": "main_control", "cmd": [...], "data": [...]},
    {"extension": "agora_rtc", "audio_frame": [...], "data": [...]},
    {"extension": "streamid_adapter", "audio_frame": [...]},
    {"extension": "tts", "data": [...], "audio_frame": [...]},
    # ...
]

# 3. Assemble graphs with helper functions
def create_basic_voice_assistant(name, has_avatar=False, avatar_type=None,
                                  tts_config=None, stt_config=None, llm_config=None):
    nodes = [agora_rtc_base, stt_config or nova3_stt_100ms, llm_config or ..., ...]
    connections = copy.deepcopy(basic_connections)
    if has_avatar:
        # Modify connections: route TTS audio through avatar instead of direct to RTC
        ...
    return {"name": name, "auto_start": False, "graph": {"nodes": nodes, "connections": connections}}

# 4. Build graph list and write property.json
new_graphs = [
    create_basic_voice_assistant("voice_assistant"),
    create_basic_voice_assistant("voice_assistant_heygen", has_avatar=True, avatar_type="heygen"),
    create_apollo_graph("flux_apollo_gpt_5_1_cartesia", gpt51_llm, flux_stt),
    # ...
]

new_data = {"ten": {"log": log_config, "predefined_graphs": new_graphs}}
with open("property.json", "w") as f:
    json.dump(new_data, f, indent=2)
```

### Key Patterns in rebuild_property.py

| Pattern                      | Purpose                                              |
| ---------------------------- | ---------------------------------------------------- |
| `copy.deepcopy(config)`      | Prevent mutation when reusing node configs           |
| Parametric helper functions  | `create_basic_voice_assistant(name, tts_config=...)` |
| Connection rewiring for avatars | Route TTS audio through avatar instead of direct to RTC |
| Preserve existing log config | `log_config = data["ten"]["log"]` before overwriting |
| Commented-out graph groups   | Keep old graph definitions for reference/reactivation|

### When to Use rebuild_property.py

- **Multiple graph variants** (A/B testing vendors: Deepgram vs Cartesia TTS)
- **Avatar variants** (same pipeline with/without HeyGen/Anam)
- **LLM model testing** (GPT-4o vs GPT-5.1 vs Groq)
- **Complex connection rewiring** (avatar graphs need different audio routing)

For simple single-graph setups, editing property.json directly is fine.

## Manifest.json Dependencies

When adding an extension to a graph, ensure its dependency is in `manifest.json`.
Local extensions in this repo use path-based dependencies:

```json
{
  "dependencies": [
    {"path": "../../../ten_packages/extension/my_vendor_tts_python"}
  ]
}
```

Published extensions use version-based dependencies:

```json
{
  "dependencies": [
    {"type": "extension", "name": "agora_rtc", "version": "=0.23.9-t1"}
  ]
}
```

Then run `task install` (not bare `tman install` — it can wipe `bin/worker`).

## Main Extension Customization

The "main" extension controls agent orchestration. Three variants exist:

| Variant              | Language   | Pattern                      | Use Case                   |
| -------------------- | ---------- | ---------------------------- | -------------------------- |
| Python Cascade       | Python     | ASR → LLM → TTS pipeline    | Standard voice assistant   |
| Python Realtime V2V  | Python     | OpenAI Realtime API          | Voice-to-voice (no ASR/TTS)|
| Node.js Cascade      | TypeScript | ASR → LLM → TTS pipeline    | TypeScript preference      |

Key customization points:
- `on_data()` — event routing (match/case dispatcher)
- `on_cmd()` — tool registration and handling
- Greeting logic in `on_start()` or `on_user_joined` handler

## Example Apps

Available in `agents/examples/`. Key examples:

| Example                           | Description                                          |
| --------------------------------- | ---------------------------------------------------- |
| `voice-assistant`                 | Basic: Deepgram ASR + OpenAI LLM + ElevenLabs TTS   |
| `voice-assistant-advanced`        | Multiple graph variants, vendor A/B testing          |
| `voice-assistant-realtime`        | OpenAI Realtime API (voice-to-voice, no ASR/TTS)    |
| `voice-assistant-video`           | Vision capability added                              |
| `voice-assistant-live2d`          | Live2D avatar integration                            |
| `voice-assistant-sip-twilio`      | SIP phone integration (Twilio)                       |
| `voice-assistant-sip-telnyx`      | SIP phone integration (Telnyx)                       |
| `voice-assistant-sip-plivo`       | SIP phone integration (Plivo)                        |
| `voice-assistant-with-ten-vad`    | Custom VAD (Voice Activity Detection)                |
| `voice-assistant-with-turn-detection` | Transformer-based turn detection              |
| `voice-assistant-nodejs`          | Node.js implementation                               |
| `doodler`                         | Spoken prompts → hand-drawn sketches                 |
| `speaker-diarization`             | Real-time multi-speaker identification               |
| `transcription`                   | Audio transcription tool                             |
| `websocket-example`               | WebSocket transport (no Agora RTC)                   |
| `http-control`                    | HTTP-based control interface                         |

### voice-assistant vs voice-assistant-advanced

| Aspect                | voice-assistant             | voice-assistant-advanced          |
| --------------------- | --------------------------- | --------------------------------- |
| Graphs                | 2 (`voice_assistant`, `voice_assistant_oracle`) | 4+ variants (Flux/Apollo/Cartesia)|
| Vendor switching      | Fixed components            | Multiple vendor combinations      |
| LLM prompts           | Simple greeting             | Multi-step research workflows     |
| Use case              | Getting started             | Production A/B testing            |

Both follow the same core pipeline:
```
Agora RTC → streamid_adapter → ASR → main_control → LLM → TTS → Agora RTC
```

### Real Graph: voice-assistant/tenapp/property.json

This is a complete, working graph. Key nodes:

| Node               | Addon                    | Role                               |
| ------------------ | ------------------------ | ---------------------------------- |
| `agora_rtc`        | `agora_rtc`              | Audio/video transport              |
| `streamid_adapter` | `streamid_adapter`       | Stream ID routing                  |
| `stt`              | `deepgram_asr_python`    | Speech-to-text                     |
| `llm`              | `openai_llm2_python`     | Language model                     |
| `tts`              | `elevenlabs_tts2_python` | Text-to-speech                     |
| `main_control`     | `main_python`            | Orchestration (greetings, routing) |
| `message_collector` | `message_collector2`    | Transcript collection              |

Connection wiring:
```
agora_rtc --pcm_frame--> streamid_adapter --pcm_frame--> stt
stt --asr_result--> main_control
main_control --text_data--> llm --text_data--> main_control --tts_text_input--> tts
tts --pcm_frame--> agora_rtc
```

## Portal References

- [Understanding property.json](https://github.com/TEN-framework/portal/blob/main/content/docs/ten_agent_examples/project_structure/property_json.md) [EXTERNAL]
- [Customize Agent via Code](https://github.com/TEN-framework/portal/blob/main/content/docs/ten_agent_examples/customize_agent/modify-main/index.mdx) [EXTERNAL]
- [Graph Concepts (official)](https://github.com/TEN-framework/portal/blob/main/content/docs/ten_framework/(latest)/graph/graph.md) [EXTERNAL] — message routing, connection semantics
- [Message System](https://github.com/TEN-framework/portal/blob/main/content/docs/ten_framework/(latest)/message_system.md) [EXTERNAL] — cmd/data/audio_frame type definitions

## See Also

- [Back to Architecture](../02_architecture.md)
- [Back to Workflows](../05_workflows.md)
- [Back to Interfaces](../06_interfaces.md)
