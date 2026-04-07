# 06 Interfaces

> REST API contracts, graph connection schemas, and base class abstract methods.

## REST API Endpoints

The Go server (`server/internal/http_server.go`) exposes:

| Endpoint             | Method | Purpose                              | Key Fields                        |
| -------------------- | ------ | ------------------------------------ | --------------------------------- |
| `/health`            | GET    | Health check                         | Returns `{"code":"0"}`            |
| `/graphs`            | GET    | List available graphs                | Returns `data[].name`             |
| `/start`             | POST   | Start agent session                  | `graph_name`, `channel_name`      |
| `/stop`              | POST   | Stop agent session                   | `channel_name`                    |
| `/ping`              | POST   | Keep session alive                   | `channel_name`                    |
| `/list`              | GET    | List active sessions                 | Returns worker list               |
| `/token/generate`    | POST   | Generate Agora RTC token             | `channel_name`, `uid`             |

### POST /start Request Body

```json
{
  "request_id": "uuid",
  "channel_name": "test_channel",
  "user_uid": 176573,
  "graph_name": "voice_assistant",
  "properties": {
    "openai_llm2_python": {"model": "gpt-4o-mini"}
  },
  "timeout": 60
}
```

- `properties` — per-extension overrides merged into graph node properties
- `timeout` — seconds of inactivity before auto-stop (-1 = never)

## Graph Connection Types

Connections in `property.json` define data flow between extensions:

### Command Connections (`cmd`)

```json
{"extension": "main", "cmd": [
  {"name": "tool_register", "dest": [{"extension": "llm"}]},
  {"name": "on_user_joined", "source": [{"extension": "agora_rtc"}]}
]}
```

Common commands: `tool_register`, `on_user_joined`, `flush`, `chat_completion_call`,
`update_configs`

### Data Connections (`data`)

```json
{"extension": "llm", "data": [
  {"name": "text_data", "source": [{"extension": "main"}]},
  {"name": "text_data", "dest": [{"extension": "tts"}]}
]}
```

Common data: `asr_result`, `text_data`, `tts_text_input`, `tts_audio_start`,
`tts_audio_end`, `error`

### Audio Frame Connections (`audio_frame`)

```json
{"extension": "agora_rtc", "audio_frame": [
  {"name": "pcm_frame", "dest": [{"extension": "stt"}]}
]}
```

### Video Frame Connections (`video_frame`)

```json
{"extension": "agora_rtc", "video_frame": [
  {"name": "video_frame", "dest": [{"extension": "vision"}]}
]}
```

## Base Class Abstract Methods

### ASR (`AsyncASRBaseExtension`)

| Method                      | Returns   | Purpose                          |
| --------------------------- | --------- | -------------------------------- |
| `vendor()`                  | `str`     | Vendor name (e.g., "deepgram")   |
| `start_connection()`        | `None`    | Connect to ASR service           |
| `stop_connection()`         | `None`    | Disconnect                       |
| `send_audio(frame)`         | `bool`    | Send audio frame to service      |
| `finalize()`                | `None`    | Drain pending audio              |
| `is_connected()`            | `bool`    | Connection status check          |
| `input_audio_sample_rate()` | `int`     | Expected sample rate (e.g., 16000)|

**Output helpers**: `send_asr_result()`, `send_asr_error()`, `send_asr_finalize_end()`,
`send_connect_delay_metrics()`, `send_vendor_metrics()`

### TTS (`AsyncTTS2BaseExtension`)

| Method                          | Returns  | Purpose                              |
| ------------------------------- | -------- | ------------------------------------ |
| `vendor()`                      | `str`    | Vendor name (e.g., "elevenlabs")     |
| `request_tts(tts_text_input)`   | `AsyncIterator` | Generate audio from text       |
| `cancel_tts()`                  | `None`   | Handle flush/cancellation            |
| `synthesize_audio_sample_rate()`| `int`    | Output sample rate (e.g., 24000)     |
| `synthesize_audio_channels()`   | `int`    | Channel count (default: 1)           |
| `synthesize_audio_sample_width()`| `int`   | Bytes per sample (default: 2)        |

**Output helpers**: `send_tts_audio_data()`, `send_tts_audio_start()`, `send_tts_audio_end()`,
`send_tts_error()`, `send_tts_ttfb_metrics()`, `send_tts_text_result()`

**State machine**: QUEUED → PROCESSING → FINALIZING → COMPLETED (per request)

### LLM (`AsyncLLMBaseExtension`)

| Method                          | Returns | Purpose                          |
| ------------------------------- | ------- | -------------------------------- |
| `on_call_chat_completion()`     | varies  | Handle sync command requests     |
| `on_data_chat_completion()`     | varies  | Handle stream-based data input   |
| `on_tools_update(tool_metadata)`| `None`  | Handle new tool registration     |

**Tool flow**: Extensions register tools via `CMD_TOOL_REGISTER` → LLM stores in
`available_tools` → LLM calls tools during completion → results returned.

## Manifest API Interface

Extensions declare their API interface in `manifest.json`:

```json
{
  "api": {
    "interface": [
      {"import_uri": "../../system/ten_ai_base/api/tts-interface.json"}
    ],
    "property": {
      "api_key": {"type": "string"},
      "model": {"type": "string"},
      "sample_rate": {"type": "int32"}
    }
  }
}
```

Interface JSON files define the standard cmd/data/audio_frame schemas for each extension type.

## Related Deep Dives

- [Extension Development](deep_dives/extension_development.md) — Implementing abstract methods
- [Server Architecture](deep_dives/server_architecture.md) — Endpoint handlers and property injection
- [Graph Configuration](deep_dives/graph_configuration.md) — Full connection wiring examples
