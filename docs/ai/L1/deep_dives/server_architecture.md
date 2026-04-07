# Server Architecture

> **When to Read This:** Load this document when you need to understand how the Go API
> server works, how property injection transforms graph configurations at runtime, or
> how worker processes are managed.

## Overview

The TEN Agent server is a Go HTTP server built with the Gin framework. It manages
agent session lifecycles — starting worker processes, injecting configuration, and
handling session keepalive/teardown.

## Server Structure

```
server/
├── main.go                 # Entry point, parses flags, starts HTTP server
└── internal/
    ├── http_server.go      # All endpoint handlers + property injection
    └── config.go           # startPropMap configuration for parameter injection
```

Key launch flag: `-tenapp_dir=<path>` — points to the example's `tenapp/` directory
containing `property.json` and `manifest.json`.

## Endpoint Handlers

| Handler                          | Route              | Purpose                             |
| -------------------------------- | ------------------ | ----------------------------------- |
| `handlerHealth()`                | `GET /health`      | Returns `{"code":"0"}` if running   |
| `handleGraphs()`                 | `GET /graphs`      | Reads predefined_graphs from property.json |
| `handlerStart()`                 | `POST /start`      | Spawns worker process for a session |
| `handlerStop()`                  | `POST /stop`       | Terminates worker process           |
| `handlerPing()`                  | `POST /ping`       | Resets session timeout timer        |
| `handlerList()`                  | `GET /list`        | Lists active workers/channels       |
| `handlerGenerateToken()`         | `POST /token/generate` | Generates Agora RTC tokens     |
| `handleAddonDefaultProperties()` | `GET /addon/default-properties` | Extension property.json files |
| `handlerVectorDocumentUpdate()`  | `POST /vector/document/update` | Vector DB updates          |
| `handlerVectorDocumentUpload()`  | `POST /vector/document/upload` | File uploads for vector DB |

## Property Injection Pipeline

When `/start` is called, the server transforms the static `property.json` into a
session-specific configuration. This is the core of the `processProperty` function:

### Step 1: Read Base Configuration

```go
// Read property.json from the configured tenapp_dir
propertyJsonFile := filepath.Join(s.config.TenappDir, "property.json")
content, _ := os.ReadFile(propertyJsonFile)
```

### Step 2: Filter Graphs

Only the requested graph is kept; its `auto_start` is set to `true`:

```go
// Find matching graph by name
for _, graph := range predefinedGraphs {
    if graph.Name == req.GraphName {
        graph.AutoStart = true
        filteredGraphs = append(filteredGraphs, graph)
    }
}
```

### Step 3: Merge Dynamic Properties

Per-extension property overrides from the request are merged:

```go
// req.Properties = {"openai_llm2_python": {"model": "gpt-4o-mini"}}
for _, node := range graph.Nodes {
    if props, ok := req.Properties[node.Name]; ok {
        mergeProperties(node.Property, props)
    }
}
```

### Step 4: Inject Start Parameters

The `startPropMap` (defined in `config.go`) maps request fields to node properties:

```go
var startPropMap = map[string]string{
    "RemoteStreamId":      "remote_stream_id",
    "BotStreamId":         "agora_uid",
    "Token":               "token",
    "WorkerHttpServerPort": "server_port",
}
```

These values are injected into every node that has the corresponding property defined.

### Step 5: Channel Auto-Injection

Any node with a `"channel"` property automatically receives the request's `channel_name`:

```go
// Scan all nodes — if node has "channel" property, inject channel_name
for _, node := range graph.Nodes {
    if _, hasChannel := node.Property["channel"]; hasChannel {
        node.Property["channel"] = req.ChannelName
    }
}
```

This is future-proof: adding a new extension with a `"channel"` property requires
zero server code changes.

### Step 6: Environment Variable Resolution

All `${env:VAR}` and `${env:VAR|default}` references in the property JSON are
resolved against the container's environment.

### Step 7: Write Temp File and Spawn Worker

The modified property JSON is written to a temporary file, and a worker process
is spawned:

```go
// Write modified config
tmpFile := filepath.Join(tmpDir, "property.json")
os.WriteFile(tmpFile, modifiedJSON, 0644)

// Spawn worker
cmd := exec.Command("tman", "run", "start", "--property", tmpFile)
```

## Worker Process Lifecycle

```
/start request
    │
    ▼
Server: processProperty() → temp property.json
    │
    ▼
Server: exec("tman run start --property <tmp>")
    │
    ▼
Worker process starts → loads graph → initializes extensions
    │
    ├── Extensions call on_init() → on_start()
    ├── Extensions process messages (cmd, data, audio_frame, video_frame)
    │
    ├── /ping requests reset the timeout timer
    │
    ▼
/stop request OR timeout
    │
    ▼
Worker: extensions call on_stop() → on_deinit()
    │
    ▼
Worker process terminates
```

**Important**: Worker processes run on the **host machine**, not inside Docker.
They can outlive the server process and even container restarts. Always check for
zombie workers with `ps -elf | grep 'bin/main'`.

## Session Management

| Action         | Server Behavior                                    |
| -------------- | -------------------------------------------------- |
| `/start`       | Spawns worker, stores in active workers map        |
| `/stop`        | Sends SIGTERM to worker, removes from map          |
| `/ping`        | Resets timeout timer for the channel               |
| Timeout        | Auto-sends SIGTERM after `timeout` seconds idle    |
| `/list`        | Returns all active channel → worker mappings       |

Timeout of `-1` means the session never auto-stops (requires explicit `/stop`).

## LOG_STDOUT for Worker Output

Worker processes write to stdout. To see their output in `/tmp/task_run.log`,
the `.env` must have:

```bash
LOG_STDOUT=true
```

Without this, extension logs (Python `print()`, `ten_env.log_*()`) are invisible.

## Security Measures

- **Path traversal prevention**: The server ignores any client-provided `tenapp_dir`
  and always uses the launch-configured path
- **Channel name sanitization**: Channel names are validated before use in file paths
- **Safe property merge**: `mergeProperties()` handles nested configs safely with
  type checking

## Configuration (config.go)

The `startPropMap` in `config.go` controls which request fields map to which
node properties:

| Request Field          | Node Property        | Purpose                        |
| ---------------------- | -------------------- | ------------------------------ |
| `RemoteStreamId`       | `remote_stream_id`   | Remote user's stream ID        |
| `BotStreamId`          | `agora_uid`          | Bot's Agora UID                |
| `Token`                | `token`              | Agora RTC token                |
| `WorkerHttpServerPort` | `server_port`        | Worker's HTTP server port      |

## See Also

- [Back to Architecture](../02_architecture.md)
- [Graph Configuration](graph_configuration.md) — Property.json structure and connections
- [Back to Interfaces](../06_interfaces.md)
