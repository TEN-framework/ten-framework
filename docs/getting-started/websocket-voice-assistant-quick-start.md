---
title: WebSocket Voice Assistant Quickstart
_portal_target: getting-started/websocket-voice-assistant-quick-start.md
---

## WebSocket Voice Assistant Quickstart

Build and run a real-time WebSocket voice assistant in under 10 minutes.

## Overview

This guide helps you run a complete voice assistant demo locally. It supports real-time voice conversations and tool calling. The demo includes:

| Component | Vendor     | Purpose                   |
| --------- | ---------- | ------------------------- |
| ASR       | Deepgram   | Speech-to-text            |
| LLM       | OpenAI     | Response generation       |
| TTS       | ElevenLabs | Text-to-speech            |
| Tool      | WeatherAPI | Weather lookup (optional) |

## Requirements

### Prerequisites

Follow the [TEN Framework Quick Start](./quick-start.md) to install the base environment, then make sure the check below passes:

```bash
tman check env
```

Expected output (at least Python and Go are Ready):

```text
✅ Operating System: Supported
✅ Python:   Ready
✅ Go:       Ready
```

### Extra tools

Install the following tools for build & run:

#### Task (task runner)

```bash
go install github.com/go-task/task/v3/cmd/task@latest
task --version  # verify
```

#### Bun (JavaScript package manager)

```bash
curl -fsSL https://bun.sh/install | bash
bun --version  # verify
```

#### uv (Python package manager)

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv --version  # verify
```

> 💡 After installation, reload your shell config: `source ~/.zshrc`

## Quickstart

### 1. Clone the repo

```bash
git clone https://github.com/TEN-framework/ten-framework.git
cd ten-framework/ai_agents/agents/examples/websocket-example
```

### 2. Install dependencies

```bash
task install
```

> ⏱️ Estimated time: 2–3 minutes

This command will:

- Install TEN extension packages (45+)
- Build the Go app and API server
- Install Python and frontend dependencies

### 3. Configure API keys

Configure vendor keys in `ai_agents/.env`:

```bash
cd ../../../  # back to ai_agents
vim .env
```

Add:

```bash
# Deepgram - ASR
DEEPGRAM_API_KEY=your_deepgram_api_key

# OpenAI - LLM
OPENAI_API_KEY=your_openai_api_key
OPENAI_MODEL=your_openai_model

# ElevenLabs - TTS
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key

# WeatherAPI - Tool (optional)
WEATHERAPI_API_KEY=your_weatherapi_key
```

### 4. Start services

```bash
cd agents/examples/websocket-example
task run
```

After startup, the following services are available:

- **Frontend UI**: <http://localhost:3000>
- **API server**: <http://localhost:8080>
- **TMAN Designer**: <http://localhost:49483>

### 5) Talk to your assistant

Open [http://localhost:3000](http://localhost:3000), click the microphone button, and start speaking.

Try:

- "Hello, who are you?"
- "What's the weather in Beijing today?" (requires WeatherAPI)
- "Tell me a joke."

**Congratulations!** Your TEN Agent voice assistant is running.

## Advanced

### Swap extensions

TEN Framework lets you swap ASR/LLM/TTS extensions flexibly.

#### Option 1: Use TMAN Designer (recommended)

Open [http://localhost:49483](http://localhost:49483), then in the visual editor:

1. Click an extension node to inspect it
2. Choose a replacement extension
3. Configure parameters
4. Save and restart

#### Option 2: Edit configuration file

Edit `tenapp/property.json` and modify the extension configs under `predefined_graphs`.

##### Example: switch ASR to Azure

```json
{
  "type": "extension",
  "name": "stt",
  "addon": "azure_asr_python",
  "extension_group": "stt",
  "property": {
    "params": {
      "key": "${env:AZURE_STT_KEY}",
      "region": "${env:AZURE_STT_REGION}",
      "language": "en-US"
    }
  }
}
```

##### Example: switch TTS to Minimax

```json
{
  "type": "extension",
  "name": "tts",
  "addon": "minimax_tts_websocket_python",
  "extension_group": "tts",
  "property": {
    "params": {
      "api_key": "${env:MINIMAX_TTS_API_KEY|}",
      "group_id": "${env:MINIMAX_TTS_GROUP_ID|}",
      "model": "speech-02-turbo",
      "audio_setting": {
        "sample_rate": 16000
      },
      "voice_setting": {
        "voice_id": "female-shaonv"
      }
    }
  }
}
```

Restart after changes (`Ctrl+C` then `task run`).

### Monitoring (Grafana)

Use Grafana to observe application metrics and logs.

#### 1. Start monitoring stack

```bash
cd tools/grafana-monitoring
docker-compose -f docker-compose.push.yml up -d
```

Services:

- **Grafana** (port 3001) - UI
- **Prometheus** (port 9091) - metrics store
- **Loki** (port 3100) - log store
- **OpenTelemetry Collector** (ports 4317/4318) - OTLP receiver

#### 2. Configure app telemetry

Edit `agents/examples/websocket-example/tenapp/property.json`:

#### Logs (export)

Add to the `ten.log.handlers` array:

```json
{
  "matchers": [{ "level": "info" }],
  "formatter": { "type": "json", "colored": false },
  "emitter": {
    "type": "otlp",
    "config": {
      "endpoint": "http://localhost:4317",
      "service_name": "websocket-example"
    }
  }
}
```

#### Metrics (export)

Add to `ten.services`:

```json
{
  "telemetry": {
    "enabled": true,
    "metrics": {
      "enabled": true,
      "exporter": {
        "type": "otlp",
        "config": {
          "endpoint": "http://localhost:4317",
          "service_name": "websocket-example"
        }
      }
    }
  }
}
```

#### 3. Restart and open Grafana

```bash
cd ../../../agents/examples/websocket-example
task run
```

Open [http://localhost:3001](http://localhost:3001) (username/password: `admin/admin`).

#### Metrics (Dashboards)

Go to `Dashboards` → **"TEN Framework"** to view:

- Extension startup latency
- Cmd processing latency
- Message queue wait time

#### Logs (Explore/Loki)

Go to `Explore` → select `Loki`, then query:

- `{service_name="websocket-example"}` - all logs
- `{service_name="websocket-example", ten_extension_name="stt"}` - ASR logs
- `{service_name="websocket-example", ten_extension_name="llm"}` - LLM logs
- `{service_name="websocket-example", ten_extension_name="tts"}` - TTS logs

> 💡 Use the `ten_extension_name` label to filter logs per extension.

Stop the monitoring stack:

```bash
cd tools/grafana-monitoring
docker-compose -f docker-compose.push.yml down
```

## Troubleshooting

<details>
<summary><strong>Frontend dependency install fails</strong></summary>

If `bun install` fails due to missing versions, switch to the official npm registry:

```bash
cd frontend
echo "registry=https://registry.npmjs.org/" > .npmrc
rm bun.lock
bun install
```

</details>

<details>
<summary><strong>macOS Python library loading fails</strong></summary>

Set the Python dynamic library path:

```bash
export DYLD_LIBRARY_PATH=/usr/local/opt/python@3.10/Frameworks/Python.framework/Versions/3.10/lib:$DYLD_LIBRARY_PATH
# Add to ~/.zshrc for persistence
```

</details>

<details>
<summary><strong>Port already in use</strong></summary>

Find and kill the process:

```bash
lsof -i :3000  # or :8080
kill -9 <PID>
```

</details>

## Next steps

- **[Extension development guide](https://theten.ai/cn/docs/ten_framework/development/how_to_develop_with_ext)** - build custom extensions
- **[More examples](https://github.com/TEN-framework/ten-framework/tree/main/ai_agents/agents/examples)** - explore other agent demos
- **[GitHub Issues](https://github.com/TEN-framework/ten-framework/issues)** - report bugs or request features
- **[Documentation hub](https://theten.ai/cn/docs)** - full TEN Framework documentation
