# Voice Assistant with PowerMem Integration

This extension integrates PowerMem memory functionality, enabling the voice assistant to remember previous conversation content and provide more personalized and coherent interaction experiences.

## Features

1. **Conversation Memory**: Automatically records user and assistant conversation content
2. **Semantic Search**: Retrieves relevant memories based on user queries using semantic search
3. **Smart Memory**: Automatically saves to PowerMem every 2 conversation rounds
4. **Personalized Greeting**: Generates personalized greetings based on user memories when user joins
5. **Configurable**: Supports enabling/disabling memory functionality through configuration

## Installation

```bash
pip install -r requirements.txt
```

The main dependency is:
- `powermem`: PowerMem SDK for memory management

## Environment Configuration

PowerMem uses automatic configuration via environment variables. Configure PowerMem by setting the appropriate environment variables. See the main example README for detailed PowerMem configuration options.

Key required configurations:
- **Database**: Choose provider (sqlite, oceanbase, postgres) and configure accordingly
- **LLM**: Choose provider (qwen, openai, mock) and set API keys
- **Embedding**: Choose provider (qwen, openai, mock) and set API keys

Example minimal configuration:
```bash
# Database (SQLite for development)
DATABASE_PROVIDER=sqlite
DATABASE_PATH=./data/powermem_dev.db

# LLM (Qwen)
LLM_PROVIDER=qwen
LLM_API_KEY=your_api_key_here
LLM_MODEL=qwen-plus

# Embedding (Qwen)
EMBEDDING_PROVIDER=qwen
EMBEDDING_API_KEY=your_api_key_here
EMBEDDING_MODEL=text-embedding-v4
```

## Configuration Options

The following parameters can be set in the configuration file:

```json
{
  "greeting": "Hello! I'm your AI assistant with memory. I can remember our previous conversations to provide more personalized help.",
  "agent_id": "voice_assistant_agent",
  "user_id": "user",
  "enable_memorization": true
}
```

### Configuration Description

- `greeting`: Default welcome message when user joins (will be replaced with personalized greeting if memories exist)
- `agent_id`: Unique identifier for the agent (used to isolate memories per agent)
- `user_id`: Unique identifier for the user (used to isolate memories per user)
- `enable_memorization`: Enable or disable memory functionality (default: `false`)

## Workflow

1. **Initialization**: Initialize PowerMem client using `auto_config()` on startup
2. **User Joins**: Generate personalized greeting based on user memories (if enabled)
3. **Conversation Processing**: Real-time recording of user input and assistant responses
4. **Memory Retrieval**: When user sends a query, search for related memories and add to LLM context
5. **Memory Saving**: Automatically save conversation to PowerMem every 2 rounds (if enabled)
6. **Shutdown**: Save final conversation state when agent stops

## Memory Management

### Memory Storage
- Conversation is saved every 2 rounds (when turn_id is even) when `enable_memorization` is `true`
- Only saves user and assistant messages (filters out system messages)
- Memory is saved asynchronously and won't block real-time interaction
- Also saves on agent shutdown to preserve final conversation state

### Memory Retrieval
- Uses semantic search to find relevant memories based on user queries
- Searches using `category_query` parameter for flexible query matching
- Retrieved memories are formatted and added to LLM context before processing user input
- Helps assistant provide more personalized and relevant responses

### Personalized Greeting
- On user join, retrieves user memory summary using a general query
- Generates a personalized greeting (2-3 sentences) based on retrieved memories
- Falls back to default greeting if no memories are found or generation fails
- Uses LLM to generate natural, conversational greetings

## Implementation Details

### Memory Store Class

The extension uses `PowerMemSdkMemoryStore` class which wraps the PowerMem SDK:

- `add(conversation, user_id, agent_id)`: Save conversation to memory
- `search(user_id, agent_id, category_query)`: Search for related memories using semantic search

### Memory-related Methods

- `_generate_personalized_greeting()`: Generate personalized greeting based on user memories
- `_retrieve_related_memory(query)`: Retrieve related memory using semantic search
- `_memorize_conversation()`: Save current conversation to PowerMem

## Error Handling

- If PowerMem client initialization fails, the system logs the error but continues running
- Memory operation failures are logged as errors without affecting main functionality
- Greeting generation failures fall back to default greeting
- Memory retrieval failures result in normal conversation without memory context

## Important Notes

1. Ensure PowerMem environment variables are properly configured (database, LLM, embedding)
2. Memory functionality requires network connection if using remote LLM/embedding services
3. Conversation memory is saved asynchronously and won't block real-time interaction
4. Recommend setting different `user_id` for different users to isolate memory
5. Memory is saved every 2 rounds to balance freshness and performance
6. PowerMem uses `auto_config()` which reads configuration from environment variables

## Troubleshooting

### Common Issues

1. **ImportError: No module named 'powermem'**
   - Solution: Install PowerMem SDK: `pip install powermem`

2. **PowerMem Initialization Failed**
   - Check if required environment variables are set (DATABASE_PROVIDER, LLM_PROVIDER, EMBEDDING_PROVIDER)
   - Verify database connection settings
   - Check LLM and embedding API keys
   - Review detailed error information in logs

3. **Memory Functionality Not Working**
   - Check if `enable_memorization` is set to `true` in configuration
   - Verify PowerMem client is properly initialized
   - Check if memory operations are being called (review logs)
   - Ensure database is accessible and properly configured

4. **Personalized Greeting Not Generated**
   - Check if user has any existing memories
   - Verify memory search is working correctly
   - Check LLM is accessible for greeting generation
   - Review timeout settings (default 10 seconds)