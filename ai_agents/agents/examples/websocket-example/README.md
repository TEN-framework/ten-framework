# WebSocket Voice Assistant

A comprehensive voice assistant with real-time conversation capabilities using WebSocket communication, Deepgram STT, OpenAI LLM, and ElevenLabs TTS.

## Features

- **WebSocket-Based Real-time Voice Interaction**: Bidirectional audio streaming over WebSocket with complete STT → LLM → TTS processing
- **Base64 Audio Encoding**: Simple JSON message format for audio transmission
- **Multi-Client Support**: Multiple WebSocket clients can connect simultaneously
- **Automatic Response Broadcasting**: Text and audio responses are automatically sent to all connected clients

## Architecture

This example demonstrates a WebSocket-based voice assistant where:

1. **Audio Input**: Clients send base64-encoded PCM audio via WebSocket → STT → LLM → TTS
2. **Audio Output**: TTS audio is sent back to clients as base64-encoded JSON messages
3. **Data Messages**: ASR results, LLM responses, and other data are automatically broadcast to all clients

```
┌─────────────────┐
│ WebSocket Client│
└────────┬────────┘
         │ {"audio": "<base64>"}
         ▼
┌─────────────────┐  pcm_frame  ┌─────┐  asr_result  ┌──────────────┐
│ websocket_server├────────────▶│ STT ├─────────────▶│ main_control │
└────────┬────────┘              └─────┘              └──────┬───────┘
         │                                                    │
         │ {"type": "audio|data"}                            ▼
         ▲                                                 ┌─────┐
         │                                                 │ LLM │
         │                                                 └──┬──┘
         │                                                    │
         │ pcm_frame                                          ▼
         │                                                 ┌─────┐
         └─────────────────────────────────────────────────┤ TTS │
                                                           └─────┘
```

## Prerequisites

### Required Environment Variables

1. **Deepgram Account**: Get credentials from [Deepgram Console](https://console.deepgram.com/)
   - `DEEPGRAM_API_KEY` - Your Deepgram API key (required)

2. **OpenAI Account**: Get credentials from [OpenAI Platform](https://platform.openai.com/)
   - `OPENAI_API_KEY` - Your OpenAI API key (required)

3. **ElevenLabs Account**: Get credentials from [ElevenLabs](https://elevenlabs.io/)
   - `ELEVENLABS_TTS_KEY` - Your ElevenLabs API key (required)

### Optional Environment Variables

- `OPENAI_MODEL` - OpenAI model name (optional, defaults to configured model)
- `OPENAI_PROXY_URL` - Proxy URL for OpenAI API (optional)
- `WEATHERAPI_API_KEY` - Weather API key for weather tool (optional)

## Setup

### 1. Set Environment Variables

Add to your `.env` file:

```bash
# Deepgram (required for speech-to-text)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (required for language model)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4
OPENAI_PROXY_URL=your_proxy_url_here

# ElevenLabs (required for text-to-speech)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here

# Optional
WEATHERAPI_API_KEY=your_weather_api_key_here
```

### 2. Install Dependencies

```bash
cd agents/examples/websocket-example
task install
```

This installs Python dependencies and frontend components.

### 3. Run the Voice Assistant

```bash
cd agents/examples/websocket-example
task run
```

The voice assistant starts with WebSocket server listening on port 8765.

### 4. Access the Application

- **WebSocket Server**: `ws://localhost:8765`
- **API Server**: http://localhost:8080
- **Frontend**: http://localhost:3000
- **TMAN Designer**: http://localhost:49483

## WebSocket Protocol

### Connecting to the WebSocket Server

```javascript
const ws = new WebSocket('ws://localhost:8765');

ws.onopen = () => {
  console.log('Connected to voice assistant');
};
```

### Sending Audio (Client → Server)

Send base64-encoded PCM audio in JSON format:

```javascript
// PCM audio format: 16kHz, mono, 16-bit
const audioBase64 = btoa(String.fromCharCode(...pcmData));

ws.send(JSON.stringify({
  audio: audioBase64,
  metadata: {
    session_id: "optional-session-id",
    // any other custom metadata
  }
}));
```

**Audio Requirements:**
- **Format**: Raw PCM (uncompressed)
- **Sample Rate**: 16000 Hz
- **Channels**: 1 (mono)
- **Bit Depth**: 16-bit (2 bytes per sample)
- **Encoding**: Base64

### Receiving Messages (Server → Client)

The server sends three types of messages:

#### 1. Audio Messages (TTS Output)

```javascript
ws.onmessage = (event) => {
  const message = JSON.parse(event.data);

  if (message.type === 'audio') {
    // Decode base64 audio
    const pcmData = atob(message.audio);

    // Get audio properties from metadata
    const sampleRate = message.metadata.sample_rate; // 16000
    const channels = message.metadata.channels; // 1
    const bytesPerSample = message.metadata.bytes_per_sample; // 2

    // Play audio using Web Audio API or other audio playback
    playAudio(pcmData, sampleRate, channels);
  }
};
```

#### 2. Data Messages (ASR Results, LLM Responses)

```javascript
if (message.type === 'data') {
  console.log('Received data:', message.name, message.data);

  // Example: ASR result
  if (message.name === 'asr_result') {
    console.log('Transcription:', message.data.text);
  }
}
```

#### 3. Error Messages

```javascript
if (message.type === 'error') {
  console.error('Server error:', message.error);
}
```

## Configuration

The voice assistant is configured in `tenapp/property.json`:

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "voice_assistant",
        "auto_start": true,
        "graph": {
          "nodes": [
            {
              "name": "websocket_server",
              "addon": "websocket_server",
              "property": {
                "port": 8765,
                "host": "0.0.0.0",
                "sample_rate": 16000,
                "channels": 1,
                "bytes_per_sample": 2
              }
            },
            {
              "name": "stt",
              "addon": "deepgram_asr_python",
              "property": {
                "params": {
                  "api_key": "${env:DEEPGRAM_API_KEY}",
                  "language": "en-US"
                }
              }
            },
            {
              "name": "llm",
              "addon": "openai_llm2_python",
              "property": {
                "api_key": "${env:OPENAI_API_KEY}",
                "model": "${env:OPENAI_MODEL}",
                "max_tokens": 512,
                "greeting": "TEN Agent connected. How can I help you today?"
              }
            },
            {
              "name": "tts",
              "addon": "elevenlabs_tts2_python",
              "property": {
                "params": {
                  "key": "${env:ELEVENLABS_TTS_KEY}",
                  "model_id": "eleven_multilingual_v2",
                  "voice_id": "pNInz6obpgDQGcFmaJgB",
                  "output_format": "pcm_16000"
                }
              }
            }
          ]
        }
      }
    ]
  }
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `websocket_server.port` | int | 8765 | WebSocket server port |
| `websocket_server.host` | string | 0.0.0.0 | WebSocket server host |
| `DEEPGRAM_API_KEY` | string | - | Deepgram API key (required) |
| `OPENAI_API_KEY` | string | - | OpenAI API key (required) |
| `OPENAI_MODEL` | string | - | OpenAI model name (optional) |
| `OPENAI_PROXY_URL` | string | - | Proxy URL for OpenAI API (optional) |
| `ELEVENLABS_TTS_KEY` | string | - | ElevenLabs API key (required) |
| `WEATHERAPI_API_KEY` | string | - | Weather API key (optional) |

## Client Example

Here's a complete example of a WebSocket client:

```javascript
class VoiceAssistantClient {
  constructor(wsUrl = 'ws://localhost:8765') {
    this.ws = new WebSocket(wsUrl);
    this.setupHandlers();
  }

  setupHandlers() {
    this.ws.onopen = () => {
      console.log('Connected to voice assistant');
    };

    this.ws.onmessage = (event) => {
      const message = JSON.parse(event.data);

      switch (message.type) {
        case 'audio':
          this.handleAudio(message);
          break;
        case 'data':
          this.handleData(message);
          break;
        case 'error':
          console.error('Error:', message.error);
          break;
      }
    };

    this.ws.onerror = (error) => {
      console.error('WebSocket error:', error);
    };

    this.ws.onclose = () => {
      console.log('Disconnected from voice assistant');
    };
  }

  sendAudio(pcmData) {
    const audioBase64 = btoa(String.fromCharCode(...new Uint8Array(pcmData)));
    this.ws.send(JSON.stringify({
      audio: audioBase64,
      metadata: {
        timestamp: Date.now()
      }
    }));
  }

  handleAudio(message) {
    // Decode and play TTS audio
    const pcmData = atob(message.audio);
    console.log('Received audio:', pcmData.length, 'bytes');
    // Implement audio playback here
  }

  handleData(message) {
    console.log('Data:', message.name, message.data);
  }

  close() {
    this.ws.close();
  }
}

// Usage
const client = new VoiceAssistantClient();

// Send audio from microphone or file
client.sendAudio(pcmAudioData);
```

## Customization

The voice assistant uses a modular design that allows you to easily replace STT, LLM, or TTS modules with other providers using TMAN Designer.

Access the visual designer at http://localhost:49483 to customize your voice agent. For detailed usage instructions, see the [TMAN Designer documentation](https://theten.ai/docs/ten_agent/customize_agent/tman-designer).

## Release as Docker image

**Note**: The following commands need to be executed outside of any Docker container.

### Build image

```bash
cd ai_agents
docker build -f agents/examples/websocket-example/Dockerfile -t websocket-voice-assistant .
```

### Run

```bash
docker run --rm -it --env-file .env -p 8080:8080 -p 3000:3000 -p 8765:8765 websocket-voice-assistant
```

### Access

- WebSocket Server: ws://localhost:8765
- Frontend: http://localhost:3000
- API Server: http://localhost:8080

## Learn More

- [Deepgram API Documentation](https://developers.deepgram.com/)
- [OpenAI API Documentation](https://platform.openai.com/docs)
- [ElevenLabs API Documentation](https://docs.elevenlabs.io/)
- [TEN Framework Documentation](https://doc.theten.ai)
- [WebSocket API (MDN)](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket)
