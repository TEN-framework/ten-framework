# Voice Assistant (with EverMemOS)

An intelligent voice assistant integrated with [EverMemOS](https://evermemos.com/) memory management capabilities, supporting persistent conversation context and long-term memory.

## 🎯 Features

- ✅ Real-time Speech Recognition (Deepgram ASR)
- ✅ Intelligent Conversation Generation (OpenAI-compatible LLM)
- ✅ Natural Speech Synthesis (ElevenLabs TTS)
- ✅ Long-term Memory Capability (EverMemOS)
- ✅ Hybrid Search Memory Retrieval
- ✅ Automatic Memory Saving and Recall

## 📋 Prerequisites

### Required Dependencies
- Python 3.10+
- Node.js 18+
- Docker (if using local database)

### API Keys
- **Agora RTC**: For real-time audio/video communication
- **Deepgram**: Speech-to-Text (STT)
- **OpenAI-compatible API**: LLM conversation generation (e.g., SiliconFlow, OpenAI, etc.)
- **ElevenLabs**: Text-to-Speech (TTS)
- **EverMemOS**: Memory system API Key

## 🚀 Quick Start

### 1. Get EverMemOS API Key

Visit [EverMemOS](https://evermemos.com/) to register and obtain an API Key.

EverMemOS is a cloud-hosted service, **no local database deployment required**.

### 2. Configure Environment Variables

Create a `.env` file in the project root directory:

```bash
# Agora RTC
AGORA_APP_ID=your_agora_app_id
AGORA_APP_CERTIFICATE=your_agora_certificate

# Deepgram STT
DEEPGRAM_API_KEY=your_deepgram_api_key

# LLM Provider (SiliconFlow example)
OPENAI_BASE_URL=https://api.siliconflow.cn/v1
OPENAI_API_KEY=your_siliconflow_api_key
OPENAI_MODEL=Qwen/Qwen2.5-14B-Instruct

# ElevenLabs TTS
ELEVENLABS_API_KEY=your_elevenlabs_api_key

# Proxy (optional)
PROXY_URL=http://127.0.0.1:7890
```

### 3. Configure EverMemOS

Edit the `evermemos_config` section in `tenapp/property.json`:

```json
{
  "property": {
    "greeting": "Hello, I'm your personal assistant",
    "agent_id": "voice_assistant_agent",
    "user_id": "user",
    "enable_memorization": true,
    "memory_save_interval_turns": 2,
    "memory_idle_timeout_seconds": 10.0,
    "evermemos_config": {
      "api_key": "your_evermemos_api_key_here"
    }
  }
}
```

**Configuration Parameters**:
- `agent_id`: Unique identifier for the assistant
- `user_id`: Unique identifier for the user (customizable, e.g., username or UUID)
- `enable_memorization`: Enable memory functionality (set to `true`)
- `memory_save_interval_turns`: Auto-save memory every N conversation turns (recommended 2-5)
- `memory_idle_timeout_seconds`: Auto-save after N seconds of conversation idle (recommended 10-30)
- `evermemos_config.api_key`: Your EverMemOS API Key

### 4. Install Dependencies

```bash
# Install Python dependencies
pip install -r requirements.txt

# Ensure evermemos SDK is installed
pip install evermemos
```

### 5. Run the Service

```bash
# Start backend service
task run

# Or use Docker
docker-compose up -d
```

### 6. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483

## 💡 How It Works

### Memory Saving Mechanism

The assistant automatically saves conversation memory in the following scenarios:

1. **Periodic Save**: Every N conversation turns (default 2 turns)
2. **Idle Save**: After N seconds of conversation idle (default 10 seconds)
3. **Exit Save**: Automatically save when user leaves

### Memory Retrieval Mechanism

When a user asks a question, the assistant will:

1. Use hybrid search (keywords + semantic vectors) to retrieve relevant memories
2. Inject the top 3 most relevant memories into LLM context
3. LLM naturally integrates memory content to generate responses

### Conversation Example

**Turns 1-2** (Memory Saving):
```
User: Hello
Assistant: Hello, I'm your personal assistant

User: I like black coffee, no sugar
Assistant: Got it, I'll remember you like black coffee
→ Triggered save: 2 conversation turns completed
```

**Turn 3** (Memory Retrieval):
```
User: What do I usually like to drink?
→ System retrieves memory: Found "User likes black coffee, no sugar"
Assistant: You like black coffee, and you prefer it without sugar
```

## 📊 Log Monitoring

After startup, observe EverMemOS-related information in the logs:

### Initialization Success
```
[MainControlExtension] EverMemOS memory store initialized successfully
[EverMemosMemoryStore] Initialized with API key: your_api_key...
```

### Saving Memory
```
╔══════════════════════════════════════════════════════════════════╗
║                [EverMemOS] Save Conversation to Memory            ║
╠══════════════════════════════════════════════════════════════════╣
║ 👤 User ID:     'user'                                            ║
║ 🤖 Agent ID:    'voice_assistant_agent'                           ║
║ 💬 Conversation Length: 4 messages                                ║
╚══════════════════════════════════════════════════════════════════╝
[EverMemOS] 📝 Preparing to save 4 messages
[EverMemOS] ✅ Successfully saved 4 messages to user 'user' memory
```

### Retrieving Memory
```
╔══════════════════════════════════════════════════════════════════╗
║                [EverMemOS] Search Relevant Memory                 ║
╠══════════════════════════════════════════════════════════════════╣
║ 👤 User ID:     'user'                                            ║
║ 🤖 Agent ID:    'voice_assistant_agent'                           ║
║ 🔍 Search Query: 'what do I like to drink'                        ║
╚══════════════════════════════════════════════════════════════════╝
[EverMemOS] 🔎 Searching user 'user' memory...
[EverMemOS] ✅ Search complete! Found 3 relevant memories for user 'user'
```

## 🔧 Troubleshooting

### Issue 1: No EverMemOS Logs Visible

**Possible Causes**:
- Insufficient conversation turns (haven't reached save threshold)
- evermemos package not installed
- API Key configuration error

**Solution**:
```bash
# Check evermemos package
pip list | grep evermemos

# Reinstall
pip install --upgrade evermemos

# Check startup logs for errors
# Look for logs containing "EverMemOS" or "Failed to initialize"
```

### Issue 2: Memory Save Failed

**Checkpoints**:
1. Is the API Key correct?
2. Is network connection normal?
3. Error messages in logs

### Issue 3: Cannot Retrieve Memory

**Possible Causes**:
- Memory hasn't been saved successfully yet
- Retrieval query has too low relevance to memory content

**Suggestions**:
- Wait for enough turns or time to trigger save
- Use more specific questions

## 📖 API Documentation

### EverMemOS SDK Usage

This project uses the [evermemos Python SDK](https://pypi.org/project/evermemos/):

```python
from evermemos import EverMemOS

# Initialize
client = EverMemOS(api_key="your_api_key")
memory = client.v0.memories

# Save message
response = memory.add(
    message_id="msg_001",
    create_time="2025-01-15T10:00:00Z",
    sender="user",
    sender_name="User",
    group_id="user_voice_assistant_agent",
    content="I like black coffee",
    flush="true"  # Last message triggers memory extraction
)

# Search memory
response = memory.search(extra_query={
    "query": "coffee preference",
    "user_id": "user",
    "retrieve_method": "hybrid",  # Hybrid search
    "memory_types": ["episodic_memory"],
    "top_k": 10
})
```

## 🎨 Custom Configuration

### Modify Save Frequency

Edit `property.json`:

```json
{
  "memory_save_interval_turns": 5,     // Save every 5 turns
  "memory_idle_timeout_seconds": 30.0  // Save after 30 seconds idle
}
```

### Modify Number of Retrieved Memories

Edit the `_retrieve_related_memory` method in `extension.py`:

```python
# Default: top 3
memorise = [
    result["memory"]
    for result in results[:3]  # Change to [:5] for top 5
    if isinstance(result, dict) and result.get("memory")
]
```

### Modify Greeting Message

Edit `property.json`:

```json
{
  "greeting": "Hello! I'm your intelligent assistant, and I can remember our previous conversations"
}
```

## 📚 Related Documentation

- [EverMemOS Official Documentation](https://evermemos.com/docs)
- [evermemos Python SDK](https://github.com/memogpt/evermemos-python)
- [TEN Framework Documentation](https://doc.theten.ai/)
- [Project Change Log](./EVERMEMOS_MIGRATION_SUMMARY.md)
- [Memory Optimization Notes](./MEMORY_OPTIMIZATION_CHANGES.md)

## 🤝 Contributing

Issues and Pull Requests are welcome!

## 📄 License

This project is open-sourced under the Apache 2.0 License.

## 💬 Support

Having issues?
- Check the [Troubleshooting](#🔧-troubleshooting) section
- Submit an [Issue](https://github.com/your-repo/issues)
- Join our community discussion

---

**Note**: This project uses the EverMemOS cloud service, no local database deployment required. Simply obtain an API Key to use the full memory management functionality.