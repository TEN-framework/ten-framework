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
export TAVUS_REPLICA_ID="your_replica_id"

# Optional: Use existing persona
export TAVUS_PERSONA_ID="your_persona_id"

# Optional: LLM Configuration (for auto-created personas)
export TAVUS_LLM_PROVIDER="openai"
export TAVUS_LLM_MODEL="gpt-4"
export TAVUS_LLM_API_KEY="your_openai_api_key"

# Agora RTC (required for TEN framework)
export AGORA_APP_ID="your_agora_app_id"
export AGORA_APP_CERTIFICATE="your_agora_app_certificate"
```

### Property Configuration

Edit `property.json` to customize conversation settings:

#### Basic Configuration (Using Existing Persona)

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

#### Full Pipeline Configuration (Auto-Create Persona)

```json
{
  "tavus_api_key": "${env:TAVUS_API_KEY}",
  "replica_id": "${env:TAVUS_REPLICA_ID}",
  "conversation_name": "TEN Agent Conversation",
  "max_call_duration": 3600,
  "enable_recording": false,
  "language": "en",
  "auto_create_persona": true,
  "persona_name": "TEN Assistant",
  "system_prompt": "You are a helpful AI assistant integrated with the TEN framework. Help users with conversational AI tasks.",
  "context": "You are running in a real-time conversational environment.",
  "enable_perception": false,
  "perception_model": "raven-0",
  "enable_smart_turn_detection": true,
  "llm_provider": "${env:TAVUS_LLM_PROVIDER}",
  "llm_model": "${env:TAVUS_LLM_MODEL}",
  "llm_api_key": "${env:TAVUS_LLM_API_KEY}",
  "tts_provider": "",
  "tts_voice_id": ""
}
```

## Persona Management

### Option 1: Auto-Create Persona (Recommended)

Set `auto_create_persona: true` in your property.json and configure the persona settings. The extension will automatically create a persona on startup using the Tavus Full Pipeline API.

**Benefits:**
- Automatic persona creation and management
- Full control over all pipeline layers (Perception, STT, LLM, TTS)
- Customizable system prompts and context
- Persona is created once per agent startup

### Option 2: Manual Persona Creation

Create a persona manually via the Tavus API:

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
      "perception": {
        "model": "raven-0"
      },
      "stt": {
        "smart_endpointing": true
      },
      "llm": {
        "provider": "openai",
        "model": "gpt-4",
        "api_key": "YOUR_OPENAI_API_KEY"
      },
      "tts": {
        "provider": "cartesia",
        "voice_id": "YOUR_VOICE_ID"
      }
    }
  }'
```

Then set the returned `persona_id` in your property.json.

## Full Pipeline Layers

The Tavus Full Pipeline supports four customizable layers:

### 1. Perception Layer (Optional)
- **Purpose**: Enable screen sharing and visual understanding capabilities
- **Model**: `raven-0` (default)
- **Configuration**: Set `enable_perception: true`

### 2. Speech-to-Text (STT) Layer
- **Purpose**: Convert user speech to text
- **Smart Turn Detection**: Automatically detects when the user has finished speaking
- **Configuration**: Set `enable_smart_turn_detection: true` (enabled by default)

### 3. Language Model (LLM) Layer
- **Purpose**: Generate conversational responses
- **Supported Providers**: OpenAI, Anthropic, custom endpoints
- **Configuration**:
  ```json
  {
    "llm_provider": "openai",
    "llm_model": "gpt-4",
    "llm_api_key": "your_api_key",
    "llm_base_url": "https://api.openai.com/v1"
  }
  ```

### 4. Text-to-Speech (TTS) Layer
- **Purpose**: Convert AI responses to natural speech
- **Supported Providers**: Various TTS providers
- **Configuration**:
  ```json
  {
    "tts_provider": "cartesia",
    "tts_voice_id": "your_voice_id"
  }
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

- `tavus_persona_created`: Emitted when persona is auto-created
  - `persona_id` (string): Unique persona identifier

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
