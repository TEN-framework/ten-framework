# Who Likes What – Speaker Diarization Demo

This example demonstrates TEN Framework's speaker diarization capabilities using Speechmatics ASR in a conversational game called **Who Likes What**, where the agent figures out “who said what” across multiple voices.

## Features

- **Real-time speaker identification**: Automatically detects and labels different speakers (S1, S2, S3, etc.)
- **Configurable sensitivity**: Adjust how aggressively the system detects new speakers
- **Multi-speaker conversations**: Supports up to 100 speakers (configurable) and powers the Who Likes What game loop
- **Visual speaker labels**: Speaker information is displayed in the transcript UI so the agent can call players by name

## Prerequisites

1. **Speechmatics API Key**: Get one from [Speechmatics](https://www.speechmatics.com/)
2. **OpenAI API Key**: For the LLM responses
3. **ElevenLabs API Key**: For text-to-speech
4. **Agora credentials**: For real-time audio streaming

## Setup

### 1. Set Environment Variables

Add to your `.env` file:

```bash
# Speechmatics (required for diarization)
SPEECHMATICS_API_KEY=your_speechmatics_api_key_here

# OpenAI (for LLM)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o

# ElevenLabs (for TTS)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here

# Agora (for RTC)
AGORA_APP_ID=your_agora_app_id_here
AGORA_APP_CERTIFICATE=your_agora_certificate_here
```

### 2. Install Dependencies

```bash
cd agents/examples/speechmatics-diarization
task install
```

This command will:
- Install required dependencies
- Configure the agent for speaker diarization
- Set up the graph with Speechmatics ASR

### 3. Run the Agent

```bash
cd agents/examples/speechmatics-diarization
task run
```

The agent will start with speaker diarization enabled.

4. **Access the application:**
   - Frontend: http://localhost:3000
   - API Server: http://localhost:8080
   - TMAN Designer: http://localhost:49483

## Configuration

You can customize diarization settings in `property.json`:

```json
{
  "params": {
    "key": "${env:SPEECHMATICS_API_KEY}",
    "language": "en",
    "sample_rate": 16000,
    "diarization": "speaker",
    "speaker_sensitivity": 0.5,
    "max_speakers": 10,
    "prefer_current_speaker": false
  }
}
```

### Diarization Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `diarization` | string | `"none"` | Diarization mode: `"none"`, `"speaker"`, `"channel"`, or `"channel_and_speaker"` |
| `max_speakers` | int | `50` | Maximum number of speakers (2-100) |
| `speaker_sensitivity` | float | `0.5` | Range 0-1. Higher values detect more unique speakers (⚠️ Not supported in current version) |
| `prefer_current_speaker` | bool | `false` | Reduce false speaker switches between similar voices (⚠️ Not supported in current version) |

**Note**: The current implementation uses `speechmatics-python==3.0.2`, which has limited diarization configuration support. Only `max_speakers` is functional. `speaker_sensitivity` and `prefer_current_speaker` are available in newer Speechmatics API versions.

## How It Works

1. **Audio Input**: User speaks through the microphone
2. **Speechmatics ASR**: Transcribes audio AND identifies speakers
3. **Speaker Labels**: Each transcription includes speaker labels like `[S1]`, `[S2]`
4. **LLM Context**: Speaker information is passed to the LLM
5. **Response**: The agent responds, acknowledging different speakers

## Example Interaction

**Elliot**: "Hello, this is Elliot."

**Transcript**: "[Elliot] Hello, this is Elliot."

**Musk**: "This is Elon."

**Transcript**: "[Musk] This is Elon."

**Agent**: "Elliot's voice is locked in. Waiting for Taytay to give me a quick hello so I can lock in their voice."

## Troubleshooting

### No speaker labels appearing

- Verify `SPEECHMATICS_API_KEY` is set correctly
- Check that `diarization` is set to `"speaker"` in property.json
- Ensure multiple people are speaking (single speaker might always be labeled S1)

### Too many false speaker switches

- Note: `prefer_current_speaker` and `speaker_sensitivity` are not supported in the current version
- Consider adjusting `max_speakers` to limit the number of detected speakers

### Not enough speakers detected

- Increase `max_speakers` if you expect more than the default number of speakers

## UI Customization

The playground UI automatically displays speaker labels in the transcript. To further customize the display, you can modify the `main_python` extension's `_on_asr_result` method in `extension.py`.

---

## Release as Docker image

**Note**: The following commands need to be executed outside of any Docker container.

### Build image
```bash
# Run at project root
cd ai_agents
docker build -f agents/examples/speechmatics-diarization/Dockerfile -t speechmatics-diarization-app .
```

### Run container
```bash
# Use local .env (optional)
docker run --rm -it \
  --env-file .env \
  -p 8080:8080 \
  -p 3000:3000 \
  speechmatics-diarization-app
```

### Access
- Frontend: http://localhost:3000
- API Server: http://localhost:8080

## Learn More

- [Speechmatics Diarization Docs](https://docs.speechmatics.com/speech-to-text/features/diarization)
- [TEN Framework Documentation](https://doc.theten.ai)
- [Voice Assistant Example](../voice-assistant/) for the base architecture

## License

Apache License 2.0
