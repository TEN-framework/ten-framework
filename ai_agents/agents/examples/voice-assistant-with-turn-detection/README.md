# Voice Assistant with Turn Detection

A voice assistant enhanced with AI-powered turn detection using a fine-tuned LLM model deployed on Cerebrium GPUs. Unlike traditional Voice Activity Detection (VAD) which only detects when speech starts/stops, turn detection intelligently determines when a speaker has finished their conversational turn by understanding context and intent.

## What is Turn Detection?

**Turn Detection** analyzes speech transcription in real-time to determine if the speaker has finished their thought (turn complete) or is pausing mid-sentence (turn incomplete). This enables:

- **Natural conversation flow** - The assistant waits for complete thoughts before responding
- **Better interruption handling** - Distinguishes between pauses and completion
- **Context-aware decisions** - Uses LLM reasoning rather than simple audio thresholds

## Prerequisites

### 1. Cerebrium Account Setup

The turn detection model requires GPU deployment on Cerebrium:

1. **Create Cerebrium Account**: Sign up at [Cerebrium](https://www.cerebrium.ai/)
2. **Install Cerebrium CLI**:
   ```bash
   pip install cerebrium
   ```

3. **Login to Cerebrium**:
   ```bash
   cerebrium login
   ```

4. **Deploy the Turn Detection Model**:
   ```bash
   cd agents/examples/voice-assistant-with-turn-detection/cerebrium
   cerebrium deploy
   ```

   This will:
   - Load the `TEN-framework/TEN_Turn_Detection` model with vLLM
   - Deploy to NVIDIA A10 GPU (2 CPU cores, 14GB memory)
   - Create an OpenAI-compatible API endpoint
   - Return your deployment URL and API key

5. **Get Your Credentials**:
   After deployment, Cerebrium provides:
   - **Base URL**: `https://api.cortex.cerebrium.ai/v4/p-xxxxx/ten-turn-detection-project/run`
   - **API Key**: Your Cerebrium API token

   **Important**: The base URL must end with `/run` for OpenAI client compatibility.

6. **Verify Your Deployment**:
   Test that everything is working properly using the included test script:
   ```bash
   cd agents/examples/voice-assistant-with-turn-detection/cerebrium

   # Export your Cerebrium credentials
   export TTD_BASE_URL="https://api.cortex.cerebrium.ai/v4/p-xxxxx/ten-turn-detection-project/run"
   export TTD_API_KEY="your_cerebrium_api_key"

   # Run the test script
   python test.py
   ```

   The test will verify your deployment by sending sample turn detection requests and showing response times.

### 2. Required Environment Variables

Set these in your `.env` file:

```bash
# Agora (required for audio streaming)
AGORA_APP_ID=your_agora_app_id_here
AGORA_APP_CERTIFICATE=your_agora_certificate_here  # optional

# Deepgram (required for STT)
DEEPGRAM_API_KEY=your_deepgram_api_key_here

# OpenAI (required for LLM)
OPENAI_API_KEY=your_openai_api_key_here
OPENAI_MODEL=gpt-4o-mini  # or gpt-4o, gpt-3.5-turbo

# ElevenLabs (required for TTS)
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key_here

# Turn Detection (required - from Cerebrium deployment)
TTD_BASE_URL=https://api.cortex.cerebrium.ai/v4/p-xxxxx/ten-turn-detection-project/run
TTD_API_KEY=your_cerebrium_api_key_here

# Optional
WEATHERAPI_API_KEY=your_weather_api_key_here  # for weather tool
```

## Setup and Running

> **Note**: Make sure you've completed the [Cerebrium deployment](#1-cerebrium-account-setup) from the Prerequisites section before proceeding.

### 1. Install Voice Assistant Dependencies

```bash
cd agents/examples/voice-assistant-with-turn-detection
task install
```

### 2. Run the Voice Assistant

```bash
task run
```

### 3. Access the Application

- **Frontend**: http://localhost:3000
- **API Server**: http://localhost:8080
- **TMAN Designer**: http://localhost:49483

## How Turn Detection Works

1. **Speech Input**: User speaks → Deepgram STT transcribes in real-time
2. **Turn Analysis**: Each transcription chunk is sent to the turn detection model
3. **Classification**: The model returns one of three states:
   - `finished` - Turn is complete, send to LLM
   - `unfinished` - Continue listening, user still speaking
   - `wait` - Wait for clarification or timeout
4. **Response**: When `finished`, text is sent to OpenAI LLM → ElevenLabs TTS → User

### Turn Detection States

| State | Description | Action |
|-------|-------------|--------|
| `finished` | Speaker has completed their thought | Send transcription to LLM for response |
| `unfinished` | Speaker is mid-sentence or pausing | Continue collecting transcription |
| `wait` | Ambiguous state, waiting for more input | Hold briefly, then timeout |

## Customization

The voice assistant uses a modular design. Access the visual designer at http://localhost:49483 to:
- Replace STT provider (Deepgram → Azure, Speechmatics, AssemblyAI, etc.)
- Change LLM (OpenAI → Claude, Llama, Coze, etc.)
- Swap TTS (ElevenLabs → Azure, Cartesia, Fish Audio, etc.)
- Adjust turn detection sensitivity

For detailed usage, see [TMAN Designer documentation](https://theten.ai/docs/ten_agent/customize_agent/tman-designer).

## Docker Deployment

**Note**: Execute outside of any Docker container.

### Build Image

```bash
cd ai_agents
docker build -f agents/examples/voice-assistant-with-turn-detection/Dockerfile -t voice-assistant-turn-detection .
```

### Run

```bash
docker run --rm -it --env-file .env -p 8080:8080 -p 3000:3000 voice-assistant-turn-detection
```

### Access

- Frontend: http://localhost:3000
- API Server: http://localhost:8080
- TMAN Designer: http://localhost:49483

## Learn More

- [Cerebrium Documentation](https://docs.cerebrium.ai/)
- [TEN Turn Detection Model](https://huggingface.co/TEN-framework/TEN_Turn_Detection)
- [vLLM Documentation](https://docs.vllm.ai/)
- [TEN Framework Documentation](https://theten.ai/docs)
- [Agora RTC Documentation](https://docs.agora.io/en/voice-calling/overview/product-overview)
- [Deepgram API Documentation](https://developers.deepgram.com/)
- [ElevenLabs API Documentation](https://docs.elevenlabs.io/)
