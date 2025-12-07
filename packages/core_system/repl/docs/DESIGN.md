# TEN Extension REPL - Design Document

## 1. Overview

**Goal**: Create an interactive REPL shell for TEN framework Python extension development, enabling developers to interactively test extensions without writing integration tests.

**Key Value Proposition**:
- Instant feedback loop: Edit extension → start REPL → test behavior
- Interactive exploration of extension behavior
- Simpler than writing tests for quick experimentation

## 2. Architecture

### 2.1 Component Separation

**Frontend (Server)**:
- Interactive shell UI with command parsing
- WebSocket server that accepts backend connections
- Message queue for async messages from extension
- Status line display and keybindings (e.g., Ctrl+O for pending messages)

**Backend (Client)**:
- Spawned as subprocess by frontend
- Connects to frontend's WebSocket server
- Wraps `AsyncExtensionTester` from ten_runtime
- Manages extension lifecycle (init→start→stop→deinit)
- Forwards messages between extension and frontend

**Communication**:
- WebSocket protocol with custom JSON messages
- Audio/video frame buffers encoded as base64
- Frontend spawns backend, passes socket address as CLI argument

### 2.2 Protocol Design

**Message Types (Frontend → Backend)**:

```json
{
  "type": "discover",
  "path": "."
}

{
  "type": "start",
  "addon_name": "my_extension_python",
  "properties": {"key": "value"} | "path/to/properties.json"
}

{
  "type": "send_cmd",
  "name": "hello",
  "payload": {"param": "value"}
}

{
  "type": "send_data",
  "name": "my_data",
  "payload": {"key": "value"}
}

{
  "type": "send_audio_frame",
  "file_path": "audio.wav",
  "auto_chunk": true
}

{
  "type": "send_video_frame",
  "file_path": "video.mp4",
  "auto_chunk": true
}

{
  "type": "stop"
}
```

**Message Types (Backend → Frontend)**:

```json
{
  "type": "discover_result",
  "extensions": [
    {
      "addon_name": "my_extension_python",
      "path": "/path/to/extension",
      "manifest": {...},
      "properties": {...}
    }
  ]
}

{
  "type": "start_result",
  "success": true,
  "error": null
}

{
  "type": "cmd_result",
  "status_code": "OK",
  "payload": {"detail": "world"}
}

{
  "type": "async_message",
  "message_type": "data" | "audio_frame" | "video_frame",
  "name": "my_data",
  "timestamp": "2025-12-08T12:34:56.789Z",
  "payload": {...} | "base64_encoded_buffer"
}

{
  "type": "error",
  "message": "timeout after 5s"
}

{
  "type": "log",
  "level": "info" | "debug" | "warn" | "error",
  "message": "Extension started successfully"
}
```

## 3. User Experience

### 3.1 Starting the REPL

```bash
$ cd my_extension_python
$ ten-repl

TEN Extension REPL v0.1.0
Discovering extensions in current directory...

Found extensions:
  1. my_extension_python (v1.0.0)
  2. another_extension (v2.1.0)

Select extension [1-2]: 1

Loading my_extension_python...
Properties loaded from property.json

[Extension: my_extension_python | State: stopped | Pending: 0] >>>
```

### 3.2 Interactive Commands

**Starting the extension**:
```
>>> start
Extension started successfully
[Extension: my_extension_python | State: started | Pending: 0] >>>

>>> start {"api_key": "test123"}
Extension started with custom properties
[Extension: my_extension_python | State: started | Pending: 0] >>>

>>> start path/to/custom_properties.json
Extension started with properties from file
[Extension: my_extension_python | State: started | Pending: 0] >>>
```

**Sending simple commands**:
```
>>> cmd hello
{
  "detail": "world"
}
[Extension: my_extension_python | State: started | Pending: 0] >>>

>>> cmd greet name=Jun age=30
{
  "message": "Hello Jun, you are 30 years old"
}
[Extension: my_extension_python | State: started | Pending: 0] >>>
```

**Sending complex commands with JSON**:
```
>>> cmd process_order --json '{"items": [{"id": 1, "qty": 2}], "shipping": {"address": "123 Main St"}}'
{
  "order_id": "12345",
  "total": 49.99
}
[Extension: my_extension_python | State: started | Pending: 0] >>>
```

**Sending data**:
```
>>> data my_data key1=value1 key2=value2
Sent data: my_data
[Extension: my_extension_python | State: started | Pending: 0] >>>
```

**Sending audio file**:
```
>>> audio input.wav
Auto-chunking input.wav into 10ms frames...
Sent 100 audio frames
[Extension: my_extension_python | State: started | Pending: 0] >>>
```

**Sending video file**:
```
>>> video input.mp4
Auto-chunking input.mp4...
Sent 300 video frames
[Extension: my_extension_python | State: started | Pending: 0] >>>
```

**Handling async messages**:
```
>>> cmd start_stream
{
  "stream_id": "abc123"
}
[Extension: my_extension_python | State: started | Pending: 3] >>>

# User presses Ctrl+O to view pending messages
--- Pending Messages (3) ---
[2025-12-08 12:34:56] data: stream_chunk
{
  "chunk_id": 1,
  "data": "..."
}

[2025-12-08 12:34:57] data: stream_chunk
{
  "chunk_id": 2,
  "data": "..."
}

[2025-12-08 12:34:58] data: stream_end
{
  "total_chunks": 2
}
--- End of Messages ---

[Extension: my_extension_python | State: started | Pending: 0] >>>
```

**Receiving audio frames (auto-dump)**:
```
>>> cmd start_recording
{
  "recording_id": "rec_001"
}
# Extension sends audio frames asynchronously
[Extension: my_extension_python | State: started | Pending: 50] >>>

>>> cmd stop_recording
{
  "file": "received_audio_20251208_123456.wav"
}
Audio frames auto-dumped to: received_audio_20251208_123456.wav
[Extension: my_extension_python | State: started | Pending: 0] >>>
```

**Error handling**:
```
>>> cmd nonexistent
Error: Command handler not found for 'nonexistent'
[Extension: my_extension_python | State: started | Pending: 0] >>>

>>> cmd slow_operation
Error: timeout after 5s
[Extension: my_extension_python | State: started | Pending: 0] >>>
```

**Stopping**:
```
>>> stop
Extension stopped successfully
[Extension: my_extension_python | State: stopped | Pending: 0] >>>

>>> exit
Goodbye!
```

### 3.3 Status Line Explained

```
[Extension: my_extension_python | State: started | Pending: 3] >>>
```

- **Extension**: Current extension addon name
- **State**: `stopped`, `starting`, `started`, `stopping`
- **Pending**: Number of queued async messages (shown with Ctrl+O)

## 4. Command Syntax

### 4.1 Built-in Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `start` | `start [properties]` | Start extension with optional properties (JSON string or file path) |
| `stop` | `stop` | Stop extension (auto init+start on next `start`) |
| `exit` | `exit` or `quit` | Exit REPL (auto stop+deinit) |
| `help` | `help [command]` | Show help for commands |

### 4.2 Message Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `cmd` | `cmd <name> [key=value...] [--json <json>]` | Send command to extension |
| `data` | `data <name> [key=value...] [--json <json>]` | Send data message |
| `audio` | `audio <file_path>` | Send audio file (auto-chunked to 10ms frames) |
| `video` | `video <file_path>` | Send video file (auto-chunked) |

### 4.3 Low Priority Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| `stream` | `stream <name> [params...]` | Send streaming command (uses send_cmd_ex) |

### 4.4 Parameter Parsing

**Simple key=value pairs**:
```
cmd hello name=Jun age=30
→ {"name": "Jun", "age": "30"}
```

**JSON flag for complex data**:
```
cmd hello --json '{"config": {"timeout": 30, "retries": 3}, "tags": ["a", "b"]}'
→ {"config": {"timeout": 30, "retries": 3}, "tags": ["a", "b"]}
```

## 5. File Structure

```
packages/core_system/repl/
├── pyproject.toml              # uv-managed Python project
├── manifest.json               # TEN package manifest
├── LICENSE
├── README.md
├── docs/
│   ├── DESIGN.md              # This document
│   ├── README.en-US.md
│   └── README.zh-CN.md
└── repl/
    ├── __init__.py
    ├── __main__.py            # Entry point for `python -m repl`
    ├── cli.py                 # CLI entry point (ten-repl command)
    ├── frontend/
    │   ├── __init__.py
    │   ├── server.py          # WebSocket server
    │   ├── shell.py           # Interactive shell (prompt_toolkit or similar)
    │   ├── commands.py        # Command parser and handlers
    │   ├── message_queue.py   # Async message queue
    │   └── display.py         # Output formatting, status line
    ├── backend/
    │   ├── __init__.py
    │   ├── client.py          # WebSocket client (connects to frontend)
    │   ├── audio_chunker.py   # Audio file → frames converter
    │   └── video_chunker.py   # Video file → frames converter
    ├── protocol.py            # Shared protocol definitions (message schemas)
    ├── bridge.py              # AsyncExtensionTester wrapper + discovery
    └── utils.py               # Shared utilities
```

## 6. Implementation Phases

### Phase 1: Core Infrastructure (MVP)
**Goal**: Basic REPL that can start extension and send/receive simple commands

- [ ] Set up pyproject.toml with uv (dependencies: websockets, asyncio)
- [ ] Define protocol.py (message schemas for discovery, start, cmd, results)
- [ ] Implement backend/client.py (WebSocket client, connects to frontend)
- [ ] Implement bridge.py (wrap AsyncExtensionTester, extension discovery)
- [ ] Implement frontend/server.py (WebSocket server)
- [ ] Implement frontend/shell.py (basic REPL loop with readline)
- [ ] Implement frontend/commands.py (parse: start, stop, cmd, exit)
- [ ] Test: Start extension, send `cmd hello`, receive result

### Phase 2: Enhanced UX
**Goal**: Production-ready shell experience

- [ ] Choose and integrate shell library (prompt_toolkit vs alternatives)
- [ ] Implement frontend/display.py (status line, pretty JSON formatting)
- [ ] Add command history (persist to ~/.local/share/ten/repl_history)
- [ ] Implement frontend/message_queue.py (queue async messages)
- [ ] Add Ctrl+O keybinding to display pending messages
- [ ] Add timestamp metadata to async messages
- [ ] Improve error display and handling

### Phase 3: Data Messages
**Goal**: Support sending and receiving data messages

- [ ] Extend protocol for send_data, async data reception
- [ ] Implement `data` command in frontend
- [ ] Handle async data messages in backend
- [ ] Test: Send data, receive async data

### Phase 4: Audio Support
**Goal**: Send audio files and receive audio frames

- [ ] Implement backend/audio_chunker.py (wav/mp3 → 10ms PCM frames)
- [ ] Extend protocol for send_audio_frame
- [ ] Implement `audio` command in frontend
- [ ] Auto-dump received audio frames to timestamped wav files
- [ ] Test: Send audio file, receive and dump audio frames

### Phase 5: Video Support
**Goal**: Send video files and receive video frames

- [ ] Implement backend/video_chunker.py (mp4/etc → frames)
- [ ] Extend protocol for send_video_frame
- [ ] Implement `video` command in frontend
- [ ] Auto-dump received video frames
- [ ] Test: Send video file, receive frames

### Phase 6: Advanced Features (Low Priority)
**Goal**: Polish and advanced capabilities

- [ ] Implement `stream` command (send_cmd_ex with multiple results)
- [ ] Add audio playback in shell (e.g., via ffplay)
- [ ] Configurable settings (output format, auto-dump paths)
- [ ] Better extension discovery (multiple extensions, interactive selection)
- [ ] Documentation and examples

## 7. Technical Decisions

### 7.1 Why WebSocket?
- **Bidirectional**: Full-duplex communication, perfect for async messages
- **Standard protocol**: Well-supported libraries (websockets in Python)
- **Debuggable**: Can use browser dev tools or wscat to inspect messages
- **Extensible**: Easy to add new message types

### 7.2 Why Frontend = Server, Backend = Client?
- **Simplicity**: Frontend controls lifecycle, spawns backend as needed
- **Clean shutdown**: Frontend can kill backend subprocess
- **Future-proof**: Backend can be replaced (e.g., multi-extension backend)
- **Isolation**: Extension crashes don't kill the shell

### 7.3 Why AsyncExtensionTester?
- **Native async**: Cleaner than callback-based sync API
- **Coroutine-friendly**: Integrates naturally with asyncio event loop
- **Better error handling**: Exceptions propagate cleanly

### 7.4 Why base64 for Audio/Video?
- **JSON compatible**: Can use same message format for all types
- **Simple**: No need for binary protocol or multipart messages
- **Trade-off**: ~33% size overhead acceptable for development tool

## 8. Open Questions for Implementation

1. **Shell library choice**:
   - `prompt_toolkit`: Rich features (status line, keybindings, autocomplete)
   - `cmd` + `readline`: Simpler, built-in, but limited features
   - Custom with `readline`: More control, more work
   - **Recommendation**: Start with `prompt_toolkit` for rapid development

2. **Extension discovery UI**:
   - If no manifest.json in current dir, show error or search subdirs?
   - **Recommendation**: Show clear error, suggest correct directory

3. **Property override format**:
   - Accept both file path and JSON string for `start`?
   - **Recommendation**: Auto-detect (if starts with `{` → JSON, else file path)

4. **Audio/Video chunking**:
   - Which libraries? (pydub for audio, opencv for video?)
   - Frame size/duration defaults?
   - **Recommendation**: Use ffmpeg via subprocess for reliability

5. **Message queue size limits**:
   - How many pending messages before warning/dropping?
   - **Recommendation**: 1000 messages max, show warning at 100

## 9. Success Criteria

The REPL is successful if:
1. Developer can start REPL in extension directory without configuration
2. Can start extension and send commands in <10 seconds
3. All message types work (cmd, data, audio, video)
4. Async messages are clearly visible and accessible
5. Error messages are actionable
6. No crashes on normal extension errors
7. Command history works across sessions

## 10. Future Enhancements (Beyond Initial Scope)

- **Autocomplete**: Extension command names, parameter names from manifest
- **Scripting**: Run REPL commands from file (`ten-repl --script test.repl`)
- **Recording/Playback**: Record REPL session, replay for testing
- **Multi-extension**: Test multiple extensions interacting
- **Remote debugging**: Backend on different machine
- **Property inspection**: `show properties`, `set property key=value` at runtime
- **Metrics**: Show message counts, timing, throughput
