#
# Agora Real Time Engagement
# Created by Claude Code in 2025-10.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
import asyncio
import json
import math
import struct
import time
from dataclasses import dataclass
from typing import Optional
import aiohttp

from ten_runtime.async_ten_env import AsyncTenEnv
from ten_runtime.async_extension import AsyncExtension
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension
from ten_ai_base.types import LLMToolMetadata, LLMToolResult, LLMToolResultLLMResult
from ten_runtime.audio_frame import AudioFrame
from ten_runtime.data import Data


@dataclass
class WellnessMetrics:
    """Thymia API wellness analysis results"""
    distress: float
    stress: float
    burnout: float
    fatigue: float
    low_self_esteem: float
    timestamp: float
    session_id: str


class AudioBuffer:
    """Buffer PCM audio frames with voice activity detection"""

    def __init__(self, sample_rate=16000, channels=1, silence_threshold=0.02):
        self.sample_rate = sample_rate
        self.channels = channels
        self.silence_threshold = silence_threshold
        self.speech_buffer = []
        self.speech_duration = 0.0

    def add_frame(self, pcm_data: bytes) -> float:
        """
        Add PCM frame and check if it contains speech.
        Returns total speech duration collected.
        """
        # Calculate RMS (Root Mean Square) volume
        volume = self._calculate_rms(pcm_data)

        if volume > self.silence_threshold:
            self.speech_buffer.append(pcm_data)
            # Duration = bytes / (sample_rate * channels * bytes_per_sample)
            self.speech_duration += len(pcm_data) / (self.sample_rate * self.channels * 2)

        return self.speech_duration

    def _calculate_rms(self, pcm_data: bytes) -> float:
        """Calculate RMS volume of PCM audio"""
        if not pcm_data:
            return 0.0

        # Convert bytes to 16-bit integers
        samples = struct.unpack(f'{len(pcm_data)//2}h', pcm_data)

        # Calculate RMS
        sum_squares = sum(s * s for s in samples)
        mean_square = sum_squares / len(samples)
        rms = math.sqrt(mean_square) / 32768.0  # Normalize to 0-1

        return rms

    def has_enough_speech(self, min_duration: float = 30.0) -> bool:
        """Check if we have enough speech for analysis"""
        return self.speech_duration >= min_duration

    def get_wav_data(self) -> bytes:
        """Convert buffered PCM data to WAV format"""
        if not self.speech_buffer:
            return b''

        # Concatenate all PCM frames
        pcm_data = b''.join(self.speech_buffer)

        # Convert to WAV
        return self._pcm_to_wav(pcm_data, self.sample_rate, self.channels)

    @staticmethod
    def _pcm_to_wav(pcm_data: bytes, sample_rate: int, channels: int, bits_per_sample: int = 16) -> bytes:
        """Convert raw PCM data to WAV format"""
        data_size = len(pcm_data)

        # Build WAV header
        header = struct.pack(
            '<4sI4s4sIHHIIHH4sI',
            b'RIFF',                                          # Chunk ID
            36 + data_size,                                   # Chunk size
            b'WAVE',                                          # Format
            b'fmt ',                                          # Subchunk1 ID
            16,                                               # Subchunk1 size (PCM)
            1,                                                # Audio format (1 = PCM)
            channels,                                         # Number of channels
            sample_rate,                                      # Sample rate
            sample_rate * channels * bits_per_sample // 8,  # Byte rate
            channels * bits_per_sample // 8,                # Block align
            bits_per_sample,                                 # Bits per sample
            b'data',                                          # Subchunk2 ID
            data_size,                                        # Subchunk2 size
        )

        return header + pcm_data

    def clear(self):
        """Clear the buffer and reset duration"""
        self.speech_buffer.clear()
        self.speech_duration = 0.0


class ThymiaAPIClient:
    """Client for Thymia Mental Wellness API"""

    def __init__(self, api_key: str, base_url: str = "https://api.thymia.ai"):
        self.api_key = api_key
        self.base_url = base_url
        self.session: Optional[aiohttp.ClientSession] = None

    async def _ensure_session(self):
        """Ensure aiohttp session exists"""
        if self.session is None:
            self.session = aiohttp.ClientSession(
                headers={
                    "x-api-key": self.api_key,
                    "Content-Type": "application/json"
                }
            )

    async def close(self):
        """Close the aiohttp session"""
        if self.session:
            await self.session.close()
            self.session = None

    async def create_session(self, user_label: str = "anonymous",
                           date_of_birth: str = "1990-01-01",
                           birth_sex: str = "UNSPECIFIED",
                           locale: str = "en-US") -> dict:
        """
        Create a new Thymia analysis session.

        Returns: {
            "id": "session_id",
            "recordingUploadUrl": "presigned_s3_url"
        }
        """
        await self._ensure_session()

        payload = {
            "user": {
                "userLabel": user_label,
                "dateOfBirth": date_of_birth,
                "birthSex": birth_sex
            },
            "language": locale
        }

        async with self.session.post(
            f"{self.base_url}/v1/models/mental-wellness",
            json=payload
        ) as response:
            if response.status not in (200, 201):
                error_text = await response.text()
                raise Exception(f"Failed to create session: {response.status} - {error_text}")

            return await response.json()

    async def upload_audio(self, upload_url: str, wav_data: bytes) -> bool:
        """Upload WAV audio file to presigned S3 URL"""
        await self._ensure_session()

        async with self.session.put(
            upload_url,
            data=wav_data,
            headers={"Content-Type": "audio/wav"}
        ) as response:
            return response.status == 200

    async def get_results(self, session_id: str) -> Optional[dict]:
        """
        Poll for analysis results.

        Returns None if still processing, dict with results if complete.
        """
        await self._ensure_session()

        async with self.session.get(
            f"{self.base_url}/v1/models/mental-wellness/{session_id}"
        ) as response:
            if response.status != 200:
                error_text = await response.text()
                raise Exception(f"Failed to get results: {response.status} - {error_text}")

            data = await response.json()

            # Check if analysis is complete
            status = data.get("status", "")
            if status in ("COMPLETE_OK", "COMPLETE_ERROR", "FAILED"):
                return data

            return None

    async def poll_results(self, session_id: str,
                          max_wait_seconds: int = 120,
                          poll_interval: int = 5) -> Optional[dict]:
        """
        Poll for results with timeout and interval.

        Returns results dict if successful, None if timeout.
        """
        start_time = time.time()

        while time.time() - start_time < max_wait_seconds:
            results = await self.get_results(session_id)
            if results:
                return results

            await asyncio.sleep(poll_interval)

        return None


class ThymiaAnalyzerExtension(AsyncLLMToolBaseExtension):
    """
    Extension that analyzes speech for mental wellness metrics.

    - Continuously buffers audio in background
    - Automatically triggers analysis when 30s of speech collected
    - Registers as LLM tool for on-demand metrics retrieval
    - Supports multiple analyses per session
    """

    def __init__(self, name: str):
        super().__init__(name)

        # Configuration
        self.api_key: str = ""
        self.min_speech_duration: float = 30.0
        self.silence_threshold: float = 0.02
        self.continuous_analysis: bool = True
        self.min_interval_seconds: int = 60
        self.max_analyses_per_session: int = 10
        self.poll_timeout: int = 120
        self.poll_interval: int = 5

        # State
        self.audio_buffer: Optional[AudioBuffer] = None
        self.api_client: Optional[ThymiaAPIClient] = None
        self.latest_results: Optional[WellnessMetrics] = None
        self.active_analysis: bool = False
        self.analysis_count: int = 0
        self.last_analysis_time: float = 0.0

        # User information for Thymia API
        self.user_name: Optional[str] = None
        self.user_dob: Optional[str] = None
        self.user_sex: Optional[str] = None
        self.user_locale: str = "en-US"

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        """Called when extension starts"""
        ten_env.log_info("ThymiaAnalyzerExtension starting...")

        # Load configuration
        try:
            # TEN Framework returns tuples (value, error) - extract first element for ALL property types
            api_key_result = await ten_env.get_property_string("api_key")
            self.api_key = api_key_result[0] if isinstance(api_key_result, tuple) else api_key_result
            min_speech_result = await ten_env.get_property_float("min_speech_duration")
            self.min_speech_duration = min_speech_result[0] if isinstance(min_speech_result, tuple) else min_speech_result

            silence_result = await ten_env.get_property_float("silence_threshold")
            self.silence_threshold = silence_result[0] if isinstance(silence_result, tuple) else silence_result

            self.continuous_analysis = await ten_env.get_property_bool("continuous_analysis")

            # TEN Framework returns tuples (value, error) for int properties too
            min_interval_result = await ten_env.get_property_int("min_interval_seconds")
            self.min_interval_seconds = min_interval_result[0] if isinstance(min_interval_result, tuple) else min_interval_result

            max_analyses_result = await ten_env.get_property_int("max_analyses_per_session")
            self.max_analyses_per_session = max_analyses_result[0] if isinstance(max_analyses_result, tuple) else max_analyses_result

            poll_timeout_result = await ten_env.get_property_int("poll_timeout")
            self.poll_timeout = poll_timeout_result[0] if isinstance(poll_timeout_result, tuple) else poll_timeout_result

            poll_interval_result = await ten_env.get_property_int("poll_interval")
            self.poll_interval = poll_interval_result[0] if isinstance(poll_interval_result, tuple) else poll_interval_result

            ten_env.log_info(f"Loaded config: silence_threshold={self.silence_threshold}, min_speech_duration={self.min_speech_duration}")
        except Exception as e:
            ten_env.log_warn(f"Failed to load some properties, using defaults: {e}")

        # Validate API key
        if not self.api_key:
            ten_env.log_error("Thymia API key not configured - extension will be disabled")
            await super().on_start(ten_env)
            return

        # Initialize components
        self.audio_buffer = AudioBuffer(
            sample_rate=16000,
            channels=1,
            silence_threshold=self.silence_threshold
        )
        self.api_client = ThymiaAPIClient(api_key=self.api_key)

        ten_env.log_info(f"ThymiaAnalyzerExtension started (min_speech={self.min_speech_duration}s)")

        # Register as LLM tool (parent class handles this)
        await super().on_start(ten_env)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        """Called when extension stops"""
        ten_env.log_info("ThymiaAnalyzerExtension stopping...")

        if self.api_client:
            await self.api_client.close()

        await super().on_stop(ten_env)

    # Counter for logging
    _audio_frame_count = 0

    async def on_audio_frame(self, ten_env: AsyncTenEnv, audio_frame: AudioFrame) -> None:
        """Process incoming audio frames"""
        try:
            # Log every 100th frame to confirm audio is being received
            self._audio_frame_count += 1
            if self._audio_frame_count % 100 == 1:
                ten_env.log_info(f"[thymia_analyzer] Received audio frame #{self._audio_frame_count}")

            if not self.audio_buffer:
                ten_env.log_warn(f"[thymia_analyzer] Audio buffer not initialized")
                return

            if not self.api_client:
                ten_env.log_warn(f"[thymia_analyzer] API client not initialized")
                return

            # Get PCM data from audio frame
            buf = audio_frame.lock_buf()
            pcm_data = bytes(buf)
            audio_frame.unlock_buf(buf)

            if self._audio_frame_count % 100 == 1:
                ten_env.log_info(f"[thymia_analyzer] PCM data size: {len(pcm_data)} bytes")

            # Add to buffer with VAD
            speech_duration = self.audio_buffer.add_frame(pcm_data)

            # Check if we have enough speech to analyze
            if self.audio_buffer.has_enough_speech(self.min_speech_duration):
                # Check if we should start a new analysis
                should_analyze = (
                    not self.active_analysis and
                    self.analysis_count < self.max_analyses_per_session and
                    (time.time() - self.last_analysis_time) >= self.min_interval_seconds
                )

                if should_analyze:
                    ten_env.log_info(f"Starting wellness analysis ({speech_duration:.1f}s speech collected)")

                    # Start analysis in background
                    asyncio.create_task(self._run_analysis(ten_env))
        except Exception as e:
            import traceback
            ten_env.log_error(f"[thymia_analyzer] Error in on_audio_frame: {e}\n{traceback.format_exc()}")

    async def _run_analysis(self, ten_env: AsyncTenEnv):
        """Run Thymia analysis workflow in background"""
        self.active_analysis = True

        try:
            # Get WAV data from buffer
            wav_data = self.audio_buffer.get_wav_data()

            if not wav_data:
                ten_env.log_warn("No audio data available for analysis")
                return

            ten_env.log_info(f"Starting Thymia API workflow ({len(wav_data)} bytes)")

            # Step 1: Create session with user info
            session_response = await self.api_client.create_session(
                user_label=self.user_name or "anonymous",
                date_of_birth=self.user_dob or "1990-01-01",
                birth_sex=self.user_sex or "UNSPECIFIED",
                locale=self.user_locale
            )
            session_id = session_response["id"]
            upload_url = session_response["recordingUploadUrl"]

            ten_env.log_info(f"Created Thymia session: {session_id}")

            # Step 2: Upload audio
            upload_success = await self.api_client.upload_audio(upload_url, wav_data)

            if not upload_success:
                ten_env.log_error("Failed to upload audio to Thymia")
                return

            ten_env.log_info("Audio uploaded successfully, polling for results...")

            # Step 3: Poll for results
            results = await self.api_client.poll_results(
                session_id,
                max_wait_seconds=self.poll_timeout,
                poll_interval=self.poll_interval
            )

            if not results:
                ten_env.log_warn(f"Thymia analysis timed out after {self.poll_timeout}s")
                return

            # Step 4: Extract metrics from results.sections[0]
            sections = results.get("results", {}).get("sections", [])
            if not sections:
                ten_env.log_error("No sections found in Thymia response")
                return

            section = sections[0]
            self.latest_results = WellnessMetrics(
                distress=section.get("uniformDistress", {}).get("value", 0.0),
                stress=section.get("uniformStress", {}).get("value", 0.0),
                burnout=section.get("uniformExhaustion", {}).get("value", 0.0),
                fatigue=section.get("uniformSleepPropensity", {}).get("value", 0.0),
                low_self_esteem=section.get("uniformLowSelfEsteem", {}).get("value", 0.0),
                timestamp=time.time(),
                session_id=session_id
            )

            self.analysis_count += 1
            self.last_analysis_time = time.time()

            ten_env.log_info(
                f"Wellness analysis complete: "
                f"distress={self.latest_results.distress:.1f}, "
                f"stress={self.latest_results.stress:.1f}, "
                f"burnout={self.latest_results.burnout:.1f}"
            )

            # Send proactive notification to LLM
            await self._notify_llm_of_results(ten_env)

            # Clear buffer if not doing continuous analysis
            if self.continuous_analysis:
                self.audio_buffer.clear()

        except Exception as e:
            ten_env.log_error(f"[thymia_analyzer] Thymia analysis failed: {e}")
            # Set last_analysis_time even on failure to prevent immediate retry spam
            self.last_analysis_time = time.time()

        finally:
            self.active_analysis = False

    async def _notify_llm_of_results(self, ten_env: AsyncTenEnv):
        """Send proactive notification to LLM when wellness results are ready"""
        try:
            # Create a data message with wellness notification
            notification = Data.create("text_data")

            # Format the message for the LLM
            message = (
                f"WELLNESS ANALYSIS COMPLETE: The user's wellness metrics are now available. "
                f"Call get_wellness_metrics to retrieve and present them."
            )

            notification.set_property_string("text", message)

            # Send to main_control which will inject into LLM context
            await ten_env.send_data(notification)

            ten_env.log_info("[thymia_analyzer] Sent wellness notification to LLM")

        except Exception as e:
            ten_env.log_error(f"[thymia_analyzer] Failed to send notification: {e}")

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        """Register wellness analysis tools"""
        return [
            LLMToolMetadata(
                name="set_user_info",
                description=(
                    "Set user information required for mental wellness analysis. "
                    "This should be called BEFORE analyzing wellness metrics. "
                    "Ask the user naturally for their name, date of birth, and sex. "
                    "Example: 'To better understand your wellness, may I ask your name, date of birth, and sex?'"
                ),
                parameters=[
                    {
                        "name": "name",
                        "type": "string",
                        "description": "User's first name or identifier",
                        "required": True
                    },
                    {
                        "name": "date_of_birth",
                        "type": "string",
                        "description": "Date of birth in YYYY-MM-DD format (e.g., 1974-04-27)",
                        "required": True
                    },
                    {
                        "name": "sex",
                        "type": "string",
                        "description": "Biological sex: MALE, FEMALE, or OTHER",
                        "required": True
                    },
                    {
                        "name": "locale",
                        "type": "string",
                        "description": "Locale/language code (e.g., en-US, en-GB). Defaults to en-US if not provided.",
                        "required": False
                    }
                ]
            ),
            LLMToolMetadata(
                name="get_wellness_metrics",
                description=(
                    "Get user's current mental wellness metrics from voice analysis. "
                    "Returns stress, distress, burnout, fatigue, and self-esteem levels on a 0-1 scale "
                    "(where 0=none/low, 0.5=moderate, 1.0=severe/high). "
                    "The analysis is based on speech patterns and provides insight into the user's emotional state. "
                    "IMPORTANT: Call this periodically to check if analysis has completed and announce results immediately. "
                    "NOTE: User information must be set first using set_user_info."
                ),
                parameters=[]
            )
        ]

    async def run_tool(self, ten_env: AsyncTenEnv, name: str, args: dict) -> LLMToolResult:
        """Handle LLM tool calls for wellness analysis"""
        ten_env.log_info(f"LLM called tool: {name}")

        try:
            # Handle set_user_info tool
            if name == "set_user_info":
                # Extract parameters
                self.user_name = args.get("name", "").strip()
                self.user_dob = args.get("date_of_birth", "").strip()
                self.user_sex = args.get("sex", "").strip().upper()
                self.user_locale = args.get("locale", "en-US").strip()

                # Validate required fields
                if not self.user_name or not self.user_dob or not self.user_sex:
                    return LLMToolResultLLMResult(
                        type="llmresult",
                        content=json.dumps({
                            "status": "error",
                            "message": "Missing required fields: name, date_of_birth, and sex are all required"
                        })
                    )

                # Validate sex value
                if self.user_sex not in ["MALE", "FEMALE", "OTHER"]:
                    self.user_sex = "OTHER"

                ten_env.log_info(f"User info set: {self.user_name}, {self.user_dob}, {self.user_sex}, {self.user_locale}")

                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=json.dumps({
                        "status": "success",
                        "message": f"User information saved. Voice analysis will use: {self.user_name}, {self.user_dob}, {self.user_sex}"
                    })
                )

            # Handle get_wellness_metrics tool
            if name == "get_wellness_metrics":
                # Check if we have results
                if not self.latest_results:
                    # Determine status
                    if self.active_analysis:
                        status = "analyzing"
                        message = "Voice analysis in progress. Results will be available soon."
                    elif self.audio_buffer and self.audio_buffer.speech_duration > 0:
                        status = "insufficient_data"
                        message = f"Collecting speech for analysis ({self.audio_buffer.speech_duration:.1f}s / {self.min_speech_duration:.1f}s)"
                    else:
                        status = "no_data"
                        message = "No speech data collected yet for analysis"

                    return LLMToolResultLLMResult(
                        type="llmresult",
                        content=json.dumps({
                            "status": status,
                            "message": message
                        })
                    )

                # Return available metrics
                age_seconds = time.time() - self.latest_results.timestamp

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
                        "analyzed_seconds_ago": int(age_seconds),
                        "speech_duration": round(self.audio_buffer.speech_duration, 1) if self.audio_buffer else 0
                    })
                )

        except Exception as e:
            ten_env.log_error(f"Error in run_tool: {e}")
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps({
                    "status": "error",
                    "message": "Wellness analysis service temporarily unavailable"
                })
            )
