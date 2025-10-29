# Live2D Voice Assistant

A voice assistant with **Live2D character integration** and real-time conversation capabilities powered by Agora RTC, Deepgram STT, OpenAI LLM, and ElevenLabs TTS. The example extends the standard voice assistant backend with a Live2D-ready frontend, so animated characters can react to the audio pipeline with synchronized motion.

> **Note**: This example reuses the backend configuration from [voice-assistant](../voice-assistant/) and adds a Live2D-aware frontend.

## Features

- **Live2D Character Integration**: Interactive Live2D model with audio-driven mouth movement.
- **Chained Model Voice Pipeline**: Real-time STT → LLM → TTS conversation loop.
- **Agora RTC Streaming**: Bidirectional audio streaming with Agora RTC/RTM.

## Prerequisites

### Tooling

- [`task`](https://taskfile.dev/) CLI v3 or newer (used for automation).
- `tman` CLI available on `PATH` (build it from the repo root with `task gen-tman` if you have not already).
- Go 1.21+ (required to build the API server and TEN runtime).
- Node.js 20+ with npm (frontend uses Next.js 15).
- Python 3.10+ plus [uv](https://docs.astral.sh/uv/) (default `PIP_INSTALL_CMD=uv pip install --system`). If you do not use `uv`, export `PIP_INSTALL_CMD="pip install"` before running `task install`.

### Environment Files

1. From the repository root: `cp ai_agents/.env.example ai_agents/.env`
2. From the repository root: `cp ai_agents/agents/examples/voice-assistant-live2d/frontend/env.example ai_agents/agents/examples/voice-assistant-live2d/frontend/.env.local`
3. Populate both files using the tables below.

### Required Environment Variables (`ai_agents/.env`)

| Variable | Description |
|----------|-------------|
| `AGORA_APP_ID` | Agora App ID used for RTC audio streaming. |
| `DEEPGRAM_API_KEY` | Deepgram API key for speech-to-text. |
| `OPENAI_API_KEY` | OpenAI API key for the LLM. |
| `OPENAI_MODEL` | OpenAI realtime model name (for example `gpt-4o` or `gpt-4o-mini`). |
| `ELEVENLABS_TTS_KEY` | ElevenLabs API key for text-to-speech synthesis. |

### Optional Environment Variables (`ai_agents/.env`)

| Variable | Description |
|----------|-------------|
| `AGORA_APP_CERTIFICATE` | Agora App Certificate if your project requires certificate-based auth. |
| `OPENAI_PROXY_URL` | HTTP proxy for routing OpenAI traffic. |
| `WEATHERAPI_API_KEY` | WeatherAPI key to enable the bundled weather tool. |

### Frontend Environment (`frontend/.env.local`)

| Variable | Description | Default |
|----------|-------------|---------|
| `NEXT_PUBLIC_AGORA_APP_ID` | Agora App ID exposed to the browser client. | _none_ |
| `NEXT_PUBLIC_API_BASE_URL` | Base URL for the local API server. | `http://localhost:8080` |

## Setup

### 1. Install Dependencies

```bash
cd ai_agents/agents/examples/voice-assistant-live2d
task install
```

This command installs TEN runtime packages via `tman`, builds the Go binary, installs Python dependencies (using `uv` by default), and installs the frontend dependencies.

### 2. Run the Voice Assistant

Start each process in its own terminal so they can continue running:

```bash
# Terminal 1 – TEN runtime
task run-tenapp

# Terminal 2 – API server
task run-api-server

# Terminal 3 – Frontend
task run-frontend

# Terminal 4 – (Optional) TMAN Designer UI
task run-gd-server
```

### 3. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483

## Live2D Models

The example ships with the **Kei Vowels Pro** Live2D model (see `frontend/public/models/kei_vowels_pro`). Replace or add additional models by copying their assets into `frontend/public/models/` and updating the frontend configuration.

## Configuration

The TEN runtime graph is defined in `tenapp/property.json`:

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "voice_assistant_live2d",
        "auto_start": true,
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "agora_rtc",
              "addon": "agora_rtc",
              "extension_group": "default",
              "property": {
                "app_id": "${env:AGORA_APP_ID}",
                "app_certificate": "${env:AGORA_APP_CERTIFICATE|}",
                "channel": "ten_agent_test",
                "stream_id": 1234,
                "remote_stream_id": 123,
                "subscribe_audio": true,
                "publish_audio": true,
                "publish_data": true,
                "enable_agora_asr": false
              }
            },
            {
              "type": "extension",
              "name": "stt",
              "addon": "deepgram_asr_python",
              "extension_group": "stt",
              "property": {
                "params": {
                  "api_key": "${env:DEEPGRAM_API_KEY}",
                  "language": "en-US",
                  "model": "nova-3"
                }
              }
            },
            {
              "type": "extension",
              "name": "llm",
              "addon": "openai_llm2_python",
              "extension_group": "chatgpt",
              "property": {
                "base_url": "https://api.openai.com/v1",
                "api_key": "${env:OPENAI_API_KEY}",
                "frequency_penalty": 0.9,
                "model": "${env:OPENAI_MODEL}",
                "max_tokens": 512,
                "prompt": "",
                "proxy_url": "${env:OPENAI_PROXY_URL|}",
                "greeting": "My name is Kei, nice to meet you I am your anime assistant, and what's your name?",
                "max_memory_length": 10
              }
            },
            {
              "type": "extension",
              "name": "tts",
              "addon": "elevenlabs_tts2_python",
              "extension_group": "tts",
              "property": {
                "params": {
                  "key": "${env:ELEVENLABS_TTS_KEY|}",
                  "voice_id": "lhTvHflPVOqgSWyuWQry",
                  "model_id": "eleven_multilingual_v2"
                },
                "dump": false,
                "dump_path": "./"
              }
            },
            {
              "type": "extension",
              "name": "main_control",
              "addon": "main_python",
              "extension_group": "control",
              "property": {
                "greeting": "My name is Kei, nice to meet you I am your anime assistant, and what's your name?"
              }
            },
            {
              "type": "extension",
              "name": "message_collector",
              "addon": "message_collector2",
              "extension_group": "transcriber",
              "property": {}
            },
            {
              "type": "extension",
              "name": "weatherapi_tool_python",
              "addon": "weatherapi_tool_python",
              "extension_group": "default",
              "property": {
                "api_key": "${env:WEATHERAPI_API_KEY|}"
              }
            },
            {
              "type": "extension",
              "name": "streamid_adapter",
              "addon": "streamid_adapter",
              "property": {}
            }
          ],
          "connections": [
            {
              "extension": "main_control",
              "cmd": [
                {
                  "names": [
                    "on_user_joined",
                    "on_user_left"
                  ],
                  "source": [
                    {
                      "extension": "agora_rtc"
                    }
                  ]
                },
                {
                  "names": [
                    "tool_register"
                  ],
                  "source": [
                    {
                      "extension": "weatherapi_tool_python"
                    }
                  ]
                }
              ],
              "data": [
                {
                  "name": "asr_result",
                  "source": [
                    {
                      "extension": "stt"
                    }
                  ]
                }
              ]
            },
            {
              "extension": "agora_rtc",
              "audio_frame": [
                {
                  "name": "pcm_frame",
                  "dest": [
                    {
                      "extension": "streamid_adapter"
                    }
                  ]
                },
                {
                  "name": "pcm_frame",
                  "source": [
                    {
                      "extension": "tts"
                    }
                  ]
                }
              ],
              "data": [
                {
                  "name": "data",
                  "source": [
                    {
                      "extension": "message_collector"
                    }
                  ]
                }
              ]
            },
            {
              "extension": "streamid_adapter",
              "audio_frame": [
                {
                  "name": "pcm_frame",
                  "dest": [
                    {
                      "extension": "stt"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ],
    "log": {
      "handlers": [
        {
          "matchers": [
            {
              "level": "info"
            }
          ],
          "formatter": {
            "type": "plain",
            "colored": true
          },
          "emitter": {
            "type": "console",
            "config": {
              "stream": "stdout"
            }
          }
        }
      ]
    }
  }
}
```

### Configuration Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `AGORA_APP_ID` | string | ✔︎ | Agora App ID for RTC. |
| `AGORA_APP_CERTIFICATE` | string | ✖︎ | Agora certificate if needed. |
| `DEEPGRAM_API_KEY` | string | ✔︎ | Deepgram API key. |
| `OPENAI_API_KEY` | string | ✔︎ | OpenAI API key. |
| `OPENAI_MODEL` | string | ✔︎ | OpenAI realtime model identifier. |
| `OPENAI_PROXY_URL` | string | ✖︎ | Proxy URL for OpenAI traffic. |
| `ELEVENLABS_TTS_KEY` | string | ✔︎ | ElevenLabs API key. |
| `WEATHERAPI_API_KEY` | string | ✖︎ | Enables the weather tool node. |

## Customization

Use TMAN Designer (http://localhost:49483) to modify the graph: swap STT/LLM/TTS providers, add tools, or adjust greetings. See the [TMAN Designer documentation](https://theten.ai/docs/ten_agent/customize_agent/tman-designer) for detailed instructions.

## Release as Docker Image

> Run the following commands from the `ai_agents` directory after populating `.env`.

### Build Image

```bash
cd ai_agents
docker build -f agents/examples/voice-assistant-live2d/Dockerfile -t voice-assistant-live2d .
```

### Run Container

```bash
docker run --rm -it --env-file .env -p 8080:8080 -p 3000:3000 voice-assistant-live2d
```

### Access

- Frontend: http://localhost:3000
- API Server: http://localhost:8080

## Learn More

- [Agora RTC Documentation](https://docs.agora.io/en/rtc/overview/product-overview)
- [Deepgram API Documentation](https://developers.deepgram.com/)
- [OpenAI API Documentation](https://platform.openai.com/docs)
- [ElevenLabs API Documentation](https://docs.elevenlabs.io/)
- [Live2D Documentation](https://docs.live2d.com/)
- [TEN Framework Documentation](https://doc.theten.ai)
