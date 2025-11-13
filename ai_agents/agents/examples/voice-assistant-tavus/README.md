# Voice Assistant with Tavus

A real-time voice assistant with Tavus Conversational Video Interface, featuring AI-powered video avatars with natural conversation capabilities.

## Features

- **Tavus Video Avatars**: Hyper-realistic digital human avatars with video conversation
- **Full Pipeline Integration**: Customizable STT, LLM, TTS, and Perception layers
- **Auto Persona Creation**: Automatically creates Tavus personas with your custom configuration
- **Smart Turn Detection**: Natural conversation flow with automatic speech detection
- **Real-time Streaming**: Low-latency audio/video streaming via Agora RTC
- **WebSocket Event Stream**: Broadcasts persona and conversation URLs to browser clients

## Prerequisites

### Required Environment Variables

1. **Agora Account**: Get credentials from [Agora Console](https://console.agora.io/)
   - `AGORA_APP_ID` - Your Agora App ID (required)
   - `AGORA_APP_CERTIFICATE` - Your Agora App Certificate (optional)

2. **Tavus Account**: Get credentials from [Tavus Platform](https://platform.tavus.io/)
   - `TAVUS_API_KEY` - Your Tavus API key (required)
   - `TAVUS_REPLICA_ID` - Your Tavus replica/avatar ID (required)

3. **LLM Provider**: Get API key from your LLM provider
   - `TAVUS_LLM_PROVIDER` - LLM provider name (e.g., "openai", "anthropic")
   - `TAVUS_LLM_MODEL` - Model name (e.g., "gpt-4", "claude-3-opus-20240229")
   - `TAVUS_LLM_API_KEY` - Your LLM API key

### Optional Environment Variables

- `TAVUS_LLM_BASE_URL` - Custom LLM endpoint URL (optional)
- `TAVUS_TTS_PROVIDER` - TTS provider name (optional, uses Tavus default if not set)
- `TAVUS_TTS_VOICE_ID` - TTS voice ID (optional)

## Setup

### 1. Set Environment Variables

Add to your `.env` file in the root of `ai_agents`:

```bash
# Agora (required for audio/video streaming)
AGORA_APP_ID=your_agora_app_id_here
AGORA_APP_CERTIFICATE=your_agora_certificate_here

# Tavus (required for video avatars)
TAVUS_API_KEY=tvs-xxx-your-tavus-api-key
TAVUS_REPLICA_ID=r7xxx-your-replica-id

# LLM Configuration (required for conversation)
TAVUS_LLM_PROVIDER=openai
TAVUS_LLM_MODEL=gpt-4
TAVUS_LLM_API_KEY=sk-xxx-your-openai-key

# Optional: Custom LLM endpoint
# TAVUS_LLM_BASE_URL=https://api.openai.com/v1

# Optional: Custom TTS
# TAVUS_TTS_PROVIDER=cartesia
# TAVUS_TTS_VOICE_ID=your-voice-id
```

### 2. Install Dependencies

```bash
cd agents/examples/voice-assistant-tavus
task install
```

This installs:
- TEN runtime dependencies
- Python dependencies (Tavus extension)
- Frontend components (shared playground)
- API server

### 3. Run the Voice Assistant

```bash
cd agents/examples/voice-assistant-tavus
task run
```

The voice assistant starts with Tavus video integration enabled.

### 4. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483
- **WebSocket Events**: ws://localhost:8765

## How It Works

### Architecture

```
User Browser → Agora RTC ↔ TEN Agent ↔ Tavus Extension
                                           ↓
                                  1. Create Persona (startup)
                                  2. Create Conversation (user join)
                                  3. Return conversation URL
                                           ↓
                              User joins Tavus video conversation
```

### Flow

1. **Agent Startup**:
   - Tavus extension automatically creates a persona with your configured LLM, TTS, and settings
   - Persona ID is logged and saved for conversation creation

2. **User Joins**:
   - When a user joins the Agora channel, Tavus extension creates a conversation
   - Conversation URL is sent to the frontend via data message
   - Frontend displays the conversation URL or embeds it

3. **Video Conversation**:
   - User clicks the URL to join the Tavus video conversation
   - Speaks to the AI avatar through their microphone
   - Avatar responds with natural voice and lip-sync

4. **User Leaves**:
   - Conversation is automatically terminated
   - Resources are cleaned up

## Configuration

The voice assistant is configured in `tenapp/property.json`:

### Tavus Node Configuration

```json
{
  "type": "extension",
  "name": "tavus_control",
  "addon": "tavus_python",
  "property": {
    "tavus_api_key": "${env:TAVUS_API_KEY}",
    "replica_id": "${env:TAVUS_REPLICA_ID}",
    "conversation_name": "TEN Voice Assistant with Tavus",
    "auto_create_persona": true,
    "persona_name": "TEN Voice Assistant",
    "system_prompt": "You are a helpful AI voice assistant...",
    "enable_smart_turn_detection": true,
    "llm_provider": "${env:TAVUS_LLM_PROVIDER}",
    "llm_model": "${env:TAVUS_LLM_MODEL}",
    "llm_api_key": "${env:TAVUS_LLM_API_KEY}"
  }
}
```

### Customization Options

#### Change AI Personality

Edit the `system_prompt` in `tenapp/property.json`:

```json
{
  "system_prompt": "You are a friendly customer service agent who helps users with technical questions. Be patient, helpful, and concise.",
  "context": "You are in a video call with a customer who needs assistance."
}
```

#### Enable Perception (Screen Sharing)

```json
{
  "enable_perception": true,
  "perception_model": "raven-0"
}
```

#### Use Different LLM

```json
{
  "llm_provider": "anthropic",
  "llm_model": "claude-3-opus-20240229",
  "llm_api_key": "${env:ANTHROPIC_API_KEY}"
}
```

#### Enable Recording

```json
{
  "enable_recording": true
}
```

Recordings will be available in your Tavus dashboard.

#### Change Language

```json
{
  "language": "es"
}
```

Supported: `en`, `es`, `fr`, `de`, `pt`, `zh`, `ja`, `ko`, `multilingual`

## Frontend Integration

### WebSocket Notifications (Recommended)

This example now exposes the Tavus lifecycle events over the shared `websocket_server` extension
(`ws://localhost:8765` by default). Any client can subscribe and react when personas or conversations
are ready—no Agora data channel required.

```javascript
const ws = new WebSocket("ws://localhost:8765");

ws.onmessage = (event) => {
  const message = JSON.parse(event.data);

  if (message?.data?.data_type === "tavus_event") {
    const payload = message.data.payload;

    if (message.data.event === "conversation_created") {
      console.log("Tavus conversation URL:", payload.conversation_url);
      window.open(payload.conversation_url, "_blank");
    }
  }
};
```

Event payloads follow this shape:

| Event | Description | Payload |
| --- | --- | --- |
| `persona_created` | Auto-created persona is ready | `persona_id`, `replica_id`, `auto_created` |
| `conversation_created` | Conversation started | `conversation_id`, `conversation_url`, `persona_id`, `replica_id` |
| `conversation_ended` | Conversation stopped | `conversation_id`, `conversation_url`, `persona_id`, `replica_id` |

### Receiving Conversation URL via Agora (Legacy Flow)

The Tavus extension still sends a `tavus_conversation_created` data message with the conversation URL.

Example frontend code to handle it:

```javascript
// Listen for Tavus conversation URL
agoraRtcEngine.on('streamMessage', (uid, data) => {
  try {
    const message = JSON.parse(data);

    if (message.type === 'tavus_conversation_created') {
      const conversationUrl = message.conversation_url;
      console.log('Tavus conversation URL:', conversationUrl);

      // Option 1: Open in new window
      window.open(conversationUrl, '_blank');

      // Option 2: Embed in iframe
      document.getElementById('tavus-iframe').src = conversationUrl;
    }
  } catch (e) {
    console.error('Error parsing message:', e);
  }
});
```

### Embedding Tavus Conversation

```html
<!-- Option 1: Simple iframe -->
<iframe
  id="tavus-iframe"
  src=""
  width="800"
  height="600"
  allow="camera; microphone; fullscreen"
></iframe>

<!-- Option 2: Using Tavus SDK -->
<script src="https://cdn.tavus.io/sdk/v1/tavus-sdk.js"></script>
<script>
  const tavus = new TavusSDK({
    apiKey: 'your_api_key',
    onReady: () => console.log('Tavus ready'),
    onError: (error) => console.error('Tavus error:', error)
  });

  // Join when URL is received
  tavus.joinConversation(conversationUrl);
</script>
```

## Advanced Configuration

### Full Pipeline Layers

The Tavus Full Pipeline supports four customizable layers:

1. **Perception Layer** (Optional)
   - Enable screen sharing and visual understanding
   - Model: `raven-0`

2. **STT Layer** (Speech-to-Text)
   - Smart turn detection for natural conversation flow
   - Automatically detects when user finishes speaking

3. **LLM Layer** (Language Model)
   - Configurable providers: OpenAI, Anthropic, custom endpoints
   - Custom prompts and context

4. **TTS Layer** (Text-to-Speech)
   - Natural voice synthesis
   - Configurable providers and voices

### Using Existing Persona

If you already have a Tavus persona, you can use it instead of auto-creating:

```json
{
  "tavus_api_key": "${env:TAVUS_API_KEY}",
  "persona_id": "per_xxxxxxxxxxxxx",
  "replica_id": "${env:TAVUS_REPLICA_ID}",
  "auto_create_persona": false
}
```

## Troubleshooting

### "Failed to create Tavus persona: 401"
- Check your `TAVUS_API_KEY` is correct and active
- Verify the API key has permissions in Tavus dashboard

### "No persona_id available"
- Ensure `auto_create_persona: true` is set in property.json
- Verify LLM configuration is correct and API key is valid

### "Failed to create conversation: 400"
- Check `TAVUS_REPLICA_ID` is valid
- Verify the replica exists in your Tavus account
- Ensure persona was created successfully (check logs)

### Conversation URL not appearing
- Check agent logs for errors
- Verify Agora RTC connection is working
- Ensure data channel is enabled in Agora configuration
- Check frontend is listening for data messages

### Agent fails to start
- Run `task install` to ensure all dependencies are installed
- Check all required environment variables are set
- Verify `tman` is installed: `tman --version`

## Development

### View Logs

```bash
# View agent logs
tail -f tenapp/logs/app.log

# View with filtering
tail -f tenapp/logs/app.log | grep Tavus
```

### Debug Mode

Enable Python debug mode in `tenapp/scripts/start.sh`:

```bash
export TEN_ENABLE_PYTHON_DEBUG=true
export TEN_PYTHON_DEBUG_PORT=5678
```

Then attach your Python debugger to port 5678.

### TMAN Designer

Access the visual designer at http://localhost:49483 to:
- View and edit the graph configuration
- Monitor extension connections
- Debug message flow

## Resources

- [Tavus Documentation](https://docs.tavus.io)
- [Tavus API Reference](https://docs.tavus.io/api-reference)
- [TEN Framework Documentation](https://github.com/TEN-framework/ten-framework)
- [Agora RTC Documentation](https://docs.agora.io)

## Support

For issues related to:
- **Tavus**: Contact Tavus support at support@tavus.io
- **TEN Framework**: Open an issue on [GitHub](https://github.com/TEN-framework/ten-framework)
- **This Agent**: Check the logs and troubleshooting section above

## License

This agent follows the TEN framework's licensing terms.
