# Voice Assistant Tavus - Quick Start

Get your Tavus AI avatar talking in 5 minutes!

## Prerequisites

You need API keys from:
1. **Agora** - [console.agora.io](https://console.agora.io) (for audio/video)
2. **Tavus** - [platform.tavus.io](https://platform.tavus.io) (for AI avatar)
3. **OpenAI** - [platform.openai.com](https://platform.openai.com) (for conversation AI)

## Step 1: Get Your API Keys

### Agora
1. Sign up at https://console.agora.io
2. Create a project
3. Copy your **App ID**

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

Navigate to the ai_agents directory and create/edit `.env`:

```bash
cd /Users/chenyifan/Code/ten-framework/ai_agents
```

Add these lines to `.env`:

```bash
# Agora
AGORA_APP_ID=your_agora_app_id_here

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
cd /Users/chenyifan/Code/ten-framework/ai_agents/agents/examples/voice-assistant-tavus

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

## Step 5: Open the Frontend

1. Open your browser to: **http://localhost:3000**

2. You should see the TEN Agent playground interface

3. Enter a channel name (or use default: `tavus_voice_assistant`)

4. Click **"Join"** or **"Connect"**

## Step 6: Get the Conversation URL

### Option A: Check the Logs

Open another terminal and watch the logs:

```bash
cd /Users/chenyifan/Code/ten-framework/ai_agents/agents/examples/voice-assistant-tavus
tail -f tenapp/logs/app.log | grep -i tavus
```

When you join, you'll see:
```
[INFO] User joined, starting Tavus conversation
[INFO] Creating Tavus conversation with payload: {...}
[INFO] Tavus conversation created: conv_xxxxxxxxxxxxx
[INFO] Conversation URL: https://tavus.io/c/xxxxxxxxxxxxx
```

### Option B: Check Browser Console

1. Open browser DevTools (Press F12)
2. Go to Console tab
3. Look for messages from Agora data channel
4. Find the `tavus_conversation_created` message with the URL

## Step 7: Join the Tavus Conversation

1. **Copy the conversation URL** from logs or console
   - It looks like: `https://tavus.io/c/xxxxxxxxxxxxx`

2. **Open the URL** in a new browser tab

3. **Allow camera and microphone** when prompted

4. **Start talking!** Your AI avatar will:
   - Listen to you
   - Process with GPT-4
   - Respond with natural voice
   - Show lip-synced video

## Example Conversation

Try saying:
- "Hello, can you hear me?"
- "Tell me about the TEN framework"
- "What can you help me with?"

The avatar should respond naturally!

## What's Happening Behind the Scenes?

```
1. Agent starts → Creates Tavus persona with GPT-4
                    ↓
2. You join channel → Agent creates Tavus conversation
                    ↓
3. Conversation URL → Sent to your browser
                    ↓
4. You open URL → Talk to AI avatar
                    ↓
5. You leave → Conversation ends automatically
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

## Advanced: Auto-Open Tavus URL

Want the conversation to open automatically? Modify the playground frontend:

1. Open `/Users/chenyifan/Code/ten-framework/ai_agents/playground/src/app/page.tsx`

2. Add this code where Agora data messages are handled:

```typescript
// Listen for Tavus conversation URL
rtcEngine.on('streamMessage', (uid, data) => {
  try {
    const message = JSON.parse(new TextDecoder().decode(data));

    if (message.type === 'tavus_conversation_created') {
      const url = message.conversation_url;
      console.log('🎥 Tavus Conversation:', url);

      // Auto-open in new tab
      window.open(url, '_blank');

      // Or show a button
      setTavusUrl(url);
    }
  } catch (e) {
    console.error('Error parsing message:', e);
  }
});
```

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
