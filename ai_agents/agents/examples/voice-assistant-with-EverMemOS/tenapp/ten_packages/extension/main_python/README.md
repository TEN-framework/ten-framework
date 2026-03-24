# Voice Assistant with PowerMem Integration

This extension integrates [PowerMem](https://github.com/oceanbase/powermem/) memory functionality, enabling the voice assistant to remember previous conversation content and provide more personalized and coherent interaction experiences.

## Features

1. **Conversation Memory**: Automatically records user and assistant conversation content
2. **Semantic Search**: Retrieves relevant memories based on user queries using semantic search
3. **Smart Memory**: Automatically saves to PowerMem based on configurable rules (by turn interval or idle timeout)
4. **Personalized Greeting**: Generates personalized greetings based on user memories when user joins
5. **Configurable**: Supports enabling/disabling memory functionality and customizing save rules through configuration

## Installation

```bash
pip install -r requirements.txt
```

The main dependencies are:
- `powermem>=0.2.0`: PowerMem SDK for memory management
- `aiohttp`: For async HTTP operations
- `pydantic`: For configuration validation

## Configuration Options

The following parameters can be set in the configuration file:

```json
{
  "greeting": "Hello! I'm your AI assistant with memory. I can remember our previous conversations to provide more personalized help.",
  "agent_id": "voice_assistant_agent",
  "user_id": "user",
  "enable_memorization": true,
  "enable_user_memory": true,
  "memory_save_interval_turns": 5,
  "memory_idle_timeout_seconds": 30.0,
  "powermem_config": {
    "vector_store": {
      "provider": "oceanbase",
      "config": {
        "collection_name": "${env:OCEANBASE_COLLECTION}",
        "host": "${env:OCEANBASE_HOST}",
        "port": "${env:OCEANBASE_PORT}",
        "user": "${env:OCEANBASE_USER}",
        "password": "${env:OCEANBASE_PASSWORD}",
        "db_name": "${env:OCEANBASE_DATABASE}",
        "embedding_model_dims": "${env:EMBEDDING_DIMS}"
      }
    },
    "llm": {
      "provider": "${env:LLM_PROVIDER}",
      "config": {
        "api_key": "${env:LLM_API_KEY}",
        "model": "${env:LLM_MODEL}"
      }
    },
    "embedder": {
      "provider": "${env:EMBEDDING_PROVIDER}",
      "config": {
        "api_key": "${env:EMBEDDING_API_KEY}",
        "model": "${env:EMBEDDING_MODEL}",
        "embedding_dims": "${env:EMBEDDING_DIMS}"
      }
    }
  }
}
```

### Configuration Description

- `greeting`: Default welcome message when user joins (will be replaced with personalized greeting if memories exist)
- `agent_id`: Unique identifier for the agent (used to isolate memories per agent)
- `user_id`: Unique identifier for the user (used to isolate memories per user)
- `enable_memorization`: Enable or disable memory functionality (default: `true`)
- `enable_user_memory`: Enable or disable user memory mode (default: `true`). When `true`, uses `UserMemory` client which provides enhanced user profile functionality via `client.profile()`. When `false`, uses standard `Memory` client
- `memory_save_interval_turns`: Number of conversation turns before automatically saving memory (default: `5`)
- `memory_idle_timeout_seconds`: Number of seconds of inactivity before automatically saving memory (default: `30.0`)
- `powermem_config`: PowerMem SDK configuration dictionary containing:
  - `vector_store`: Vector database configuration (e.g., OceanBase, supports `${env:VAR_NAME}` syntax for environment variables)
  - `llm`: LLM provider configuration for memory processing
  - `embedder`: Embedding model configuration for semantic search

## Workflow

1. **Initialization**: Initialize PowerMem client using `powermem_config` dictionary on startup. The extension selects the appropriate client based on `enable_user_memory`:
   - If `enable_user_memory` is `true`: Uses `UserMemory` client (provides enhanced profile functionality)
   - If `enable_user_memory` is `false`: Uses standard `Memory` client
2. **User Joins**: Generate personalized greeting based on user memories (if enabled). The greeting is generated asynchronously using LLM with a 10-second timeout
3. **Conversation Processing**: Real-time recording of user input and assistant responses from LLM context
4. **Memory Retrieval**: When user sends a query, search for related memories using semantic search and add formatted results to LLM context via `CONTEXT_MESSAGE_WITH_MEMORY_TEMPLATE`
5. **Memory Saving**: Automatically save conversation to PowerMem based on configured rules:
   - **Turn-based saving**: Saves every N conversation turns (configurable via `memory_save_interval_turns`)
   - **Idle timeout saving**: Saves after N seconds of inactivity (configurable via `memory_idle_timeout_seconds`)
   - The two rules are coordinated: when turn-based save triggers, the idle timer is cancelled to avoid duplicate saves
   - When a new user query arrives, the idle timer is cancelled and reset
6. **Shutdown**: Save final conversation state when agent stops (if there are unsaved conversations)

## Memory Management

### Memory Storage
- Conversation is saved based on two configurable rules when `enable_memorization` is `true`:
  - **Turn-based saving**: Saves every N conversation turns (default: 5 turns, configurable via `memory_save_interval_turns`)
  - **Idle timeout saving**: Saves after N seconds of inactivity (default: 30 seconds, configurable via `memory_idle_timeout_seconds`)
- Only saves user and assistant messages from LLM context (filters out system messages)
- Memory is saved asynchronously using `asyncio.create_task()` and won't block real-time interaction
- Also saves on agent shutdown to preserve final conversation state (if there are unsaved conversations)
- The two rules are coordinated to avoid duplicate saves:
  - When turn-based save triggers, the idle timer is cancelled
  - When a new user query arrives, the idle timer is cancelled and reset
  - Turn counter is updated immediately before creating the save task to prevent race conditions

### Memory Retrieval
- Uses semantic search to find relevant memories based on user queries
- Searches using `query` parameter for flexible query matching
- Retrieved memories are formatted and added to LLM context before processing user input
- Helps assistant provide more personalized and relevant responses

### Personalized Greeting
- On user join, retrieves user memory summary:
  - If `enable_user_memory` is `true`: Uses `UserMemory.client.profile()` to get user profile, falls back to semantic search with "User Profile" query if profile is not available
  - If `enable_user_memory` is `false`: Uses semantic search with "User Profile" query
- Generates a personalized greeting (2-3 sentences) based on retrieved memories using LLM with `PERSONALIZED_GREETING_TEMPLATE`
- Falls back to default greeting if no memories are found or generation fails (with 10-second timeout)
- Uses LLM to generate natural, conversational greetings that may adapt to user's language/region based on memory content

## Implementation Details

### Memory Store Classes

The extension uses an abstract `MemoryStore` interface with two implementations:

1. **`PowerMemSdkMemoryStore`**: Wraps PowerMem `Memory` client (used when `enable_user_memory` is `false`)
   - `add(conversation, user_id, agent_id)`: Save conversation to memory
   - `search(user_id, agent_id, query)`: Search for related memories using semantic search
   - `get_user_profile(user_id, agent_id)`: Get user profile via semantic search with "User Profile" query

2. **`PowerMemSdkUserMemoryStore`**: Wraps PowerMem `UserMemory` client (used when `enable_user_memory` is `true`)
   - `add(conversation, user_id, agent_id)`: Save conversation to memory
   - `search(user_id, agent_id, query)`: Search for related memories using semantic search
   - `get_user_profile(user_id, agent_id)`: Get user profile via `client.profile()`, falls back to semantic search if profile is not available

### Memory-related Methods

- `_generate_personalized_greeting()`: Generate personalized greeting based on user memories using LLM (with 10-second timeout)
- `_retrieve_related_memory(query)`: Retrieve related memory using semantic search and format as "Memorise:" list
- `_memorize_conversation()`: Save current conversation to PowerMem (reads from LLM context, filters user/assistant messages only)
- `_start_memory_idle_timer()`: Start/reset idle timer for automatic memory saving
- `_cancel_memory_idle_timer()`: Cancel the idle timer (called when new query arrives or turn-based save triggers)

## Error Handling

- If PowerMem client initialization fails, the system logs the error but continues running
- Memory operation failures are logged as errors without affecting main functionality
- Greeting generation failures fall back to default greeting
- Memory retrieval failures result in normal conversation without memory context

## Important Notes

1. PowerMem configuration is provided via `powermem_config` dictionary in the configuration file (not via `auto_config()`). The config supports `${env:VAR_NAME}` syntax for environment variable substitution
2. Ensure all required environment variables are set for PowerMem components:
   - Vector store: `OCEANBASE_COLLECTION`, `OCEANBASE_HOST`, `OCEANBASE_PORT`, `OCEANBASE_USER`, `OCEANBASE_PASSWORD`, `OCEANBASE_DATABASE`, `EMBEDDING_DIMS`
   - LLM: `LLM_PROVIDER`, `LLM_API_KEY`, `LLM_MODEL`
   - Embedder: `EMBEDDING_PROVIDER`, `EMBEDDING_API_KEY`, `EMBEDDING_MODEL`, `EMBEDDING_DIMS`
3. Memory functionality requires network connection if using remote LLM/embedding services
4. Conversation memory is saved asynchronously and won't block real-time interaction
5. Recommend setting different `user_id` for different users to isolate memory
6. Memory saving rules are configurable:
   - Adjust `memory_save_interval_turns` to control turn-based saving frequency (default: 5)
   - Adjust `memory_idle_timeout_seconds` to control idle timeout duration (default: 30.0)
   - Both rules work together to ensure memories are saved regularly, with coordination to avoid duplicate saves
7. When `enable_user_memory` is `true`, the extension uses `UserMemory` client which provides enhanced user profile functionality via `client.profile()` method
8. Memory is read from LLM context (`agent.llm_exec.get_context()`), so only messages that have been processed by the LLM are saved

## Troubleshooting

### Common Issues

1. **ImportError: No module named 'powermem'**
   - Solution: Install PowerMem SDK: `pip install powermem`

2. **PowerMem Initialization Failed**
   - Check if `powermem_config` is properly configured in the configuration file
   - Verify all required environment variables are set (see Important Notes section)
   - Check if `enable_memorization` is `true` and `powermem_config` is not empty
   - Verify database connection settings in `vector_store.config`
   - Check LLM and embedding API keys in `llm.config` and `embedder.config`
   - Review detailed error information in logs (includes full traceback)

3. **Memory Functionality Not Working**
   - Check if `enable_memorization` is set to `true` in configuration
   - Verify PowerMem client is properly initialized
   - Check if memory operations are being called (review logs)
   - Ensure database is accessible and properly configured

4. **Personalized Greeting Not Generated**
   - Check if user has any existing memories
   - Verify memory search/profile retrieval is working correctly (check logs for `get_user_profile` calls)
   - If using `enable_user_memory=true`, check if `client.profile()` is working or falling back to query-based search
   - Check LLM is accessible for greeting generation
   - Review timeout settings (default 10 seconds for greeting generation)
   - Check if `_is_generating_greeting` flag is properly reset (may cause issues if previous greeting generation didn't complete)