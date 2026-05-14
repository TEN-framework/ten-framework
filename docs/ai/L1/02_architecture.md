# 02 Architecture

> System design overview: extensions, graphs, connections, and the server-worker model.

## TEN Ecosystem

| Component             | Purpose                                                |
| --------------------- | ------------------------------------------------------ |
| TEN Framework         | Core runtime (C/C++, Go, Python, Node.js bindings)     |
| TEN Agent Examples    | Pre-built agent configurations (this repo's `ai_agents/`) |
| TEN VAD               | Low-latency voice activity detection                   |
| TEN Turn Detection    | Full-duplex dialogue management                        |
| TEN Portal            | Documentation and blog site                            |

## Extension System

Extensions are modular components that process data — ASR, TTS, LLM, tools, RTC, avatars, etc.
Each extension has a lifecycle:

```
on_init() → on_start() → [process messages] → on_stop() → on_deinit()
```

Every extension contains:

| File              | Purpose                                    |
| ----------------- | ------------------------------------------ |
| `addon.py`        | Registration via `@register_addon_as_extension` |
| `extension.py`    | Main logic, inherits from a base class     |
| `manifest.json`   | Metadata, dependencies, API interface      |
| `property.json`   | Default configuration values               |

**Base classes** (in `ten_ai_base/interface/ten_ai_base/`):

| Base Class                    | Use For           |
| ----------------------------- | ----------------- |
| `AsyncASRBaseExtension`       | Speech-to-text    |
| `AsyncTTS2BaseExtension`      | Text-to-speech    |
| `AsyncLLMBaseExtension`       | Chat completion   |
| `AsyncLLMToolBaseExtension`   | LLM function tools|
| `AsyncExtension`              | Generic / custom  |

## Graph-Based Configuration

Agents are assembled by defining **graphs** in `property.json`. A graph specifies
which extensions run (nodes) and how data flows between them (connections).

```json
{
  "ten": {
    "predefined_graphs": [{
      "name": "voice_assistant",
      "auto_start": true,
      "graph": {
        "nodes": [
          {"type": "extension", "name": "stt", "addon": "deepgram_asr_python", "property": {}},
          {"type": "extension", "name": "llm", "addon": "openai_llm2_python", "property": {}},
          {"type": "extension", "name": "tts", "addon": "elevenlabs_tts2_python", "property": {}}
        ],
        "connections": [...]
      }
    }]
  }
}
```

## Connection Types

| Type          | Payload              | Example                                      |
| ------------- | -------------------- | -------------------------------------------- |
| `cmd`         | Named commands       | `tool_register`, `on_user_joined`, `flush`   |
| `data`        | Named data messages  | `asr_result`, `text_data`, `tts_text_input`  |
| `audio_frame` | PCM audio streams    | `pcm_frame` (16-bit, mono, 16/24/48 kHz)    |
| `video_frame` | Video streams        | Raw video frames for vision/avatar           |

## RTC-First Design

TEN uses Agora RTC (Real-Time Communication) as the default transport, not WebSockets.

| Aspect           | RTC (default)                    | WebSocket                  |
| ---------------- | -------------------------------- | -------------------------- |
| Latency          | 50-150ms (UDP-based)             | Higher (TCP-based)         |
| Codec support    | Opus, VP8, VP9, AV1              | Raw PCM only               |
| Bandwidth adapt  | Built-in adaptation + FEC        | Manual implementation      |
| Use case         | Real-time voice/video            | Signaling, configuration   |

WebSockets are used for signaling and configuration; RTC handles the media path.

## Server-Worker Model

```
┌─────────────────┐     ┌──────────────────┐
│  Go HTTP Server  │────▶│  Worker Process   │  (one per session)
│  (port 8080)     │     │  (tman run start) │
│                  │     │                   │
│  /start → spawn  │     │  Loads graph from │
│  /stop  → kill   │     │  property.json    │
│  /ping  → keep   │     │  Runs extensions  │
└─────────────────┘     └──────────────────┘
```

- **POST /start** spawns a worker process for a channel/session
- **POST /stop** terminates the worker
- **POST /ping** keeps the session alive (if timeout != -1)

## Property Injection

When `/start` is called, the server auto-injects dynamic values into the graph:

- `channel_name` → injected into every node that has a `"channel"` property
- `remote_stream_id`, `bot_stream_id`, `token` → injected via `startPropMap`
- `req.Properties[extensionName]` → merged into specific node properties

This is future-proof: any new extension with a "channel" property automatically
receives the dynamic channel value without code changes.

## Component Diagram

```
    Client (Browser/Mobile)
           │
           ▼
    ┌──────────────┐
    │  Playground   │  Next.js frontend (port 3000)
    │  (UI)         │
    └──────┬───────┘
           │ REST API
           ▼
    ┌──────────────┐        ┌──────────────────────────────────┐
    │  Go Server    │──spawn─▶│  Worker Process                   │
    │  (port 8080)  │        │  ┌─────┐  ┌─────┐  ┌─────┐     │
    │               │        │  │ ASR │─▶│ LLM │─▶│ TTS │     │
    │               │        │  └──┬──┘  └─────┘  └──┬──┘     │
    └──────────────┘        │     │                   │        │
                             │  ┌──┴───────────────────┴──┐    │
                             │  │      Agora RTC           │    │
                             │  └─────────────────────────┘    │
                             └──────────────────────────────────┘
```

## Related Deep Dives

- [Server Architecture](deep_dives/server_architecture.md) — Go server internals, property injection pipeline
- [Graph Configuration](deep_dives/graph_configuration.md) — Node schema, connection wiring, parallel routing
