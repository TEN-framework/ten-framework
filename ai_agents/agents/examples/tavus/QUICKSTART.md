# Tavus Full Pipeline - Quick Start

Get up and running with Tavus conversational video in 5 minutes!

## Step 1: Get Your API Keys

### Tavus
1. Sign up at [platform.tavus.io](https://platform.tavus.io)
2. Generate API key: [platform.tavus.io/api-keys](https://platform.tavus.io/api-keys)
3. Create or note your Replica ID from the dashboard

### OpenAI (for LLM)
1. Get your API key from [platform.openai.com](https://platform.openai.com)

### Agora (for RTC)
1. Sign up at [console.agora.io](https://console.agora.io)
2. Create a project and get App ID and App Certificate

## Step 2: Set Environment Variables

Create a `.env` file in the `ai_agents/agents/examples/tavus/` directory:

```bash
# Tavus Configuration
TAVUS_API_KEY="tvs-xxx-your-api-key"
TAVUS_REPLICA_ID="r7xxx-your-replica-id"

# OpenAI for LLM
TAVUS_LLM_PROVIDER="openai"
TAVUS_LLM_MODEL="gpt-4"
TAVUS_LLM_API_KEY="sk-xxx-your-openai-key"

# Agora RTC
AGORA_APP_ID="your-agora-app-id"
AGORA_APP_CERTIFICATE="your-agora-certificate"
```

Or export them in your terminal:

```bash
export TAVUS_API_KEY="tvs-xxx-your-api-key"
export TAVUS_REPLICA_ID="r7xxx-your-replica-id"
export TAVUS_LLM_PROVIDER="openai"
export TAVUS_LLM_MODEL="gpt-4"
export TAVUS_LLM_API_KEY="sk-xxx-your-openai-key"
export AGORA_APP_ID="your-agora-app-id"
export AGORA_APP_CERTIFICATE="your-agora-certificate"
```

## Step 3: Configure the Agent

### Option A: Use Full Pipeline Example (Recommended)

Copy the example configuration:

```bash
cd ai_agents/agents/examples/tavus
cp property.full_pipeline.example.json property.json
```

### Option B: Update Existing Configuration

Edit your existing `property.json` and update the `tavus_control` node property:

```json
{
  "tavus_api_key": "${env:TAVUS_API_KEY}",
  "replica_id": "${env:TAVUS_REPLICA_ID}",
  "conversation_name": "My Tavus Conversation",
  "max_call_duration": 3600,
  "enable_recording": false,
  "language": "en",
  "auto_create_persona": true,
  "persona_name": "My AI Assistant",
  "system_prompt": "You are a helpful AI assistant. Be friendly and concise.",
  "context": "You are in a video conversation.",
  "enable_perception": false,
  "enable_smart_turn_detection": true,
  "llm_provider": "${env:TAVUS_LLM_PROVIDER}",
  "llm_model": "${env:TAVUS_LLM_MODEL}",
  "llm_api_key": "${env:TAVUS_LLM_API_KEY}"
}
```

## Step 4: Start the Agent

```bash
cd ai_agents/agents/examples/tavus
./bin/start
```

You should see logs like:

```
[INFO] TavusExtension on_start
[INFO] Tavus configuration loaded: persona_id=, auto_create_persona=true
[INFO] Creating Tavus persona with payload: {...}
[INFO] Tavus persona created successfully: per_xxxxx
```

## Step 5: Connect a Client

### Using TEN Playground Frontend

1. Start the frontend (if not already running):
```bash
cd ai_agents/playground
npm install
npm run dev
```

2. Open browser to `http://localhost:3000`

3. Enter configuration:
   - Channel: `tavus_conversation` (or match your property.json)
   - Click "Join"

4. Watch the logs - when you join, you'll see:
```
[INFO] User joined, starting Tavus conversation
[INFO] Tavus conversation created: conv_xxxxx
[INFO] Conversation URL: https://tavus.io/c/xxxxx
```

### Using Your Own Frontend

Connect to Agora RTC and listen for the `tavus_conversation_created` data message:

```javascript
// Listen for Tavus conversation URL
agoraRtcEngine.on('streamMessage', (uid, data) => {
  try {
    const message = JSON.parse(data);

    if (message.type === 'tavus_conversation_created') {
      console.log('Conversation URL:', message.conversation_url);

      // Open Tavus conversation in new window
      window.open(message.conversation_url, '_blank');
    }
  } catch (e) {
    console.error('Error parsing message:', e);
  }
});
```

## Step 6: Join the Video Conversation

When the conversation is created, you'll receive a URL like:
```
https://tavus.io/c/conv_xxxxxxxxxxxxx
```

Click the URL to join the video conversation with your AI avatar!

## Customization Options

### Change AI Personality

Edit the `system_prompt` in property.json:

```json
{
  "system_prompt": "You are a technical expert who helps developers with coding questions. Be detailed and provide examples.",
  "context": "You are assisting a software developer."
}
```

### Enable Screen Sharing (Perception)

```json
{
  "enable_perception": true,
  "perception_model": "raven-0"
}
```

### Change Language

```json
{
  "language": "es"  // Spanish
}
```

Supported: `en`, `es`, `fr`, `de`, `pt`, `zh`, `ja`, `ko`, `multilingual`

### Use Different LLM

```json
{
  "llm_provider": "anthropic",
  "llm_model": "claude-3-opus-20240229",
  "llm_api_key": "${env:ANTHROPIC_API_KEY}"
}
```

### Enable Recording

```json
{
  "enable_recording": true
}
```

Recordings will be available in your Tavus dashboard.

## Troubleshooting

### "Failed to create Tavus persona: 401"
- Check your `TAVUS_API_KEY` is correct
- Verify the API key is active in Tavus dashboard

### "No persona_id available"
- Make sure `auto_create_persona: true` is set
- Verify LLM configuration is correct

### "Failed to create conversation: 400"
- Check `TAVUS_REPLICA_ID` is valid
- Verify the replica exists in your Tavus account

### Conversation URL not appearing
- Check agent logs for errors
- Verify Agora RTC connection is working
- Make sure data channel is enabled in Agora configuration

## What's Next?

- Read [SETUP_GUIDE.md](./SETUP_GUIDE.md) for detailed configuration options
- Read [README.md](./ten_packages/extension/tavus_python/README.md) for API documentation
- Customize your persona's behavior with advanced prompts
- Integrate the conversation URL into your custom UI

## Architecture Overview

```
User Browser → Agora RTC → TEN Agent → Tavus Extension
                                          ↓
                                     Creates Persona (once)
                                     Creates Conversation (per user)
                                          ↓
                                     Returns conversation URL
                                          ↓
                             User joins Tavus video conversation
```

## Key Features You're Using

✅ **Auto Persona Creation** - No manual API calls needed
✅ **Full Pipeline** - Perception, STT, LLM, TTS all configurable
✅ **Smart Turn Detection** - Natural conversation flow
✅ **Lifecycle Management** - Automatic start/stop with user join/leave
✅ **Environment Variables** - Secure configuration management

---

**Need Help?**
- Check logs: `tail -f logs/agent.log`
- Detailed setup: [SETUP_GUIDE.md](./SETUP_GUIDE.md)
- TEN Framework docs: [GitHub](https://github.com/TEN-framework/ten-framework)
