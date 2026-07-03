# Blaze TTS Extension for TEN Framework

Blaze Text-to-Speech (TTS) extension for [TEN Framework](https://github.com/TEN-framework/ten-framework).

## Installation

```bash
pip install -r requirements.txt
```

Or install dependencies directly:

```bash
pip install httpx pydantic
```

## Configuration

### Environment Variables

Set the following environment variables:

```bash
export BLAZE_TTS_API_URL="http://localhost:8000"
export BLAZE_TTS_API_KEY="your-api-key-here"  # Optional
```

### Property.json (TEN Framework)

The extension includes a `property.json` file with default configuration that TEN framework can use:

```json
{
    "params": {
        "api_url": "${env:BLAZE_TTS_API_URL}",
        "api_key": "${env:BLAZE_TTS_API_KEY}",
        "language": "vi",
        "speaker_id": null,
        "audio_speed": 1.0,
        "audio_quality": 64,
        "timeout": 3600
    }
}
```

TEN framework will automatically read this file and use environment variables for configuration.

## Usage

### As TEN Framework Extension

```python
from blaze_tts_python import BlazeTTSExtension

# Initialize extension (can accept dict config from TEN framework)
tts = BlazeTTSExtension(config={
    "api_url": "http://localhost:8000",
    "api_key": "your-api-key",
    "speaker_id": "speaker-123",
})

# Synthesize text using TEN framework interface
result = tts.process({
    "text": "Xin chào",
    "speaker_id": "speaker-123",
    "language": "vi",
})

audio_bytes = result.get("audio_data")

# Get extension metadata
metadata = tts.get_metadata()
print(metadata)
```

### As Direct Extension

```python
from blaze_tts_python import BlazeTTSExtension, BlazeTTSConfig

# Initialize extension
config = BlazeTTSConfig(
    api_url="http://localhost:8000",
    api_key="your-api-key",
    default_language="vi",
)
tts = BlazeTTSExtension(config=config)

# Synthesize text
result = tts.synthesize(
    text="Xin chào",
    speaker_id="speaker-123",
    language="vi",
)

# Download audio
job_id = result["id"]
audio_bytes = tts.download_audio(job_id)
```

## API Reference

### BlazeTTSExtension

**TEN Framework Interface Methods:**
- `process(input_data)` - Process text and return audio (TEN framework interface)
- `get_metadata()` - Get extension metadata (TEN framework interface)

**Direct Methods:**

- `synthesize(text, speaker_id, language, audio_speed, audio_quality, ...)` - Synthesize text to speech
- `get_speakers()` - Get list of available speakers
- `download_audio(job_id, output_path)` - Download generated audio
- `get_job_info(job_id)` - Get TTS job information

## Supported Formats

- `wav` - WAV format
- `mp3` - MP3 format
- `ogg` - OGG format

## License

This extension is provided as-is for use with the TEN Framework and Blaze services.

