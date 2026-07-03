# gemini_llm2_python

A Google Gemini LLM2 extension for the TEN framework, providing integration with Google's Generative AI models.

## Features

- Integration with Google Gemini models (Gemini 3 Flash/Pro, etc.)
- Full compatibility with the TEN LLM2 interface
- Streaming and non-streaming responses
- Tool calling support
- Configurable temperature, top_p, and token limits

## API

Refer to the `api` definition in [manifest.json](manifest.json) and default values in [property.json](property.json).

| **Property** | **Type** | **Description** |
|---|---|---|
| `api_key` | `string` | API key for authenticating with Google Gemini |
| `model` | `string` | Model identifier (e.g., `gemini-1.5-pro-latest`) |
| `base_url` | `string` | OpenAI-compatible endpoint base (default `https://generativelanguage.googleapis.com/v1beta/openai`) |
| `temperature` | `float` | Sampling temperature, higher values mean more randomness |
| `top_p` | `float` | Nucleus sampling parameter |
| `max_tokens` | `int` | Maximum number of tokens to generate |
| `prompt` | `string` | System prompt for the model |

> **Note**: If your API key is tied to Vertex AI, override `base_url` with your OpenAI-compatible endpoint, e.g. `https://us-central1-aiplatform.googleapis.com/v1beta/projects/<PROJECT_ID>/locations/us-central1/publishers/google/openai`.

## Configuration

Set the `GEMINI_API_KEY` environment variable with your Google Gemini API key:

```bash
export GEMINI_API_KEY=your_api_key
```

## Usage

The extension uses Google's OpenAI-compatible endpoint at `https://generativelanguage.googleapis.com/v1beta/openai` to provide seamless integration with Gemini models.

### Gemini 3 Flash

- **Model IDs:** `gemini-3-flash` (default), `gemini-3-flash-preview` as a
  fallback.
- **Endpoint:** OpenAI-compatible
  `https://generativelanguage.googleapis.com/v1beta/openai/chat/completions`.
- **How to use:** Set `model` in `property.json` (or Ten App properties) to
  `gemini-3-flash` and keep the default `base_url`. No other code changes are
  required because this extension already speaks the OpenAI-compatible API.

## Code Style & Linting Rules

This extension follows the TEN framework's strict code quality standards. All code must comply with the following rules before being merged:

### Line Length
- **Maximum line length**: 80 characters
- Tools: Black formatter, Pylint
- Lines must be broken at logical points (variable assignments, function arguments, etc.)

### Type Hints
- **Required for**: All function parameters and return types
- **Preferred**: Use `Optional[T]` instead of `T | None` for backward compatibility
- **Example**:
  ```python
  from typing import Optional, AsyncGenerator

  async def get_chat_completions(
      self, request_input: LLMRequest
  ) -> AsyncGenerator[LLMResponse, None]:
      pass
  ```

### Naming Conventions
- **Classes**: PascalCase (e.g., `GeminiChatAPI`, `GeminiLLM2Config`)
- **Functions/Methods**: snake_case (e.g., `get_chat_completions`, `_convert_tools_to_dict`)
- **Variables**: snake_case (e.g., `api_key`, `max_tokens`)
- **Constants**: UPPER_CASE (e.g., `DEFAULT_TIMEOUT`)
- **Private methods**: Prefix with underscore (e.g., `_parse_message_content`)

### Imports
- Group imports in this order: stdlib, third-party, local imports
- Use explicit imports (avoid `import *`)
- Sort imports alphabetically within groups

### Exception Handling
- Use specific exception types (avoid bare `except:`)
- Include logging for all exceptions
- Example:
  ```python
  try:
      result = await self.http_client.post(...)
  except json.JSONDecodeError:
      self.ten_env.log_debug(f"Could not parse: {line}")
      continue
  except Exception as e:
      self.ten_env.log_error(f"Error: {e}")
      raise
  ```

### Logging
- Use `ten_env.log_info()` for important operations
- Use `ten_env.log_debug()` for detailed debugging information
- Use `ten_env.log_error()` for error conditions
- Always include context in log messages

### Code Formatting
- Use Black formatter with line length of 80 characters
- Run before committing:
  ```bash
  cd ai_agents
  task format
  task check
  ```

### Linting
- Use Pylint with the configuration in `tools/pylint/.pylintrc`
- Run before committing:
  ```bash
  cd ai_agents
  task lint-extension EXTENSION=gemini_llm2_python
  ```

### Disabled Pylint Checks
The following checks are disabled in this project:
- `C0114`: missing-module-docstring
- `C0115`: missing-class-docstring
- `C0116`: missing-function-docstring
- `W0718`: broad-exception-caught
- `W0621`: redefined-outer-name

### Complexity Limits
- **Max function arguments**: 5
- **Max class attributes**: 7
- **Max statements per function**: 50
- **Max local variables**: 15
- **Max branches per function**: 12
- **Max nested blocks**: 5

### Refactoring Long Lines
When breaking long lines, follow these patterns:

**Dictionary/List Access:**
```python
# Before (too long)
properties = json_dict["function"]["parameters"]["properties"]

# After (create intermediate variable)
parameters = json_dict["function"]["parameters"]
properties = parameters["properties"]
```

**Function Arguments:**
```python
# Before (too long)
yield LLMResponseMessageDelta(
    response_id=chunk_data.get("id", ""),
    role="assistant",
    content=content,
    created=chunk_data.get("created", 0),
)

# After (keep aligned)
yield LLMResponseMessageDelta(
    response_id=chunk_data.get("id", ""),
    role="assistant",
    content=content,
    created=chunk_data.get("created", 0),
)
```

**String Concatenation:**
```python
# Before (too long)
error_msg = f"Gemini API error: {response.status_code} - {error_text}{extra_hint}"

# After (break into multiple f-strings)
error_msg = (
    f"Gemini API error: {response.status_code} - "
    f"{error_text}{extra_hint}"
)
```

### Pre-commit Checklist
Before pushing your code:
- [ ] All lines are ≤ 80 characters
- [ ] Run `task format` (from ai_agents directory)
- [ ] Run `task check` (format check)
- [ ] Run `task lint-extension EXTENSION=gemini_llm2_python`
- [ ] All type hints are present
- [ ] Naming conventions followed
- [ ] Exception handling with proper logging
- [ ] No disabled rules violated

### Example: Proper Code Structure
See [gemini.py](gemini.py) and [extension.py](extension.py) for examples of properly formatted code that follows all these rules.
