# OpenAI GPT Image 1.5 Extension

A TEN Framework extension for generating images using OpenAI's GPT Image 1.5 model with automatic fallback to DALL-E 3.

## Features

- **GPT Image 1.5 Support**: Uses OpenAI's latest and fastest image generation model
- **Automatic Fallback**: Falls back to DALL-E 3 if GPT Image 1.5 is unavailable
- **LLM Tool Integration**: Works as a callable tool within conversational flows
- **Kid-Friendly Error Messages**: Provides gentle, appropriate error responses
- **Azure OpenAI Support**: Compatible with Azure OpenAI endpoints
- **Quality Control**: Supports both standard and HD quality generation
- **Flexible Configuration**: Environment variable-based configuration

## Installation

This extension is part of the TEN Framework. It will be automatically installed when you run:

```bash
cd your_agent/tenapp
tman install
```

## Configuration

### Environment Variables

```bash
# Required
OPENAI_API_KEY=sk-your_openai_key_here

# Optional
OPENAI_IMAGE_BASE_URL=https://api.openai.com/v1  # Custom endpoint
AZURE_OPENAI_IMAGE_ENDPOINT=https://your.openai.azure.com  # For Azure
AZURE_OPENAI_IMAGE_API_VERSION=2024-02-01  # For Azure
```

### Property Configuration

Add to your agent's `property.json`:

```json
{
  "type": "extension",
  "name": "image_gen_tool",
  "addon": "openai_gpt_image_python",
  "property": {
    "params": {
      "api_key": "${env:OPENAI_API_KEY}",
      "model": "gpt-image-1.5",
      "size": "1024x1024",
      "quality": "standard",
      "fallback_model": "dall-e-3"
    }
  }
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `params.api_key` | string | - | OpenAI API key (required) |
| `params.model` | string | `gpt-image-1.5` | Image model to use |
| `params.size` | string | `1024x1024` | Image dimensions (1024x1024, 1792x1024, 1024x1792) |
| `params.quality` | string | `standard` | Image quality (standard, hd) |
| `params.fallback_model` | string | `dall-e-3` | Fallback model if primary unavailable |
| `params.vendor` | string | `openai` | API vendor (openai, azure) |
| `params.base_url` | string | - | Custom API base URL (optional) |
| `dump` | boolean | `false` | Enable response logging for debugging |
| `dump_path` | string | `./openai_image_responses.json` | Path for debug logs |

## Usage

### As an LLM Tool

The extension registers as a tool that LLMs can call during conversations:

```python
# The LLM will automatically call this when users request images
# Example user input: "Create a purple dragon!"
# The LLM calls: generate_image(prompt="A majestic purple dragon...")
```

### Tool Metadata

- **Tool Name**: `generate_image`
- **Parameters**:
  - `prompt` (required): Detailed image description
  - `quality` (optional): Override quality setting (`standard` or `hd`)

### Graph Integration

Connect the extension in your agent graph:

```json
{
  "connections": [
    {
      "extension": "llm",
      "cmd": [
        {
          "name": "tool_register",
          "dest": [{"extension": "image_gen_tool"}]
        }
      ]
    },
    {
      "extension": "image_gen_tool",
      "data": [
        {
          "name": "content_data",
          "dest": [{"extension": "main_control"}]
        }
      ]
    }
  ]
}
```

## Output Format

Generated images are sent as `content_data` messages with the following JSON structure:

```json
{
  "data": {
    "image_url": "https://oaidalleapiprodscus.blob.core.windows.net/..."
  },
  "type": "image_url"
}
```

## Error Handling

The extension provides user-friendly error messages:

- **Content Policy Violation**: "I can't create that image. Let's try something different!"
- **Invalid API Key**: "API key is invalid. Please check your configuration."
- **Model Not Found**: Automatically falls back to DALL-E 3
- **Rate Limit**: "Rate limit exceeded. Please try again later."
- **Generic Error**: "Something went wrong. Please try again."

## Model Support

### Supported Models

- `gpt-image-1.5` (Primary) - Latest, fastest, 4x speed improvement
- `dall-e-3` (Fallback) - Previous generation, proven reliability
- `dall-e-2` (Legacy) - Older model, basic functionality

### Image Sizes

- `1024x1024` - Square (default)
- `1792x1024` - Landscape
- `1024x1792` - Portrait

### Quality Modes

- `standard` - Fast generation, good quality (default)
- `hd` - High detail, slower generation, higher cost

## Development

### Running Tests

```bash
cd ten_packages/extension/openai_gpt_image_python
python -m pytest tests/
```

### Debug Mode

Enable debug logging by setting `dump: true` in your configuration:

```json
{
  "dump": true,
  "dump_path": "./debug_images.json"
}
```

This will save all requests and responses to the specified file.

## Troubleshooting

### Image Generation Fails

1. **Check API Key**: Ensure `OPENAI_API_KEY` is set correctly
2. **Verify Model Access**: GPT Image 1.5 requires API access
3. **Check Prompt**: Ensure prompt doesn't violate content policies
4. **Review Logs**: Check TEN runtime logs for detailed error messages

### Azure OpenAI Setup

For Azure OpenAI, configure:

```json
{
  "params": {
    "api_key": "${env:AZURE_OPENAI_KEY}",
    "vendor": "azure",
    "azure_endpoint": "${env:AZURE_OPENAI_IMAGE_ENDPOINT}",
    "azure_api_version": "2024-02-01"
  }
}
```

## Examples

See the [doodler](../../../examples/doodler/) example for a complete implementation.

## License

This extension is part of the TEN Framework, licensed under the Apache License, Version 2.0.

## Learn More

- [OpenAI Images API Documentation](https://platform.openai.com/docs/guides/image-generation)
- [TEN Framework Documentation](https://doc.theten.ai)
- [GPT Image 1.5 Announcement](https://openai.com/index/new-chatgpt-images-is-here/)
