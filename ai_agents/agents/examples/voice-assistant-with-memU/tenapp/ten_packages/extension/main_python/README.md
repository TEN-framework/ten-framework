# Voice Assistant with Memory Integration

This extension integrates MemU memory functionality, enabling the voice assistant to remember previous conversation content and provide more personalized and coherent interaction experiences.

## Features

1. **Conversation Memory**: Automatically records user and assistant conversation content
2. **Context Loading**: Loads historical conversation summaries from MemU on startup
3. **Smart Memory**: Automatically saves to MemU at the end of each conversation round
4. **Configurable**: Supports enabling/disabling memory functionality through configuration

## Installation

```bash
pip install -r requirements.txt
```

## Environment Configuration

Set the MemU API key:

```bash
export MEMU_API_KEY="your_memu_api_key_here"
```

## Configuration Options

The following parameters can be set in the configuration file:

```json
{
  "greeting": "Hello, I am your AI assistant.",
  "agent_id": "voice_assistant_agent",
  "agent_name": "Voice Assistant with Memory",
  "user_id": "default_user",
  "user_name": "User"
}
```

### Configuration Description

- `greeting`: Welcome message when user joins
- `agent_id`: Unique identifier for the agent
- `agent_name`: Display name for the agent
- `user_id`: Unique identifier for the user
- `user_name`: Display name for the user

## Workflow

1. **Initialization**: Initialize MemU client and load historical memory on startup
2. **Conversation Processing**: Real-time recording of user input and assistant responses
3. **Memory Saving**: Automatically save to MemU at the end of each conversation round
4. **Context Synchronization**: Keep LLM context synchronized with conversation records

## Memory Management

### Automatic Memory
- When `LLMResponseEvent.is_final` is `true`, it indicates the end of a conversation round
- The system automatically saves the complete conversation context to MemU
- Filters out system messages, only saving user and assistant conversation content

### Memory Retrieval
- Retrieves user's historical conversation summaries from MemU on startup
- Adds summaries as system messages to LLM context
- Helps assistant provide more personalized and relevant responses

## API Extensions

### LLMExec Context Management

The following methods are available in the `LLMExec` class:

- `get_context()`: Get current conversation context
- `clear_context()`: Clear current conversation context
- `write_context(ten_env, role, content)`: Append or merge a message into context without constructing `LLMMessage` externally

### Memory-related Methods

- `_initialize_memory_client()`: Initialize MemU client
- `_retrieve_memory()`: Retrieve memory from MemU
- `_memorize_conversation()`: Save conversation to MemU
- `_load_memory_to_context()`: Load memory to LLM context
- `_update_llm_context()`: Update LLM context

## Error Handling

- If MemU client initialization fails, the system logs the error but continues running
- Memory operation failures are logged as errors without affecting main functionality

## Important Notes

1. Ensure the correct `MEMU_API_KEY` environment variable is set
2. Memory functionality requires network connection to access MemU API
3. Conversation memory is saved asynchronously and won't block real-time interaction
4. Recommend setting different `user_id` for different users to isolate memory

## Troubleshooting

### Common Issues

1. **ImportError: No module named 'memu'**
   - Solution: Install MemU SDK: `pip install memu-sdk-python`

2. **MemU API Connection Failed**
   - Check if `MEMU_API_KEY` is set correctly
   - Confirm network connection is working
   - Check detailed error information in logs

3. **Memory Functionality Not Working**
   - Check if MemU client is properly initialized
   - Review relevant log information