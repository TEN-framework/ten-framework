# Voice Assistant Tavus - Quick Start

Get your Tavus AI avatar talking in 5 minutes!

## Prerequisites

You need API keys from:
1. **Tavus** - [platform.tavus.io](https://platform.tavus.io) (for the avatar)
2. **OpenAI (or another LLM)** - [platform.openai.com](https://platform.openai.com)

## Step 1: Get Your API Keys

### Tavus
1. Sign up at https://platform.tavus.io
2. Go to https://platform.tavus.io/api-keys
3. Generate an **API Key** (starts with `tvs-`)
4. Go to Replicas section
5. Note your **Replica ID** (starts with `r7`)

### OpenAI
1. Sign up at https://platform.openai.com
2. Generate an **API Key** (starts with `sk-`)

## Step 2: Set Environment Variables

Navigate to the `ai_agents` directory and create/edit `.env`:

```bash
cd ai_agents
```

Add these lines to `.env`:

```bash
# Tavus
TAVUS_API_KEY=tvs-xxx-your-tavus-key
TAVUS_REPLICA_ID=r7xxx-your-replica-id

# OpenAI
TAVUS_LLM_PROVIDER=openai
TAVUS_LLM_MODEL=gpt-4
TAVUS_LLM_API_KEY=sk-xxx-your-openai-key
```

**Pro tip**: Use `gpt-3.5-turbo` if you don't have GPT-4 access - it's cheaper and works fine!

## Step 3: Install Dependencies

```bash
cd ai_agents/agents/examples/voice-assistant-tavus

# Install everything (takes 2-3 minutes)
task install
```

You should see:
```
✓ Installing tenapp dependencies
✓ Building Go application
✓ Installing Python dependencies
✓ Installing frontend dependencies
✓ Building API server
```

## Step 4: Start the Agent

```bash
# Start everything
task run
```

You should see three services starting:
- **TMAN Designer** - http://localhost:49483
- **Frontend** - http://localhost:3000
- **API Server** - http://localhost:8080

Wait until you see:
```
[INFO] TavusExtension on_start
[INFO] Creating Tavus persona...
[INFO] Tavus persona created successfully: per_xxxxx
```

## Step 5: Use the Tavus Studio UI

1. Open your browser to **http://localhost:3000** — this is the new micro frontend dedicated to Tavus.
2. Pick a channel name (or keep the default), then click **Start Session**.
3. The status pill will show `Running` once the TEN worker is alive and the websocket is receiving `tavus_event` updates.
4. When the conversation is ready, an embedded Tavus Conversational Video Interface appears. Grant camera & microphone access and start chatting.
5. Click **Stop** to tear everything down (the iframe disappears and the worker shuts down).

## Step 6: (Optional) Inspect the WebSocket Feed

The UI already listens to `ws://localhost:8765`, but you can watch the raw events for debugging:

```bash
npx wscat -c ws://localhost:8765
```

You should see events such as `persona_created`, `conversation_created`, and `conversation_ended`. Each payload contains IDs and URLs you can use elsewhere.

## Step 7: Join the Tavus Conversation (Automatically)

As soon as `conversation_created` fires, the UI injects the iframe and you are inside the Tavus Daily-powered experience. You can still pop it out into its own tab via the “Open in new tab” link if you prefer.

## Example Conversation

Try saying:
- "Hello, can you hear me?"
- "Tell me about the TEN framework"
- "What can you help me with?"

The avatar should respond naturally!

## What's Happening Behind the Scenes?

```
1. task run          → boots TEN + API + websocket broadcaster
2. Start Session     → /start spins a worker + Tavus persona/conversation
3. Websocket event   → UI embeds the Tavus CVI iframe instantly
4. Talk inside iframe→ Tavus handles media via Daily
5. Stop Session      → TEN worker shuts down & Tavus room closes
```

## Customizing Your Avatar

Want to change how your avatar talks? Edit `tenapp/property.json`:

```json
{
  "property": {
    "system_prompt": "You are a friendly tech support agent. Help users with their questions about software. Be patient and clear.",
    "persona_name": "Tech Support Bot"
  }
}
```

Then restart:
```bash
# Stop with Ctrl+C, then:
task run
```

## Troubleshooting

### "Failed to create Tavus persona: 401"
❌ Your `TAVUS_API_KEY` is wrong
✅ Double-check the key in your `.env` file

### "No persona_id available"
❌ Your `TAVUS_LLM_API_KEY` might be invalid
✅ Verify your OpenAI API key has credits

### "Failed to create conversation: 400"
❌ Your `TAVUS_REPLICA_ID` is wrong
✅ Check the replica ID in Tavus dashboard

### Agent won't start
```bash
# Re-install dependencies
task install

# Check if tman is installed
tman --version

# Check environment variables
cat ../.env | grep TAVUS
```

### Frontend won't load
```bash
# Check if services are running
curl http://localhost:8080/health
curl http://localhost:3000

# If not, check if ports are available
lsof -i :8080
lsof -i :3000
```

### No conversation URL appearing

1. Check agent logs:
```bash
tail -f tenapp/logs/app.log
```

2. Look for errors after you join the channel

3. Verify you're joining the correct channel name

## Advanced: Customize the Tavus Studio UI

All of the UI code lives in `ai_agents/agents/examples/voice-assistant-tavus/frontend`. Feel free to tweak `src/App.tsx` to match your brand (colors, typography, copy). The websocket handler already lives inside the `useEffect` hook—if you want to perform additional actions (e.g., fire analytics, preload assets) simply extend the `handleMessage` function to react to more `tavus_event` payloads.

## Next Steps

- ✅ Got it working? Try customizing the system prompt!
- 📖 Read the full [README.md](./README.md) for advanced configuration
- 🏗️ Check [STRUCTURE.md](./STRUCTURE.md) to understand the architecture
- 🎨 Add more extensions to enhance capabilities

## Need Help?

- **Logs**: `tail -f tenapp/logs/app.log`
- **TEN Framework**: https://github.com/TEN-framework/ten-framework
- **Tavus Docs**: https://docs.tavus.io
- **This Agent**: See README.md

---

**Enjoy your AI avatar! 🎉**
