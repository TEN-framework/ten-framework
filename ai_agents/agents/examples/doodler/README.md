# Voice Image Kids - AI Art Generator for Children

A delightful voice-to-image generation application built with TEN Framework. Kids speak what they want to draw, and AI creates it instantly!

## Features

- **Voice Activity Detection**: Automatic speech detection - no buttons needed!
- **Natural Speech Input**: Kids just talk naturally about what they want to create
- **GPT Image 1.5**: Latest, fastest OpenAI image generation (4x faster than DALL-E 3)
- **Kid-Friendly UI**: Colorful, engaging interface via dedicated doodler frontend
- **Instant Results**: Images appear in seconds
- **Safe & Encouraging**: Gentle error messages and positive feedback

## How It Works

1. **Kid speaks**: "I want a purple dragon flying over a rainbow castle!"
2. **AI listens**: OpenAI Whisper transcribes speech
3. **AI understands**: GPT-4o-mini processes the request
4. **AI creates**: GPT Image 1.5 generates the image
5. **Kid sees**: Image appears in the chat!

## Prerequisites

### Required API Keys

1. **Agora Account** (for voice input)
   - Sign up at [console.agora.io](https://console.agora.io/)
   - Get your `AGORA_APP_ID`

2. **OpenAI Account** (for everything else)
   - Sign up at [platform.openai.com](https://platform.openai.com/)
   - Get your `OPENAI_API_KEY`
   - You'll need access to:
     - Whisper (speech-to-text)
     - GPT-4o-mini (language model)
     - GPT Image 1.5 (image generation)

### System Requirements

- **Node.js**: >= 20
- **Bun**: Latest version
- **Go**: For API server
- **Python**: 3.10+ (for TEN extensions)
- **TEN Framework**: Installed via `tman`

## Quick Start

### 1. Clone and Navigate

```bash
cd ai_agents/agents/examples/doodler
```

### 2. Set Environment Variables

Create a `.env` file in the root `ai_agents` directory:

```bash
# Required
AGORA_APP_ID=your_agora_app_id
OPENAI_API_KEY=sk-your_openai_key

# Optional
AGORA_APP_CERTIFICATE=your_certificate
OPENAI_MODEL=gpt-4o-mini
AGENT_SERVER_URL=http://localhost:8080
TEN_DEV_SERVER_URL=http://localhost:49483
NEXT_PUBLIC_EDIT_GRAPH_MODE=true
```

### 3. Install Dependencies

```bash
task install
```

This will:
- Install TEN framework packages
- Install Python dependencies
- Install frontend
- Build the API server

### 4. Run the App

```bash
task run
```

This starts:
- TEN Runtime (agent backend)
- API Server (port 8080)
- TMAN Designer (port 49483)
- Frontend will be available from the doodler frontend

Frontend source lives in `ai_agents/agents/examples/doodler/frontend`.

### 5. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483

## API Endpoints

- `GET /graphs` lists available graphs in `tenapp/property.json` for the API server:
  ```
  curl http://localhost:8080/graphs
  ```
- `POST /start` launches a worker with the selected graph and optional property overrides:
  ```
  curl -X POST http://localhost:8080/start \
    -H 'Content-Type: application/json' \
    -d '{
      "request_id":"any-id",
      "channel_name":"kids_demo",
      "user_uid":10001,
      "graph_name":"voice_image_kids",
      "properties":{}
    }'
  ```

## Usage

1. Open http://localhost:3000 in your browser
2. Allow microphone access when prompted
3. Start speaking! For example:
   - "I want a spaceship in outer space!"
   - "Draw a cute puppy playing in a park"
   - "Create a magical fairy castle with rainbows"
4. Watch as the AI creates your image!

## Configuration

### Agent Graph

The app uses these components:

- **agora_rtc**: Audio I/O with voice activity detection
- **openai_asr_python**: Speech-to-text (Whisper)
- **openai_llm2_python**: Language model (GPT-4o-mini)
- **openai_gpt_image_python**: Image generation (GPT Image 1.5)
- **main_python**: Orchestration
- **message_collector**: Chat history

### Customization

Edit `tenapp/property.json` to customize:

**LLM Prompt** (make it more/less kid-friendly):
```json
{
  "nodes": [{
    "name": "llm",
    "property": {
      "prompt": "Your custom system prompt here..."
    }
  }]
}
```

**Image Settings**:
```json
{
  "nodes": [{
    "name": "image_gen_tool",
    "property": {
      "params": {
        "model": "gpt-image-1.5",  // or "dall-e-3"
        "size": "1024x1024",       // or "1792x1024", "1024x1792"
        "quality": "standard"      // or "hd" for higher quality
      }
    }
  }]
}
```

## Project Structure

```
doodler/
├── tenapp/
│   ├── property.json              # Agent graph configuration
│   ├── manifest.json               # App metadata
│   └── ten_packages/
│       └── extension/
│           └── main_python/        # Main control logic
│               ├── extension.py    # Event handlers
│               ├── config.py       # Configuration
│               └── agent/          # Agent framework
├── Taskfile.yml                    # Build & run automation
├── Dockerfile                       # Container deployment
├── .env.example                    # Environment template
└── README.md                       # This file
```

## Troubleshooting

### No Voice Input Detected

- Check microphone permissions in browser
- Verify `AGORA_APP_ID` is set correctly
- Check browser console for errors

### Images Not Generating

- Verify `OPENAI_API_KEY` has access to GPT Image 1.5
- Check TEN runtime logs: `tail -f tenapp/logs/latest.log`
- Try fallback model (DALL-E 3) in configuration

### "API key is invalid"

- Double-check `OPENAI_API_KEY` in `.env`
- Ensure no extra spaces or quotes
- Verify key is active on OpenAI platform

### Installation Fails

```bash
# Clean install
cd tenapp
rm -rf ten_packages
tman install
./scripts/install_python_deps.sh
```

## Docker Deployment

### Build Image

```bash
cd ai_agents
docker build -f agents/examples/doodler/Dockerfile -t doodler .
```

### Run Container

```bash
docker run --rm -it --env-file .env \
  -p 8080:8080 \
  -p 3000:3000 \
  -p 49483:49483 \
  doodler
```

### Access

- Frontend: http://localhost:3000
- API: http://localhost:8080
- TMAN Designer: http://localhost:49483

## Development

### Visual Graph Designer

Access TMAN Designer at http://localhost:49483 to:
- Visualize the agent graph
- Modify connections visually
- Test different configurations
- Add new extensions

### Adding New Features

1. **Add a new extension** to `tenapp/property.json`
2. **Configure connections** in the graph
3. **Update main_python** to handle new events
4. **Test** with `task run`

### Debugging

Enable debug mode in `tenapp/property.json`:

```json
{
  "nodes": [{
    "name": "image_gen_tool",
    "property": {
      "dump": true,
      "dump_path": "./debug_images.json"
    }
  }]
}
```

View logs:
```bash
# TEN runtime logs
tail -f tenapp/logs/latest.log

# API server logs
# (shown in terminal where you ran `task run`)
```

## Safety & Content Policy

The app uses OpenAI's content policy filtering. If an image request violates policies, kids will see:
> "I can't create that image. Let's try something different!"

The LLM is configured to be encouraging and kid-friendly.

## Performance

- **Voice-to-Text**: ~500ms (Whisper)
- **LLM Processing**: ~1-2s (GPT-4o-mini)
- **Image Generation**: ~3-5s (GPT Image 1.5) - 4x faster than DALL-E 3!
- **Total**: ~5-8 seconds from speech to image

## Cost Estimation

Per image generation:
- Whisper transcription: ~$0.006/minute
- GPT-4o-mini: ~$0.001 (for prompt processing)
- GPT Image 1.5 (1024x1024, standard): ~$0.04
- **Total per image**: ~$0.05

(Prices as of December 2024, may vary)

## Learn More

- [TEN Framework Documentation](https://doc.theten.ai)
- [OpenAI Image Generation Guide](https://platform.openai.com/docs/guides/image-generation)
- [GPT Image 1.5 Announcement](https://openai.com/index/new-chatgpt-images-is-here/)
- [Agora RTC Documentation](https://docs.agora.io/en/)

## License

This example is part of the TEN Framework, licensed under the Apache License, Version 2.0.

## Support

- **TEN Framework Issues**: [github.com/TEN-framework/TEN-Agent](https://github.com/TEN-framework/TEN-Agent)
- **OpenAI Support**: [help.openai.com](https://help.openai.com/)
- **Agora Support**: [agora.io/support](https://www.agora.io/en/customer-support/)

---

**Have fun creating amazing art with AI!** 🎨✨
