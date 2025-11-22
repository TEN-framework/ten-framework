# gemini_llm2_python

A Google Gemini LLM2 extension for the TEN framework, providing integration with Google's Generative AI models.

## Features

- Integration with Google Gemini models (Gemini 3 Pro, etc.)
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
