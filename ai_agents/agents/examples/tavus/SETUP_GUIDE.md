# Tavus Full Pipeline Setup Guide

This guide walks you through setting up the Tavus Conversational Video Interface with the full pipeline in TEN Framework.

## Prerequisites

1. **Tavus Account**: Sign up at [platform.tavus.io](https://platform.tavus.io)
2. **Tavus API Key**: Generate from [platform.tavus.io/api-keys](https://platform.tavus.io/api-keys)
3. **Tavus Replica**: Create or select a digital avatar replica from the Tavus dashboard
4. **Agora Account**: Sign up at [console.agora.io](https://console.agora.io) for RTC capabilities
5. **LLM API Key**: OpenAI, Anthropic, or other LLM provider (if using auto-create persona)

## Setup Options

### Option A: Auto-Create Persona (Recommended for Development)

This option automatically creates a Tavus persona with your custom configuration on agent startup.

#### Step 1: Set Environment Variables

Create a `.env` file in the agent directory:

```bash
# Tavus Configuration
TAVUS_API_KEY="your_tavus_api_key_here"
TAVUS_REPLICA_ID="your_replica_id_here"

# LLM Configuration (for persona creation)
TAVUS_LLM_PROVIDER="openai"
TAVUS_LLM_MODEL="gpt-4"
TAVUS_LLM_API_KEY="your_openai_api_key_here"

# Optional: Custom LLM endpoint
# TAVUS_LLM_BASE_URL="https://api.openai.com/v1"

# Optional: TTS Configuration
# TAVUS_TTS_PROVIDER="cartesia"
# TAVUS_TTS_VOICE_ID="your_voice_id"

# Agora RTC Configuration
AGORA_APP_ID="your_agora_app_id"
AGORA_APP_CERTIFICATE="your_agora_app_certificate"
```

#### Step 2: Configure Agent

Use the full pipeline example configuration:

```bash
cp property.full_pipeline.example.json property.json
```

Or manually update your `property.json` with the tavus_control node:

```json
{
  "type": "extension",
  "name": "tavus_control",
  "addon": "tavus_python",
  "extension_group": "control",
  "property": {
    "tavus_api_key": "${env:TAVUS_API_KEY}",
    "replica_id": "${env:TAVUS_REPLICA_ID}",
    "conversation_name": "TEN Agent Conversation",
    "max_call_duration": 3600,
    "enable_recording": false,
    "language": "en",
    "auto_create_persona": true,
    "persona_name": "TEN Framework Assistant",
    "system_prompt": "You are a helpful AI assistant integrated with the TEN framework. Help users with conversational AI tasks.",
    "context": "You are running in a real-time conversational environment.",
    "enable_perception": false,
    "enable_smart_turn_detection": true,
    "llm_provider": "${env:TAVUS_LLM_PROVIDER}",
    "llm_model": "${env:TAVUS_LLM_MODEL}",
    "llm_api_key": "${env:TAVUS_LLM_API_KEY}"
  }
}
```

#### Step 3: Start the Agent

```bash
cd ai_agents/agents/examples/tavus
./bin/start
```

The agent will automatically:
1. Load configuration from environment variables
2. Create a Tavus persona with your specified settings
3. Log the created persona ID
4. Wait for users to join

### Option B: Use Existing Persona

If you already have a persona created in Tavus, use this simpler configuration.

#### Step 1: Create Persona via Tavus API

```bash
curl -X POST https://tavusapi.com/v2/personas \
  -H "x-api-key: YOUR_TAVUS_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "persona_name": "My TEN Assistant",
    "system_prompt": "You are a helpful AI assistant.",
    "default_replica_id": "YOUR_REPLICA_ID",
    "pipeline_mode": "full",
    "layers": {
      "stt": {
        "smart_endpointing": true
      },
      "llm": {
        "provider": "openai",
        "model": "gpt-4",
        "api_key": "YOUR_OPENAI_API_KEY"
      }
    }
  }'
```

Save the returned `persona_id`.

#### Step 2: Set Environment Variables

```bash
# Tavus Configuration
TAVUS_API_KEY="your_tavus_api_key_here"
TAVUS_PERSONA_ID="persona_id_from_step_1"
TAVUS_REPLICA_ID="your_replica_id_here"

# Agora RTC Configuration
AGORA_APP_ID="your_agora_app_id"
AGORA_APP_CERTIFICATE="your_agora_app_certificate"
```

#### Step 3: Configure Agent

Update your `property.json`:

```json
{
  "type": "extension",
  "name": "tavus_control",
  "addon": "tavus_python",
  "extension_group": "control",
  "property": {
    "tavus_api_key": "${env:TAVUS_API_KEY}",
    "persona_id": "${env:TAVUS_PERSONA_ID}",
    "replica_id": "${env:TAVUS_REPLICA_ID}",
    "conversation_name": "TEN Agent Conversation"
  }
}
```

#### Step 4: Start the Agent

```bash
cd ai_agents/agents/examples/tavus
./bin/start
```

## Full Pipeline Layer Configuration

### Perception Layer

Enable screen sharing and visual understanding:

```json
{
  "enable_perception": true,
  "perception_model": "raven-0"
}
```

**Use Cases:**
- Screen sharing analysis
- Visual content understanding
- Multimodal interactions

### STT Layer (Speech-to-Text)

Enable smart turn detection for natural conversation flow:

```json
{
  "enable_smart_turn_detection": true
}
```

**Benefits:**
- Automatic detection of speech completion
- Reduced interruptions
- More natural conversation flow

### LLM Layer

Configure your language model:

```json
{
  "llm_provider": "openai",
  "llm_model": "gpt-4",
  "llm_api_key": "your_api_key",
  "llm_base_url": "https://api.openai.com/v1"
}
```

**Supported Providers:**
- `openai`: OpenAI GPT models
- `anthropic`: Anthropic Claude models
- `custom`: Custom API endpoints

### TTS Layer

Configure text-to-speech:

```json
{
  "tts_provider": "cartesia",
  "tts_voice_id": "your_voice_id"
}
```

## Testing the Integration

### 1. Check Agent Logs

When the agent starts, you should see:

```
[INFO] TavusExtension on_start
[INFO] Tavus configuration loaded: persona_id=, auto_create_persona=true
[INFO] Creating Tavus persona with payload: {...}
[INFO] Tavus persona created successfully: per_xxxxxxxxxxxxx
```

### 2. Connect a Client

Use the TEN Framework playground or your custom frontend to connect:

1. Open the frontend (typically at `http://localhost:3000`)
2. Enter the Agora channel name: `tavus_conversation`
3. Click "Join"

### 3. Verify Conversation Creation

When a user joins, check logs for:

```
[INFO] User joined, starting Tavus conversation
[INFO] Creating Tavus conversation with payload: {...}
[INFO] Tavus conversation created: conv_xxxxxxxxxxxxx
[INFO] Conversation URL: https://tavus.io/c/xxxxxxxxxxxxx
```

### 4. Join the Tavus Conversation

The conversation URL will be sent to the frontend via a data message. You can:

- Open the URL in a new browser tab
- Embed it in an iframe in your frontend
- Use the Tavus JavaScript SDK for deeper integration

## Troubleshooting

### Issue: "Failed to load Tavus configuration"

**Solution:** Verify environment variables are set correctly:
```bash
env | grep TAVUS
```

### Issue: "Failed to create Tavus persona: 401"

**Solution:** Check your Tavus API key is valid and active.

### Issue: "No persona_id available"

**Solution:** Either:
- Set `auto_create_persona: true` and configure LLM settings
- Or provide an existing `persona_id` in environment variables

### Issue: "Failed to create conversation: 400"

**Solution:** Check that:
- `replica_id` is valid
- `persona_id` exists in your Tavus account
- All required configuration fields are set

## Frontend Integration

### Receiving Conversation URL

**Option 1 – WebSocket (recommended)**

Connect to the built-in WebSocket server (`ws://localhost:8765`) and listen for `text_data`
messages with `data_type: "tavus_event"`:

```javascript
const ws = new WebSocket("ws://localhost:8765");
ws.onmessage = (event) => {
  const message = JSON.parse(event.data);

  if (
    message?.data?.data_type === "tavus_event" &&
    message.data.event === "conversation_created"
  ) {
    const url = message.data.payload.conversation_url;
    console.log("Tavus conversation URL:", url);
    window.open(url, "_blank");
  }
};
```

**Option 2 – Agora data channel**

Listen for the `tavus_conversation_created` data message:

```javascript
// Example: Handling Tavus events
agoraClient.on('stream-message', (uid, data) => {
  const message = JSON.parse(data);

  if (message.type === 'tavus_conversation_created') {
    const conversationUrl = message.conversation_url;
    console.log('Tavus conversation URL:', conversationUrl);

    // Option 1: Open in new window
    window.open(conversationUrl, '_blank');

    // Option 2: Embed in iframe
    document.getElementById('tavus-iframe').src = conversationUrl;
  }
});
```

### Embedding with Tavus SDK

For a more integrated experience, use the Tavus JavaScript SDK:

```html
<script src="https://cdn.tavus.io/sdk/v1/tavus-sdk.js"></script>
<script>
  const tavus = new TavusSDK({
    apiKey: 'your_api_key',
    onReady: () => console.log('Tavus ready'),
    onError: (error) => console.error('Tavus error:', error)
  });

  // Join conversation when URL is received
  tavus.joinConversation(conversationUrl);
</script>
```

## Advanced Configuration

### Custom System Prompts

Tailor the AI's behavior with detailed system prompts:

```json
{
  "system_prompt": "You are an expert in real-time AI systems. You help developers understand the TEN framework architecture, debug issues, and implement advanced features. Always provide code examples when relevant.",
  "context": "You are assisting a developer working on a real-time multimodal AI application using the TEN framework. The user may ask about extension development, graph configuration, or troubleshooting."
}
```

### Language Configuration

Support multiple languages:

```json
{
  "language": "multilingual"
}
```

Supported values: `en`, `es`, `fr`, `de`, `pt`, `zh`, `ja`, `ko`, `multilingual`

### Recording Conversations

Enable conversation recording:

```json
{
  "enable_recording": true
}
```

Recordings will be available in your Tavus dashboard.

## Production Considerations

1. **API Key Security**: Never commit API keys to version control. Use environment variables or secrets management.

2. **Persona Reuse**: In production, create personas once and reuse them by setting `persona_id` directly instead of using `auto_create_persona`.

3. **Rate Limits**: Be aware of Tavus API rate limits for persona and conversation creation.

4. **Error Handling**: Implement proper error handling in your frontend for conversation failures.

5. **Cost Management**: Set `max_call_duration` to prevent runaway costs.

## Resources

- [Tavus Documentation](https://docs.tavus.io)
- [Tavus API Reference](https://docs.tavus.io/api-reference)
- [TEN Framework Documentation](https://github.com/TEN-framework/ten-framework)
- [Agora RTC Documentation](https://docs.agora.io)

## Support

For issues related to:
- **Tavus**: Contact Tavus support at support@tavus.io
- **TEN Framework**: Open an issue on [GitHub](https://github.com/TEN-framework/ten-framework)
- **This Extension**: Check the README.md or open an issue in the TEN Framework repository
