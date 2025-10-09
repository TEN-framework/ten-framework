# Tavus Conversational Video Interface Extension

This extension integrates Tavus's Conversational Video Interface (CVI) into the TEN framework, enabling AI-powered video conversations with lifelike digital avatars.

## Overview

Tavus provides an end-to-end conversational AI video pipeline with:
- Hyper-realistic digital human avatars (Phoenix replicas)
- Natural voice conversations with AI personas
- Real-time video streaming via WebRTC (Daily.co)
- Customizable LLM, TTS, STT, and perception layers

## Prerequisites

1. **Tavus Account**: Sign up at [platform.tavus.io](https://platform.tavus.io)
2. **API Key**: Generate from Developer Portal at platform.tavus.io/api-keys
3. **Persona**: Create a persona via Tavus API or dashboard
4. **Replica**: Create or select a digital avatar replica

## Configuration

### Environment Variables

Set the following environment variables:

```bash
# Tavus Configuration
export TAVUS_API_KEY="your_tavus_api_key"
export TAVUS_PERSONA_ID="your_persona_id"
export TAVUS_REPLICA_ID="your_replica_id"

# Agora RTC (required for TEN framework)
export AGORA_APP_ID="your_agora_app_id"
export AGORA_APP_CERTIFICATE="your_agora_app_certificate"
```

### Property Configuration

Edit `property.json` to customize conversation settings:

```json
{
  "tavus_api_key": "${env:TAVUS_API_KEY}",
  "persona_id": "${env:TAVUS_PERSONA_ID}",
  "replica_id": "${env:TAVUS_REPLICA_ID}",
  "conversation_name": "TEN Agent Conversation",
  "max_call_duration": 3600,
  "enable_recording": false,
  "language": "en"
}
```

## Creating a Persona

Use the Tavus API to create a persona:

```bash
curl -X POST https://tavusapi.com/v2/personas \
  -H "x-api-key: YOUR_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "persona_name": "TEN Assistant",
    "system_prompt": "You are a helpful AI assistant integrated with the TEN framework.",
    "pipeline_mode": "full",
    "context": "You help users with conversational AI tasks.",
    "default_replica_id": "YOUR_REPLICA_ID",
    "layers": {
      "stt": {
        "smart_endpointing": true
      }
    }
  }'
```

## How It Works

1. **User Joins**: When a user joins the Agora RTC channel, the extension automatically creates a Tavus conversation
2. **Conversation Created**: The extension sends a `tavus_conversation_created` data message with the conversation URL
3. **Video Interface**: Users can join the Tavus conversation via the provided URL
4. **User Leaves**: When the user leaves, the conversation is automatically terminated

## API

### Commands In

- `on_user_joined`: Triggered when user joins (auto-starts conversation)
- `on_user_left`: Triggered when user leaves (auto-ends conversation)
- `start_conversation`: Manually start a Tavus conversation
- `end_conversation`: Manually end the conversation

### Data Out

- `tavus_conversation_created`: Emitted when conversation is created
  - `conversation_id` (string): Unique conversation identifier
  - `conversation_url` (string): URL to join the conversation

## Architecture Notes

### WebRTC Bridging

Tavus uses its own WebRTC infrastructure (Daily.co) separate from Agora RTC. This extension creates conversations via the Tavus API and provides the conversation URL for users to join directly.

For full integration, you would need to:
1. Bridge audio/video between Agora RTC and Tavus/Daily
2. Implement media forwarding logic
3. Handle codec translation if needed

### Alternative Integration

Consider using Tavus's JavaScript SDK in your frontend to embed the conversation directly in your application UI, while using this extension for conversation lifecycle management.

## Dependencies

- `aiohttp`: Async HTTP client for Tavus API calls
- `ten_runtime_python`: TEN framework runtime
- `ten_ai_base`: TEN AI base system

Install Python dependencies:

```bash
pip install aiohttp
```

## Usage Example

1. Start the TEN agent with the Tavus example:

```bash
cd ai_agents/agents/examples/tavus
./bin/start
```

2. Connect a client to the Agora RTC channel
3. The extension will automatically create a Tavus conversation
4. Join the conversation using the provided URL

## Resources

- [Tavus Documentation](https://docs.tavus.io)
- [Tavus API Reference](https://docs.tavus.io/api-reference)
- [Tavus Examples](https://github.com/Tavus-Engineering/tavus-examples)
- [TEN Framework](https://github.com/TEN-framework/ten-framework)

## License

This extension follows the TEN framework's licensing terms.
