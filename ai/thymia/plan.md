# Thymia Integration Plan

## Overview

Create a TEN Framework extension that integrates Thymia's mental wellness API to analyze user speech patterns and provide wellness metrics (distress, stress, burnout, fatigue, self-esteem) to the LLM for contextual conversation.

**Branch**: `feat/thymia-agora`

---

## Goals

1. **Capture and analyze speech**: Buffer 22+ seconds of actual speech (volume > threshold) from user audio
2. **Convert audio format**: Transform PCM frames to WAV format required by Thymia API
3. **Upload to Thymia**: Send audio segments to Thymia API for analysis
4. **Continuous analysis**: Support multiple audio uploads per session as conversation progresses
5. **Inject results**: Provide wellness metrics to LLM via tool/function call pattern

---

## Architecture

### Data Flow

```
User Speech
    ↓
agora_rtc (PCM audio frames)
    ↓
streamid_adapter
    ↓         ↓
    ↓         STT (transcription) → main_control → LLM
    ↓
thymia_analyzer (wellness analysis)
    ↓
    → main_control (wellness context)
```

### Component Responsibilities

**thymia_analyzer extension**:
- Receives PCM audio frames
- Buffers audio with voice activity detection
- Converts PCM to WAV format
- Uploads to Thymia API when threshold reached (22s of speech)
- Polls for analysis results
- Registers as LLM tool for get_wellness_metrics function calls

**main_control**:
- Receives wellness data from thymia_analyzer
- Injects wellness context into LLM prompts
- Can be queried by LLM for current wellness state

**LLM**:
- Receives enriched context with wellness metrics
- Can adjust conversation tone/content based on user's mental state

---

## Extension Design: thymia_analyzer_python

### File Structure

```
ten_packages/extension/thymia_analyzer_python/
├── addon.py                    # Extension registration
├── extension.py                # Main extension logic
├── audio_buffer.py            # Audio buffering and VAD
├── wav_converter.py           # PCM to WAV conversion
├── thymia_client.py           # API client
├── manifest.json              # Extension manifest
├── property.json              # Default properties
├── requirements.txt           # Dependencies
└── README.md                  # Documentation
```

### Core Components

#### 1. Audio Buffering (`audio_buffer.py`)

```python
class AudioBuffer:
    """Buffer PCM audio frames with voice activity detection"""

    def __init__(self, sample_rate, channels, silence_threshold):
        self.sample_rate = sample_rate
        self.channels = channels
        self.silence_threshold = silence_threshold
        self.speech_buffer = []
        self.speech_duration = 0.0

    def add_frame(self, pcm_data):
        """Add PCM frame and check if it contains speech"""
        # Calculate RMS volume
        volume = calculate_rms(pcm_data)

        if volume > self.silence_threshold:
            self.speech_buffer.append(pcm_data)
            self.speech_duration += len(pcm_data) / (self.sample_rate * self.channels * 2)

        return self.speech_duration

    def get_buffer(self):
        """Get buffered speech data as single PCM blob"""
        return b''.join(self.speech_buffer)

    def clear(self):
        """Clear buffer after upload"""
        self.speech_buffer = []
        self.speech_duration = 0.0

    def has_enough_speech(self, min_duration=22.0):
        """Check if we have enough speech for analysis"""
        return self.speech_duration >= min_duration
```

#### 2. WAV Converter (`wav_converter.py`)

```python
import struct

class WavConverter:
    """Convert PCM audio to WAV format"""

    @staticmethod
    def pcm_to_wav(pcm_data, sample_rate, channels, bits_per_sample=16):
        """
        Convert raw PCM data to WAV format

        Args:
            pcm_data: Raw PCM bytes
            sample_rate: Sample rate (e.g., 16000)
            channels: Number of channels (1 for mono, 2 for stereo)
            bits_per_sample: Bits per sample (typically 16)

        Returns:
            WAV file bytes
        """
        data_size = len(pcm_data)

        # Build WAV header
        header = struct.pack(
            '<4sI4s4sIHHIIHH4sI',
            b'RIFF',                                          # Chunk ID
            36 + data_size,                                   # Chunk size
            b'WAVE',                                          # Format
            b'fmt ',                                          # Subchunk1 ID
            16,                                               # Subchunk1 size
            1,                                                # Audio format (PCM)
            channels,                                         # Number of channels
            sample_rate,                                      # Sample rate
            sample_rate * channels * bits_per_sample // 8,  # Byte rate
            channels * bits_per_sample // 8,                # Block align
            bits_per_sample,                                 # Bits per sample
            b'data',                                          # Subchunk2 ID
            data_size,                                        # Subchunk2 size
        )

        return header + pcm_data
```

#### 3. Thymia API Client (`thymia_client.py`)

```python
import aiohttp
import asyncio
import json
from dataclasses import dataclass
from typing import Optional

@dataclass
class ThymiaConfig:
    api_key: str
    api_base_url: str = "https://api.thymia.ai"
    user_label: str = ""
    date_of_birth: str = ""
    birth_sex: str = ""
    language: str = "en-GB"

@dataclass
class WellnessResults:
    distress: float
    stress: float
    burnout: float
    fatigue: float
    low_self_esteem: float
    transcript: str
    status: str

class ThymiaClient:
    """Async client for Thymia API"""

    def __init__(self, config: ThymiaConfig):
        self.config = config
        self.session: Optional[aiohttp.ClientSession] = None

    async def create_session(self) -> tuple[str, str]:
        """
        Create a new mental wellness analysis session

        Returns:
            (model_run_id, upload_url)
        """
        url = f"{self.config.api_base_url}/v1/models/mental-wellness"

        payload = {
            "user": {
                "userLabel": self.config.user_label,
                "dateOfBirth": self.config.date_of_birth,
                "birthSex": self.config.birth_sex
            },
            "language": self.config.language
        }

        headers = {
            "x-api-key": self.config.api_key,
            "Content-Type": "application/json"
        }

        async with self.session.post(url, json=payload, headers=headers) as resp:
            if resp.status != 200:
                raise Exception(f"Failed to create session: {resp.status}")

            data = await resp.json()
            return data['id'], data['recordingUploadUrl']

    async def upload_audio(self, upload_url: str, wav_data: bytes):
        """Upload WAV audio to Thymia"""
        headers = {"Content-Type": "audio/wav"}

        async with self.session.put(upload_url, data=wav_data, headers=headers) as resp:
            if resp.status not in (200, 204):
                raise Exception(f"Failed to upload audio: {resp.status}")

    async def get_results(self, model_run_id: str, max_attempts=60, poll_interval=2) -> WellnessResults:
        """
        Poll for analysis results

        Args:
            model_run_id: ID from create_session
            max_attempts: Maximum polling attempts
            poll_interval: Seconds between polls

        Returns:
            WellnessResults object
        """
        url = f"{self.config.api_base_url}/v1/models/mental-wellness/{model_run_id}"
        headers = {"x-api-key": self.config.api_key}

        for attempt in range(max_attempts):
            async with self.session.get(url, headers=headers) as resp:
                if resp.status != 200:
                    raise Exception(f"Failed to get results: {resp.status}")

                data = await resp.json()
                status = data['status']

                if status in ('COMPLETE_OK', 'COMPLETE_ERROR', 'FAILED'):
                    # Extract results
                    if status == 'COMPLETE_OK' and 'results' in data:
                        section = data['results']['sections'][0]
                        return WellnessResults(
                            distress=section['uniformDistress']['value'],
                            stress=section['uniformStress']['value'],
                            burnout=section['uniformExhaustion']['value'],
                            fatigue=section['uniformSleepPropensity']['value'],
                            low_self_esteem=section['uniformLowSelfEsteem']['value'],
                            transcript=section.get('transcript', ''),
                            status=status
                        )
                    else:
                        raise Exception(f"Analysis failed with status: {status}")

            await asyncio.sleep(poll_interval)

        raise Exception("Max polling attempts reached")

    async def __aenter__(self):
        self.session = aiohttp.ClientSession()
        return self

    async def __aexit__(self, *args):
        if self.session:
            await self.session.close()
```

#### 4. Main Extension (`extension.py`)

```python
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    AudioFrame,
    Data,
)
from dataclasses import dataclass
import asyncio
import logging

from .audio_buffer import AudioBuffer
from .wav_converter import WavConverter
from .thymia_client import ThymiaClient, ThymiaConfig

logger = logging.getLogger(__name__)

@dataclass
class ThymiaAnalyzerConfig:
    api_key: str
    user_label: str = "anonymous"
    date_of_birth: str = "1990-01-01"
    birth_sex: str = "UNSPECIFIED"
    language: str = "en-GB"
    sample_rate: int = 16000
    channels: int = 1
    min_speech_duration: float = 30.0
    silence_threshold: float = 0.02
    continuous_analysis: bool = True

class ThymiaAnalyzerExtension(AsyncExtension):
    """Extension for mental wellness analysis using Thymia API"""

    def __init__(self, name: str):
        super().__init__(name)
        self.config: ThymiaAnalyzerConfig = None
        self.audio_buffer: AudioBuffer = None
        self.thymia_client: ThymiaClient = None
        self.active_analysis: Optional[asyncio.Task] = None
        self.analysis_count = 0

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        logger.info("ThymiaAnalyzerExtension starting")

        # Load configuration
        self.config = ThymiaAnalyzerConfig(
            api_key=await ten_env.get_property_string("api_key"),
            user_label=await ten_env.get_property_string("user_label"),
            date_of_birth=await ten_env.get_property_string("date_of_birth"),
            birth_sex=await ten_env.get_property_string("birth_sex"),
            language=await ten_env.get_property_string("language"),
            sample_rate=await ten_env.get_property_int("sample_rate"),
            min_speech_duration=await ten_env.get_property_float("min_speech_duration"),
            silence_threshold=await ten_env.get_property_float("silence_threshold"),
            continuous_analysis=await ten_env.get_property_bool("continuous_analysis"),
        )

        # Initialize components
        self.audio_buffer = AudioBuffer(
            self.config.sample_rate,
            self.config.channels,
            self.config.silence_threshold
        )

        thymia_config = ThymiaConfig(
            api_key=self.config.api_key,
            user_label=self.config.user_label,
            date_of_birth=self.config.date_of_birth,
            birth_sex=self.config.birth_sex,
            language=self.config.language
        )

        self.thymia_client = ThymiaClient(thymia_config)
        await self.thymia_client.__aenter__()

        ten_env.on_start_done()

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        logger.info("ThymiaAnalyzerExtension stopping")

        if self.active_analysis:
            self.active_analysis.cancel()

        if self.thymia_client:
            await self.thymia_client.__aexit__()

        ten_env.on_stop_done()

    async def on_audio_frame(self, ten_env: AsyncTenEnv, audio_frame: AudioFrame) -> None:
        """Receive audio frames from agora_rtc via streamid_adapter"""

        # Get PCM data from frame
        pcm_data = audio_frame.get_data()

        # Add to buffer and check speech duration
        speech_duration = self.audio_buffer.add_frame(pcm_data)

        # If we have enough speech and no active analysis, start one
        if (self.audio_buffer.has_enough_speech(self.config.min_speech_duration)
            and not self.active_analysis):

            logger.info(f"Sufficient speech detected ({speech_duration:.1f}s), starting analysis")
            self.active_analysis = asyncio.create_task(
                self._analyze_audio(ten_env)
            )

    async def _analyze_audio(self, ten_env: AsyncTenEnv):
        """Analyze buffered audio via Thymia API"""
        try:
            self.analysis_count += 1
            logger.info(f"Starting analysis #{self.analysis_count}")

            # Get buffered audio
            pcm_data = self.audio_buffer.get_buffer()

            # Convert to WAV
            wav_data = WavConverter.pcm_to_wav(
                pcm_data,
                self.config.sample_rate,
                self.config.channels
            )

            # Create Thymia session
            model_run_id, upload_url = await self.thymia_client.create_session()
            logger.info(f"Created Thymia session: {model_run_id}")

            # Upload audio
            await self.thymia_client.upload_audio(upload_url, wav_data)
            logger.info("Audio uploaded, polling for results...")

            # Get results (polls until complete)
            results = await self.thymia_client.get_results(model_run_id)
            logger.info(f"Analysis complete: distress={results.distress:.2f}, stress={results.stress:.2f}")

            # Send results to main_control for LLM context
            await self._send_results_to_llm(ten_env, results)

            # Clear buffer if continuous analysis is enabled
            if self.config.continuous_analysis:
                self.audio_buffer.clear()
                logger.info("Buffer cleared for next analysis")

        except Exception as e:
            logger.error(f"Analysis failed: {e}", exc_info=True)

        finally:
            self.active_analysis = None

    async def _send_results_to_llm(self, ten_env: AsyncTenEnv, results: WellnessResults):
        """Send wellness results to LLM via data message"""

        # Format results for LLM
        wellness_data = Data.create("wellness_analysis")
        wellness_data.set_property_string("distress", f"{results.distress:.2f}")
        wellness_data.set_property_string("stress", f"{results.stress:.2f}")
        wellness_data.set_property_string("burnout", f"{results.burnout:.2f}")
        wellness_data.set_property_string("fatigue", f"{results.fatigue:.2f}")
        wellness_data.set_property_string("low_self_esteem", f"{results.low_self_esteem:.2f}")
        wellness_data.set_property_string("transcript", results.transcript)
        wellness_data.set_property_int("analysis_count", self.analysis_count)

        await ten_env.send_data(wellness_data)
        logger.info("Wellness results sent to LLM")
```

### Configuration Files

#### manifest.json

```json
{
  "type": "extension",
  "name": "thymia_analyzer_python",
  "version": "0.1.0",
  "dependencies": [
    {
      "type": "system",
      "name": "ten_runtime_python",
      "version": "0.11"
    }
  ],
  "package": {
    "include": [
      "manifest.json",
      "property.json",
      "**.py",
      "README.md",
      "requirements.txt"
    ]
  },
  "api": {
    "property": {
      "properties": {
        "api_key": {
          "type": "string"
        },
        "user_label": {
          "type": "string"
        },
        "date_of_birth": {
          "type": "string"
        },
        "birth_sex": {
          "type": "string"
        },
        "language": {
          "type": "string"
        },
        "sample_rate": {
          "type": "int64"
        },
        "min_speech_duration": {
          "type": "float64"
        },
        "silence_threshold": {
          "type": "float64"
        },
        "continuous_analysis": {
          "type": "bool"
        }
      }
    }
  }
}
```

#### property.json

```json
{
  "api_key": "${env:THYMIA_API_KEY}",
  "user_label": "anonymous",
  "date_of_birth": "1990-01-01",
  "birth_sex": "UNSPECIFIED",
  "language": "en-GB",
  "sample_rate": 16000,
  "min_speech_duration": 22.0,
  "silence_threshold": 0.02,
  "continuous_analysis": true
}
```

#### requirements.txt

```
aiohttp>=3.9.0
numpy>=1.24.0
```

---

## Graph Configuration: voice-assistant-thymia

### Directory Structure

```
ai_agents/agents/examples/voice-assistant-thymia/
├── tenapp/
│   ├── property.json
│   ├── manifest.json
│   └── scripts/
└── README.md
```

### Graph Definition (property.json)

```json
{
  "ten": {
    "predefined_graphs": [
      {
        "name": "voice_assistant_thymia",
        "auto_start": true,
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "agora_rtc",
              "addon": "agora_rtc",
              "extension_group": "default",
              "property": {
                "app_id": "${env:AGORA_APP_ID}",
                "app_certificate": "${env:AGORA_APP_CERTIFICATE|}",
                "channel": "ten_agent_test",
                "stream_id": 1234,
                "remote_stream_id": 123,
                "subscribe_audio": true,
                "publish_audio": true,
                "publish_data": true,
                "enable_agora_asr": false
              }
            },
            {
              "type": "extension",
              "name": "streamid_adapter",
              "addon": "streamid_adapter",
              "property": {}
            },
            {
              "type": "extension",
              "name": "stt",
              "addon": "deepgram_asr_python",
              "extension_group": "stt",
              "property": {
                "params": {
                  "api_key": "${env:DEEPGRAM_API_KEY}",
                  "language": "en-US",
                  "model": "nova-3"
                }
              }
            },
            {
              "type": "extension",
              "name": "llm",
              "addon": "openai_llm2_python",
              "extension_group": "chatgpt",
              "property": {
                "base_url": "https://api.openai.com/v1",
                "api_key": "${env:OPENAI_API_KEY}",
                "frequency_penalty": 0.9,
                "model": "${env:OPENAI_MODEL}",
                "max_tokens": 512,
                "prompt": "You are a supportive AI assistant with access to the user's mental wellness metrics. Be empathetic and adjust your responses based on their emotional state.",
                "greeting": "Hello! I'm here to chat with you. How are you feeling today?",
                "max_memory_length": 10
              }
            },
            {
              "type": "extension",
              "name": "tts",
              "addon": "elevenlabs_tts2_python",
              "extension_group": "tts",
              "property": {
                "params": {
                  "key": "${env:ELEVENLABS_TTS_KEY}",
                  "model_id": "eleven_multilingual_v2",
                  "voice_id": "pNInz6obpgDQGcFmaJgB",
                  "output_format": "pcm_16000"
                }
              }
            },
            {
              "type": "extension",
              "name": "main_control",
              "addon": "main_python",
              "extension_group": "control",
              "property": {
                "greeting": "Hello! I'm here to chat with you. How are you feeling today?"
              }
            },
            {
              "type": "extension",
              "name": "message_collector",
              "addon": "message_collector2",
              "extension_group": "transcriber",
              "property": {}
            },
            {
              "type": "extension",
              "name": "thymia_analyzer",
              "addon": "thymia_analyzer_python",
              "extension_group": "default",
              "property": {
                "api_key": "${env:THYMIA_API_KEY}",
                "user_label": "${env:USER_LABEL|anonymous}",
                "date_of_birth": "${env:USER_DOB|1990-01-01}",
                "birth_sex": "${env:USER_BIRTH_SEX|UNSPECIFIED}",
                "language": "en-GB",
                "sample_rate": 16000,
                "min_speech_duration": 22.0,
                "silence_threshold": 0.02,
                "continuous_analysis": true
              }
            }
          ],
          "connections": [
            {
              "extension": "main_control",
              "cmd": [
                {
                  "names": ["on_user_joined", "on_user_left"],
                  "source": [{"extension": "agora_rtc"}]
                },
                {
                  "names": ["tool_register"],
                  "source": [
                    {"extension": "weatherapi_tool_python"},
                    {"extension": "thymia_analyzer"}
                  ]
                }
              ],
              "data": [
                {
                  "name": "asr_result",
                  "source": [{"extension": "stt"}]
                }
              ]
            },
            {
              "extension": "agora_rtc",
              "audio_frame": [
                {
                  "name": "pcm_frame",
                  "dest": [{"extension": "streamid_adapter"}]
                },
                {
                  "name": "pcm_frame",
                  "source": [{"extension": "tts"}]
                }
              ],
              "data": [
                {
                  "name": "data",
                  "source": [{"extension": "message_collector"}]
                }
              ]
            },
            {
              "extension": "streamid_adapter",
              "audio_frame": [
                {
                  "name": "pcm_frame",
                  "dest": [
                    {"extension": "stt"},
                    {"extension": "thymia_analyzer"}
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}
```

### Environment Variables (.env)

```bash
# Agora
AGORA_APP_ID=your_agora_app_id
AGORA_APP_CERTIFICATE=

# Thymia
THYMIA_API_KEY=your_thymia_api_key
USER_LABEL=test_user
USER_DOB=1990-01-01
USER_BIRTH_SEX=UNSPECIFIED

# Speech-to-Text
DEEPGRAM_API_KEY=your_deepgram_api_key

# LLM
OPENAI_API_KEY=your_openai_api_key
OPENAI_MODEL=gpt-4o-mini

# Text-to-Speech
ELEVENLABS_TTS_KEY=your_elevenlabs_api_key
```

---

## LLM Context Integration

### Approach: Tool/Function Call Pattern ⭐ **RECOMMENDED**

Following the TEN Framework pattern (weatherapi_tool, bing_search), the extension will:

1. **Inherit from `AsyncLLMToolBaseExtension`**
2. **Register as an LLM tool** on startup
3. **Respond to tool calls** with latest wellness metrics
4. **Run analysis in background** while buffering audio

```python
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolMetadataParameter,
    LLMToolResult,
    LLMToolResultLLMResult,
)

class ThymiaAnalyzerExtension(AsyncLLMToolBaseExtension):
    """Extension that analyzes speech for wellness metrics AND acts as LLM tool"""

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        """Register wellness metrics as queryable tool"""
        return [
            LLMToolMetadata(
                name="get_wellness_metrics",
                description="Get user's current mental wellness metrics from voice analysis. "
                           "Returns distress, stress, burnout, fatigue, and self-esteem levels (0-10 scale).",
                parameters=[]  # No parameters needed
            )
        ]

    async def run_tool(self, ten_env: AsyncTenEnv, name: str, args: dict) -> LLMToolResult:
        """Called when LLM wants wellness metrics"""
        ten_env.log_info(f"LLM requesting wellness metrics")

        if not self.latest_results:
            # Analysis not ready yet
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps({
                    "status": "analyzing" if self.active_analysis else "insufficient_data",
                    "message": "Wellness analysis in progress" if self.active_analysis
                              else "Need 22 seconds of speech for analysis",
                    "speech_collected_seconds": self.audio_buffer.speech_duration
                })
            )

        # Return latest available metrics
        return LLMToolResultLLMResult(
            type="llmresult",
            content=json.dumps({
                "status": "available",
                "metrics": {
                    "distress": round(self.latest_results.distress, 1),
                    "stress": round(self.latest_results.stress, 1),
                    "burnout": round(self.latest_results.burnout, 1),
                    "fatigue": round(self.latest_results.fatigue, 1),
                    "low_self_esteem": round(self.latest_results.low_self_esteem, 1)
                },
                "analyzed_at": self.analysis_timestamp,
                "analysis_count": self.analysis_count
            })
        )

    async def on_audio_frame(self, ten_env: AsyncTenEnv, audio_frame: AudioFrame):
        """Background: continuously buffer and analyze audio"""
        # Buffer audio with VAD
        # When 30s speech collected, automatically trigger analysis
        # Store results for tool to retrieve
```

### How It Works

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Extension Startup                                        │
│    thymia_analyzer → sends tool_register command            │
│                   → main_control → forwards to LLM          │
│                                                              │
│ 2. Audio Processing (background)                            │
│    User speaks → agora_rtc → streamid_adapter               │
│              → thymia_analyzer (buffers & analyzes)         │
│              → Thymia API (when 30s collected)              │
│              → stores results internally                     │
│                                                              │
│ 3. LLM Requests Metrics                                     │
│    User: "I'm feeling overwhelmed"                          │
│    LLM: <calls get_wellness_metrics tool>                   │
│    main_control → sends tool_call command                   │
│               → thymia_analyzer.run_tool()                  │
│               → returns latest metrics                       │
│    LLM: "I can sense elevated stress in your voice..."      │
└─────────────────────────────────────────────────────────────┘
```

### Benefits

- ✅ **LLM-controlled**: Wellness data retrieved only when relevant
- ✅ **Non-blocking**: Analysis happens in background, tool returns immediately
- ✅ **Graceful**: Returns status if analysis not ready yet
- ✅ **Standard pattern**: Follows TEN Framework tool architecture
- ✅ **Privacy-friendly**: Data only accessed when LLM requests it

---

## Implementation Phases

### Phase 1: Core Extension (Week 1)

**Tasks**:
- [ ] Create extension directory structure
- [ ] Implement AudioBuffer with VAD
- [ ] Implement WavConverter
- [ ] Implement ThymiaClient (basic)
- [ ] Implement main extension logic
- [ ] Write unit tests

**Deliverables**:
- Working extension that can buffer audio and convert to WAV
- Basic Thymia API integration (create session, upload, poll)

### Phase 2: Graph Integration (Week 1-2)

**Tasks**:
- [ ] Create voice-assistant-thymia example
- [ ] Configure graph with thymia_analyzer node
- [ ] Wire up audio routing (streamid_adapter → thymia_analyzer)
- [ ] Wire up data routing (thymia_analyzer → main_control)
- [ ] Test audio flow end-to-end

**Deliverables**:
- Working graph that routes audio to Thymia
- Wellness results visible in logs

### Phase 3: LLM Context (Week 2)

**Tasks**:
- [ ] Modify main_control to receive wellness data
- [ ] Implement system prompt injection
- [ ] Test LLM responses with wellness context
- [ ] Add wellness-aware conversation examples

**Deliverables**:
- LLM receives and uses wellness context in responses
- Demonstrable empathetic behavior based on metrics

### Phase 4: Polish & Testing (Week 2-3)

**Tasks**:
- [ ] Add comprehensive error handling
- [ ] Implement retry logic for API failures
- [ ] Add metrics and logging
- [ ] Write integration tests
- [ ] Create test scripts
- [ ] Write documentation

**Deliverables**:
- Production-ready extension
- Test suite
- User and developer documentation

---

## Testing Strategy

### Unit Tests

```python
# test_audio_buffer.py
def test_speech_detection():
    buffer = AudioBuffer(16000, 1, 0.02)
    # Test with silence
    # Test with speech
    # Test duration calculation

# test_wav_converter.py
def test_pcm_to_wav_conversion():
    # Verify WAV header format
    # Verify data integrity

# test_thymia_client.py
@pytest.mark.asyncio
async def test_create_session():
    # Mock Thymia API
    # Test session creation
```

### Integration Tests

```bash
# test_thymia_flow.sh
# 1. Start agent with thymia graph
# 2. Send audio with speech
# 3. Wait for analysis
# 4. Verify wellness data sent to LLM
# 5. Check LLM response includes context
```

### Manual Testing Checklist

- [ ] Audio buffering works (30s speech threshold)
- [ ] VAD correctly filters silence
- [ ] WAV conversion produces valid audio
- [ ] Thymia API session creation succeeds
- [ ] Audio upload succeeds
- [ ] Result polling completes
- [ ] Wellness data reaches LLM
- [ ] LLM adjusts responses based on metrics
- [ ] Continuous analysis works (multiple uploads)
- [ ] Error handling graceful (API failures, network issues)

---

## API Reference

### Thymia API Endpoints

**Create Session**:
```http
POST https://api.thymia.ai/v1/models/mental-wellness
Headers:
  x-api-key: {api_key}
  Content-Type: application/json
Body:
  {
    "user": {
      "userLabel": "string",
      "dateOfBirth": "YYYY-MM-DD",
      "birthSex": "MALE|FEMALE|UNSPECIFIED"
    },
    "language": "en-GB"
  }
Response:
  {
    "id": "model_run_id",
    "recordingUploadUrl": "https://..."
  }
```

**Upload Audio**:
```http
PUT {recordingUploadUrl}
Headers:
  Content-Type: audio/wav
Body: WAV file bytes
```

**Get Results**:
```http
GET https://api.thymia.ai/v1/models/mental-wellness/{model_run_id}
Headers:
  x-api-key: {api_key}
Response:
  {
    "status": "COMPLETE_OK|COMPLETE_ERROR|FAILED|PROCESSING",
    "results": {
      "sections": [{
        "uniformDistress": {"value": 0.0-10.0},
        "uniformStress": {"value": 0.0-10.0},
        "uniformExhaustion": {"value": 0.0-10.0},
        "uniformSleepPropensity": {"value": 0.0-10.0},
        "uniformLowSelfEsteem": {"value": 0.0-10.0},
        "transcript": "string"
      }]
    }
  }
```

---

## Security Considerations

1. **API Key Management**:
   - Store in environment variables
   - Never commit to git
   - Rotate regularly

2. **User Privacy**:
   - Use anonymous user labels by default
   - Allow opt-out of analysis
   - Clear audio buffers after upload
   - Don't log sensitive wellness data

3. **Data Retention**:
   - Thymia may retain audio - inform users
   - Consider local-only mode for testing
   - Implement data deletion on user request

---

## Future Enhancements

1. **Multi-language Support**: Extend beyond en-GB
2. **Real-time Metrics**: Stream partial results as they become available
3. **Trend Analysis**: Track wellness metrics over time
4. **Alerts**: Notify if metrics indicate crisis
5. **Visualization**: Dashboard showing wellness trends
6. **Configurable Thresholds**: Allow customization of speech duration, silence threshold
7. **Local VAD**: Use more sophisticated voice activity detection
8. **Compression**: Compress WAV before upload to reduce bandwidth

---

## Known Limitations

1. **Latency**: Analysis takes 30-120 seconds after audio upload
2. **Speech Requirement**: Needs 22+ seconds of actual speech (not total audio)
3. **API Costs**: Each analysis counts toward Thymia API quota
4. **Single Language**: Currently supports only configured language
5. **No Cancellation**: Once started, analysis cannot be cancelled
6. **Network Dependency**: Requires stable connection to Thymia API
7. **Proactive Notification**: text_data notifications don't trigger LLM responses automatically

---

## References

- [Thymia API Documentation](https://api.thymia.ai/docs)
- [TEN Framework Documentation](https://doc.theten.ai)
- [WAV File Format Specification](http://soundfile.sapp.org/doc/WaveFormat/)
- [Voice Activity Detection Techniques](https://en.wikipedia.org/wiki/Voice_activity_detection)

---

**Created**: 2025-10-28
**Author**: Claude Code
**Status**: ✅ **IMPLEMENTED** - Testing in progress

## Implementation Status (2025-10-28)

### ✅ Completed
- Extension created at `agents/ten_packages/extension/thymia_analyzer_python/`
- All core components implemented (AudioBuffer, Thymia API client, tool registration)
- Graph configuration added to voice-assistant property.json
- Parallel audio routing configured (STT + Thymia analyzer)
- Tool/function call pattern implemented (set_user_info, get_wellness_metrics)
- Environment variables configured (.env)
- Dependencies installed (pydantic, aiohttp, aiofiles)

### 🐛 Bugs Fixed
1. **TypeError in _calculate_rms** (2025-10-28):
   - Issue: `'>' not supported between instances of 'float' and 'tuple'`
   - Root cause: TEN Framework's `get_property_float()` returns tuple `(value, None)` not just the value
   - Fix: Extract first element from tuple with pattern: `result[0] if isinstance(result, tuple) else result`
   - File: extension.py:277-281

2. **TypeError in analysis threshold checks** (2025-10-28):
   - Issue: `'<' not supported between instances of 'int' and 'tuple'`
   - Root cause: TEN Framework's `get_property_int()` also returns tuples `(value, None)`
   - Fix: Applied same tuple extraction pattern to all int properties:
     - min_interval_seconds
     - max_analyses_per_session
     - poll_timeout
     - poll_interval
   - File: extension.py:286-296

3. **Thymia API 403 Forbidden errors** (2025-10-28):
   - Issue: `Failed to create session: 403 - {"message":"Forbidden"}`
   - Root causes:
     - Wrong authentication: using `Authorization: Bearer` instead of `x-api-key` header
     - Wrong payload structure: fields at root level instead of nested under "user"
     - Wrong field name: using "locale" instead of "language"
     - Wrong status field: checking "state" instead of "status"
     - Wrong status values: checking "FINISHED" instead of "COMPLETE_OK"
     - Wrong metrics path: using `analysis.metrics.*` instead of `results.sections[0].uniform*`
   - Fixes applied:
     - Changed authentication header from `Authorization: Bearer {key}` to `x-api-key: {key}` (line 134)
     - Nested user info under "user" object in payload (lines 159-166)
     - Changed "locale" to "language" in payload
     - Accept both 200 and 201 status codes (line 172)
     - Check for "status" field with values "COMPLETE_OK", "COMPLETE_ERROR", "FAILED" (lines 207-209)
     - Extract metrics from `results.sections[0].uniformDistress.value` etc (lines 427-441)
     - Added cooldown on failures to prevent retry spam (line 455)
   - File: extension.py

4. **API key serialization error** (2025-10-28):
   - Issue: `Cannot serialize non-str key ('<api_key_value>', None)`
   - Root cause: `get_property_string()` ALSO returns tuples `(value, None)` like float and int getters
   - Fix: Applied tuple extraction to API key loading (lines 277-278)
   - File: extension.py
   - **Critical Pattern**: ALL TEN Framework property getters return tuples, regardless of type (string, int, float, bool)

### ✅ Working Configuration (2025-10-28)

**Current Settings**:
- Speech threshold: 22 seconds (reduced from 30s for faster analysis)
- Silence threshold: 0.02 RMS
- Poll timeout: 120 seconds
- Poll interval: 5 seconds
- Max analyses per session: 10
- Continuous analysis: enabled

**LLM Integration**:
- LLM prompt includes instructions to check wellness metrics after 22s of speech
- Short responses (max 15 words) unless presenting full stats
- Scale clarification: 0-1 scale (0=none/low, 0.5=moderate, 1.0=severe/high)
- Greeting: "Hi! I'll analyze your wellness from your voice. Tell me about your day today?"

**Audio Routing**:
- Parallel routing from agora_rtc to both streamid_adapter and thymia_analyzer
- Audio split at source, not intermediate node

**Tool Integration**:
- `set_user_info(name, date_of_birth, birth_sex)` - Set user demographic info for API
- `get_wellness_metrics()` - Retrieve latest wellness analysis results

**Known Limitations**:
- Proactive notifications sent but don't trigger LLM to announce results automatically
- LLM will present results when explicitly asked, but won't proactively announce
- Workaround: LLM prompted to check for results after 22s of speech

**Test Results**:
- ✅ Analysis completes successfully with correct metrics
- ✅ Audio buffering and VAD working
- ✅ Thymia API integration working (authentication, upload, polling)
- ✅ Tool calls working (set_user_info, get_wellness_metrics)
- ⚠️ Proactive announcement needs improvement
