# Voice Assistant (with PowerMem)

中文版本: [README.zh-CN.md](README.zh-CN.md)

A memory-enabled voice assistant built on the TEN Framework. PowerMem adds durable, semantic memory so the agent can recall user preferences, summarize prior context, and keep behavior consistent across sessions.

This doc focuses on the "why" and "how" of memory in the agent, not API reference details.

## Why memory for voice agents?

Voice conversations are ephemeral. Without memory, every session resets to zero and the agent repeats basic questions (name, preferences, goals). Memory lets the agent:

- Maintain continuity across sessions ("last time you said you prefer concise answers")
- Personalize behavior without re-asking the same questions
- Resolve ambiguous follow-ups by recalling prior context
- Improve experience over time with accumulated preferences and facts

## Architecture overview

The TEN graph wires speech, LLM, and TTS. PowerMem plugs into the control layer to retrieve and save memory.

```mermaid
flowchart LR
  mic((User Voice)) --> agora[agora_rtc]
  agora --> stream[streamid_adapter]
  stream --> stt[deepgram_asr_python]
  stt --> main[main_python (main_control)]
  main --> llm[openai_llm2_python]
  llm --> main
  main --> tts[elevenlabs_tts2_python]
  tts --> agora
  main --> collector[message_collector2]

  main -. "search / add" .-> powermem[PowerMem\n(UserMemory/Memory)]
```

Key idea: `main_python` is the orchestrator. It decides when to retrieve memory, how to inject it into the LLM context, and when to save new memory.

## How PowerMem fits into the TEN workflow

PowerMem is wired through the `main_control` extension:

- On **user join**, `main_python` requests a user profile from PowerMem to generate a personalized greeting.
- On **final ASR results**, it runs a semantic search in PowerMem and injects related memory into the LLM prompt.
- On **final LLM responses**, it saves new conversation turns to PowerMem on a configurable schedule.
- On **idle timeout** or **shutdown**, it saves any unsaved conversation turns.

All of this is driven by `powermem_config` and `enable_memorization` in `tenapp/property.json`.

## What data is stored, retrieved, and updated?

Stored (written to PowerMem):
- `messages`: Only user + assistant messages from the LLM context (system messages are filtered out)
- `user_id` and `agent_id`: Used to scope memories per user and per agent

Retrieved (read from PowerMem):
- **Related memories** for the current user query (semantic search results)
- **User profile** summary for greeting personalization (from `UserMemory.profile()` when enabled)

Updated (over time):
- The agent only adds new conversation turns; PowerMem handles indexing, embeddings, and ranking

## Step-by-step walkthrough (one conversation turn)

1. **Audio in**: User speaks into the Agora channel.
2. **ASR**: STT produces a final transcript.
3. **Memory search**: `main_python` queries PowerMem with the transcript.
4. **Prompt augmentation**: Related memories are injected into the LLM prompt alongside the user query.
5. **LLM response**: The LLM responds with memory-aware context.
6. **TTS output**: Assistant audio is streamed back via Agora.
7. **Memory save**: The turn is saved to PowerMem by interval or idle timeout.

## Memory lifecycle and save rules

The agent uses two coordinated save triggers:

- **Turn-based save**: every `memory_save_interval_turns` (default 5)
- **Idle save**: after `memory_idle_timeout_seconds` of inactivity (default 30s)

Pseudo-flow (simplified):

```text
on_final_llm_response:
  if turns_since_last_save >= interval:
    save_to_powermem()
    cancel_idle_timer()
  else:
    start_or_reset_idle_timer()

on_idle_timeout:
  if unsaved_turns:
    save_to_powermem()
```

On shutdown, the agent saves any remaining unsaved turns.

## How memory changes behavior over time

Over multiple sessions, the agent can:

- Greet the user with known preferences ("Welcome back. Still prefer short answers?")
- Use past tasks to infer intent ("You asked about hiking last week, here are new trails")
- Maintain consistent tone and tool usage by recalling user feedback

This happens because related memories are inserted into the LLM context before each response.

## Annotated configuration (main_control)

```json
{
  "name": "main_control",
  "addon": "main_python",
  "property": {
    "agent_id": "voice_assistant_agent",
    "user_id": "user",
    "enable_memorization": true,
    "enable_user_memory": true,
    "memory_save_interval_turns": 5,
    "memory_idle_timeout_seconds": 30.0,
    "powermem_config": {
      "vector_store": { "...": "OceanBase/SeekDB settings" },
      "llm": { "...": "LLM for memory processing" },
      "embedder": { "...": "Embedding model for search" }
    }
  }
}
```

Tip: Use unique `user_id` values to isolate memory per person, and unique `agent_id` values to isolate memory per assistant persona.

## Key files to explore

- `ai_agents/agents/examples/voice-assistant-with-PowerMem/tenapp/property.json` - Graph wiring + PowerMem config
- `ai_agents/agents/examples/voice-assistant-with-PowerMem/tenapp/ten_packages/extension/main_python/extension.py` - Retrieval + save logic
- `ai_agents/agents/examples/voice-assistant-with-PowerMem/tenapp/ten_packages/extension/main_python/memory.py` - PowerMem store adapters
- `ai_agents/agents/examples/voice-assistant-with-PowerMem/tenapp/ten_packages/extension/main_python/prompt.py` - Memory injection templates

## PowerMem configuration

Set the environment variables in `.env`:

```bash
# Database
DATABASE_PROVIDER=oceanbase
OCEANBASE_HOST=127.0.0.1
OCEANBASE_PORT=2881
OCEANBASE_USER=root
OCEANBASE_PASSWORD=password
OCEANBASE_DATABASE=oceanbase
OCEANBASE_COLLECTION=memories

# LLM Provider (for PowerMem)
LLM_PROVIDER=qwen
LLM_API_KEY=your_qwen_api_key
LLM_MODEL=qwen-plus

# Embedding Provider (for PowerMem)
EMBEDDING_PROVIDER=qwen
EMBEDDING_API_KEY=your_qwen_api_key
EMBEDDING_MODEL=text-embedding-v4
EMBEDDING_DIMS=1536
```

## Quick start

1. **Start SeekDB server**
   ```bash
   docker run -d \
      --name seekdb \
      -p 2881:2881 \
      -p 2886:2886 \
      -v ./data:/var/lib/oceanbase \
      -e SEEKDB_DATABASE=powermem \
      -e ROOT_PASSWORD=password \
      oceanbase/seekdb:latest
   ```

2. **Install dependencies**
   ```bash
   task install
   ```

3. **Run the voice assistant with PowerMem**
   ```bash
   task run
   ```

4. **Access the application**
   - Frontend: http://localhost:3000
   - API Server: http://localhost:8080
   - TMAN Designer: http://localhost:49483
