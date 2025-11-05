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
from datetime import datetime
import aiohttp

from ten_runtime.async_ten_env import AsyncTenEnv
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolResult,
    LLMToolResultLLMResult,
)
from ten_runtime.audio_frame import AudioFrame
from ten_runtime.data import Data
from .apollo_api import ApolloAPI, ApolloResult


class ThymiaAPIError(Exception):
    """Custom exception for Thymia API errors"""


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
    """Buffer PCM audio frames with voice activity detection and natural onset/offset"""

    def __init__(self, sample_rate=16000, channels=1, silence_threshold=0.02):
        self.sample_rate = sample_rate
        self.channels = channels
        self.silence_threshold = silence_threshold
        self.speech_buffer = []
        self.speech_duration = 0.0

        # Circular buffer for 0.5 second of recent audio (pre-speech capture)
        self.circular_buffer = []
        self.circular_buffer_max_duration = 0.5  # seconds

        # Track speech state
        self.is_speaking = False
        self.silence_frames = (
            []
        )  # Frames during potential end-of-speech silence
        self.silence_duration = 0.0
        self.silence_threshold_duration = (
            0.5  # seconds of silence to end speech
        )

    def add_frame(self, pcm_data: bytes) -> float:
        """
        Add PCM frame with intelligent onset/offset detection.
        Returns total speech duration collected.
        """
        # Calculate RMS (Root Mean Square) volume
        volume = self._calculate_rms(pcm_data)
        frame_duration = len(pcm_data) / (self.sample_rate * self.channels * 2)

        is_speech = volume > self.silence_threshold

        if not self.is_speaking:
            # Currently not speaking - maintain circular buffer
            self.circular_buffer.append(pcm_data)

            # Trim circular buffer to 1 second
            while (
                self._get_circular_buffer_duration()
                > self.circular_buffer_max_duration
            ):
                self.circular_buffer.pop(0)

            # Check for speech onset
            if is_speech:
                # Speech detected! Add circular buffer contents (pre-speech context)
                for buffered_frame in self.circular_buffer:
                    self.speech_buffer.append(buffered_frame)
                    self.speech_duration += len(buffered_frame) / (
                        self.sample_rate * self.channels * 2
                    )

                self.is_speaking = True
                self.circular_buffer.clear()  # Clear after using contents

        else:
            # Currently speaking
            if is_speech:
                # Continue speaking - add frame to speech buffer
                # Also flush any accumulated silence frames (they were part of speech)
                for silence_frame in self.silence_frames:
                    self.speech_buffer.append(silence_frame)
                    self.speech_duration += len(silence_frame) / (
                        self.sample_rate * self.channels * 2
                    )
                self.silence_frames.clear()
                self.silence_duration = 0.0

                # Add current frame
                self.speech_buffer.append(pcm_data)
                self.speech_duration += frame_duration

            else:
                # Silence during speech - accumulate for potential end-of-speech
                self.silence_frames.append(pcm_data)
                self.silence_duration += frame_duration

                # Check if we have 1 second of silence (end of speech)
                if self.silence_duration >= self.silence_threshold_duration:
                    # End of speech segment - finalize without trailing silence
                    self.is_speaking = False

                    # Clear accumulated silence (don't add to speech buffer)
                    self.silence_frames.clear()
                    self.silence_duration = 0.0

                    # Start new circular buffer with current frame
                    self.circular_buffer = [pcm_data]

        return self.speech_duration

    def _get_circular_buffer_duration(self) -> float:
        """Calculate total duration of circular buffer"""
        total_bytes = sum(len(frame) for frame in self.circular_buffer)
        return total_bytes / (self.sample_rate * self.channels * 2)

    def _calculate_rms(self, pcm_data: bytes) -> float:
        """Calculate RMS volume of PCM audio"""
        if not pcm_data:
            return 0.0

        # Ensure even length for 16-bit samples
        if len(pcm_data) % 2 != 0:
            pcm_data = pcm_data[:-1]  # Truncate last byte
            if not pcm_data:
                return 0.0

        # Convert bytes to 16-bit integers
        samples = struct.unpack(f"{len(pcm_data)//2}h", pcm_data)

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
            return b""

        # Concatenate all PCM frames
        pcm_data = b"".join(self.speech_buffer)

        # Convert to WAV
        return self._pcm_to_wav(pcm_data, self.sample_rate, self.channels)

    @staticmethod
    def _pcm_to_wav(
        pcm_data: bytes,
        sample_rate: int,
        channels: int,
        bits_per_sample: int = 16,
    ) -> bytes:
        """Convert raw PCM data to WAV format"""
        data_size = len(pcm_data)

        # Build WAV header
        header = struct.pack(
            "<4sI4s4sIHHIIHH4sI",
            b"RIFF",  # Chunk ID
            36 + data_size,  # Chunk size
            b"WAVE",  # Format
            b"fmt ",  # Subchunk1 ID
            16,  # Subchunk1 size (PCM)
            1,  # Audio format (1 = PCM)
            channels,  # Number of channels
            sample_rate,  # Sample rate
            sample_rate * channels * bits_per_sample // 8,  # Byte rate
            channels * bits_per_sample // 8,  # Block align
            bits_per_sample,  # Bits per sample
            b"data",  # Subchunk2 ID
            data_size,  # Subchunk2 size
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
                    "Content-Type": "application/json",
                }
            )

    async def close(self):
        """Close the aiohttp session"""
        if self.session:
            await self.session.close()
            self.session = None

    async def create_session(
        self,
        user_label: str = "anonymous",
        date_of_birth: str = "1990-01-01",
        birth_sex: str = "UNSPECIFIED",
        locale: str = "en-US",
    ) -> dict:
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
                "birthSex": birth_sex,
            },
            "language": locale,
        }

        async with self.session.post(
            f"{self.base_url}/v1/models/mental-wellness", json=payload
        ) as response:
            if response.status not in (200, 201):
                error_text = await response.text()
                raise ThymiaAPIError(
                    f"Failed to create session: {response.status} - {error_text}"
                )

            return await response.json()

    async def upload_audio(self, upload_url: str, wav_data: bytes) -> bool:
        """Upload WAV audio file to presigned S3 URL"""
        await self._ensure_session()

        async with self.session.put(
            upload_url, data=wav_data, headers={"Content-Type": "audio/wav"}
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
                raise ThymiaAPIError(
                    f"Failed to get results: {response.status} - {error_text}"
                )

            data = await response.json()

            # Check if analysis is complete
            status = data.get("status", "")
            if status in ("COMPLETE_OK", "COMPLETE_ERROR", "FAILED"):
                return data

            return None

    async def poll_results(
        self,
        session_id: str,
        max_wait_seconds: int = 120,
        poll_interval: int = 5,
    ) -> Optional[dict]:
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

        # DEBUG: Write to file to verify Python is running
        import sys

        try:
            with open(
                "/tmp/thymia_extension_init.log", "a", encoding="utf-8"
            ) as f:
                f.write(
                    f"[INIT] ThymiaAnalyzerExtension __init__ called at {time.time()}\n"
                )
                f.write(f"[INIT] Python version: {sys.version}\n")
                f.write(f"[INIT] stdout: {sys.stdout}, stderr: {sys.stderr}\n")
                f.flush()
            # Try stdout too
            print(
                f"[THYMIA_INIT] Extension initializing at {time.time()}",
                flush=True,
            )
            sys.stdout.flush()
            sys.stderr.write(
                f"[THYMIA_INIT_STDERR] Extension initializing at {time.time()}\n"
            )
            sys.stderr.flush()
        except Exception:
            pass  # Silently fail if logging fails

        # Configuration
        self.api_key: str = ""
        self.min_speech_duration: float = 30.0
        self.silence_threshold: float = 0.02
        self.continuous_analysis: bool = True
        self.min_interval_seconds: int = 60
        self.max_analyses_per_session: int = 10
        self.poll_timeout: int = 120
        self.poll_interval: int = 5

        # Analysis mode configuration (for backwards compatibility)
        self.analysis_mode: str = "hellos_only"  # "hellos_only" or "demo_dual"
        self.apollo_mood_duration: float = 22.0  # Duration of mood audio for Apollo
        self.apollo_read_duration: float = 30.0  # Duration of reading audio for Apollo

        # State
        self.audio_buffer: Optional[AudioBuffer] = None
        self.api_client: Optional[ThymiaAPIClient] = None
        self.apollo_client: Optional[ApolloAPI] = None
        self.latest_results: Optional[WellnessMetrics] = None
        self.apollo_results: Optional[ApolloResult] = None
        self.active_analysis: bool = False
        self.analysis_count: int = 0
        self.last_analysis_time: float = 0.0

        # Phased analysis state (demo_dual mode only)
        self.hellos_complete: bool = False
        self.apollo_complete: bool = False
        self.hellos_analysis_running: bool = False
        self.apollo_analysis_running: bool = False

        # User information for Thymia API
        self.user_name: Optional[str] = None
        self.user_dob: Optional[str] = None
        self.user_sex: Optional[str] = None
        self.user_locale: str = "en-US"

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        """Called when extension starts"""
        # DEBUG: Write to file to verify on_start is called
        import sys

        try:
            with open(
                "/tmp/thymia_extension_on_start.log", "a", encoding="utf-8"
            ) as f:
                f.write(f"[ON_START] on_start called at {time.time()}\n")
                f.flush()
            print(
                f"[THYMIA_ON_START] on_start called at {time.time()}",
                flush=True,
            )
            sys.stdout.flush()
        except Exception:
            pass

        ten_env.log_info("ThymiaAnalyzerExtension starting...")

        # Load configuration
        try:
            # TEN Framework returns tuples (value, error) - extract first element for ALL property types
            api_key_result = await ten_env.get_property_string("api_key")
            self.api_key = (
                api_key_result[0]
                if isinstance(api_key_result, tuple)
                else api_key_result
            )
            min_speech_result = await ten_env.get_property_float(
                "min_speech_duration"
            )
            self.min_speech_duration = (
                min_speech_result[0]
                if isinstance(min_speech_result, tuple)
                else min_speech_result
            )

            silence_result = await ten_env.get_property_float(
                "silence_threshold"
            )
            self.silence_threshold = (
                silence_result[0]
                if isinstance(silence_result, tuple)
                else silence_result
            )

            self.continuous_analysis = await ten_env.get_property_bool(
                "continuous_analysis"
            )

            # TEN Framework returns tuples (value, error) for int properties too
            min_interval_result = await ten_env.get_property_int(
                "min_interval_seconds"
            )
            self.min_interval_seconds = (
                min_interval_result[0]
                if isinstance(min_interval_result, tuple)
                else min_interval_result
            )

            max_analyses_result = await ten_env.get_property_int(
                "max_analyses_per_session"
            )
            self.max_analyses_per_session = (
                max_analyses_result[0]
                if isinstance(max_analyses_result, tuple)
                else max_analyses_result
            )

            poll_timeout_result = await ten_env.get_property_int("poll_timeout")
            self.poll_timeout = (
                poll_timeout_result[0]
                if isinstance(poll_timeout_result, tuple)
                else poll_timeout_result
            )

            poll_interval_result = await ten_env.get_property_int(
                "poll_interval"
            )
            self.poll_interval = (
                poll_interval_result[0]
                if isinstance(poll_interval_result, tuple)
                else poll_interval_result
            )

            # Load analysis mode (defaults to hellos_only for backwards compatibility)
            try:
                analysis_mode_result = await ten_env.get_property_string(
                    "analysis_mode"
                )
                self.analysis_mode = (
                    analysis_mode_result[0]
                    if isinstance(analysis_mode_result, tuple)
                    else analysis_mode_result
                )
            except Exception:
                # Property not set, use default
                self.analysis_mode = "hellos_only"

            # Load Apollo-specific durations if specified
            try:
                apollo_mood_result = await ten_env.get_property_float(
                    "apollo_mood_duration"
                )
                self.apollo_mood_duration = (
                    apollo_mood_result[0]
                    if isinstance(apollo_mood_result, tuple)
                    else apollo_mood_result
                )
            except Exception:
                pass  # Use default

            try:
                apollo_read_result = await ten_env.get_property_float(
                    "apollo_read_duration"
                )
                self.apollo_read_duration = (
                    apollo_read_result[0]
                    if isinstance(apollo_read_result, tuple)
                    else apollo_read_result
                )
            except Exception:
                pass  # Use default

            ten_env.log_info(
                f"Loaded config: analysis_mode={self.analysis_mode}, "
                f"silence_threshold={self.silence_threshold}, "
                f"min_speech_duration={self.min_speech_duration}"
            )
        except Exception as e:
            ten_env.log_warn(
                f"Failed to load some properties, using defaults: {e}"
            )

        # Validate API key
        if not self.api_key:
            ten_env.log_error(
                "Thymia API key not configured - extension will be disabled"
            )
            await super().on_start(ten_env)
            return

        # Initialize components
        self.audio_buffer = AudioBuffer(
            sample_rate=16000,
            channels=1,
            silence_threshold=self.silence_threshold,
        )
        self.api_client = ThymiaAPIClient(api_key=self.api_key)

        # Initialize Apollo client if in demo_dual mode
        if self.analysis_mode == "demo_dual":
            self.apollo_client = ApolloAPI(api_key=self.api_key)
            ten_env.log_info(
                f"ThymiaAnalyzerExtension started in DEMO_DUAL mode "
                f"(Hellos + Apollo, mood={self.apollo_mood_duration}s, "
                f"read={self.apollo_read_duration}s)"
            )
        else:
            ten_env.log_info(
                f"ThymiaAnalyzerExtension started in HELLOS_ONLY mode "
                f"(min_speech={self.min_speech_duration}s)"
            )

        # Register as LLM tool (parent class handles this)
        await super().on_start(ten_env)

        # Log tool registration
        tools = self.get_tool_metadata(ten_env)
        tool_names = [t.name for t in tools]
        ten_env.log_info(
            f"[TOOL_REGISTRATION] Registered {len(tools)} tools: {', '.join(tool_names)}"
        )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        """Called when extension stops"""
        ten_env.log_info("ThymiaAnalyzerExtension stopping...")

        if self.api_client:
            await self.api_client.close()

        if self.apollo_client:
            await self.apollo_client.close()

        await super().on_stop(ten_env)

    # Counter for logging
    _audio_frame_count = 0

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        """Process incoming audio frames"""
        try:
            # Log every 50th frame to confirm audio is being received
            self._audio_frame_count += 1
            if self._audio_frame_count % 50 == 1:
                print(
                    f"[THYMIA_AUDIO] ✅ Received audio frame #{self._audio_frame_count}",
                    flush=True,
                )
                ten_env.log_info(
                    f"[thymia_analyzer] ✅ Received audio frame #{self._audio_frame_count}"
                )

            if not self.audio_buffer:
                ten_env.log_warn(
                    "[thymia_analyzer] Audio buffer not initialized"
                )
                return

            if not self.api_client:
                ten_env.log_warn("[thymia_analyzer] API client not initialized")
                return

            # Get PCM data from audio frame
            buf = audio_frame.lock_buf()
            pcm_data = bytes(buf)
            audio_frame.unlock_buf(buf)

            if self._audio_frame_count % 50 == 1:
                ten_env.log_info(
                    f"[thymia_analyzer] PCM data size: {len(pcm_data)} bytes"
                )

            # Add to buffer with VAD
            speech_duration = self.audio_buffer.add_frame(pcm_data)

            # === SEPARATE LOGIC FOR EACH MODE ===

            if self.analysis_mode == "hellos_only":
                # ============ HELLOS_ONLY MODE ============
                required_duration = self.min_speech_duration

                if self._audio_frame_count % 50 == 1:
                    ten_env.log_info(
                        f"[thymia_analyzer] 📊 Speech buffer status: {speech_duration:.1f}s / {required_duration:.1f}s "
                        f"(mode=hellos_only, need {max(0, required_duration - speech_duration):.1f}s more)"
                    )

                # Check if we have enough speech to analyze
                if self.audio_buffer.has_enough_speech(required_duration):
                    should_analyze = (
                        not self.active_analysis
                        and self.analysis_count < self.max_analyses_per_session
                        and (time.time() - self.last_analysis_time)
                        >= self.min_interval_seconds
                    )

                    if should_analyze:
                        # Validate user info before starting
                        if not self.user_name or not self.user_dob or not self.user_sex:
                            if self._audio_frame_count % 100 == 1:  # Log every 1 second
                                ten_env.log_warn(
                                    f"[thymia_analyzer] ⚠️ Waiting for user info before analysis "
                                    f"(have: name={self.user_name}, dob={self.user_dob}, sex={self.user_sex})"
                                )
                        else:
                            ten_env.log_info(
                                f"[thymia_analyzer] 🚀 Starting Hellos analysis "
                                f"({speech_duration:.1f}s speech collected)"
                            )
                            asyncio.create_task(self._run_hellos_only_analysis(ten_env))

            elif self.analysis_mode == "demo_dual":
                # ============ DEMO_DUAL MODE (PHASED) ============
                # Phase 1: Hellos at 22s
                # Phase 2: Apollo at 44s (uses 0-22s for mood, 22-44s for read)

                if not self.hellos_complete:
                    # Waiting for 22s to run Hellos
                    required_duration = self.min_speech_duration  # 22s
                    phase = "hellos (1/2)"

                    if self._audio_frame_count % 50 == 1:
                        ten_env.log_info(
                            f"[thymia_analyzer] 📊 Speech buffer status: {speech_duration:.1f}s / {required_duration:.1f}s "
                            f"(mode=demo_dual, phase={phase}, need {max(0, required_duration - speech_duration):.1f}s more)"
                        )

                    if self.audio_buffer.has_enough_speech(required_duration) and not self.hellos_analysis_running:
                        # Validate user info before starting
                        if not self.user_name or not self.user_dob or not self.user_sex:
                            if self._audio_frame_count % 100 == 1:  # Log every 1 second
                                ten_env.log_warn(
                                    f"[thymia_analyzer] ⚠️ Waiting for user info before analysis "
                                    f"(have: name={self.user_name}, dob={self.user_dob}, sex={self.user_sex})"
                                )
                        else:
                            ten_env.log_info(
                                f"[thymia_analyzer] 🚀 Starting Hellos analysis (phase 1/2) "
                                f"({speech_duration:.1f}s speech collected)"
                            )
                            self.hellos_analysis_running = True
                            asyncio.create_task(self._run_hellos_phase(ten_env))

                elif not self.apollo_complete:
                    # Waiting for 44s to run Apollo
                    required_duration = self.apollo_mood_duration + self.apollo_read_duration  # 44s
                    phase = "apollo (2/2)"

                    if self._audio_frame_count % 50 == 1:
                        ten_env.log_info(
                            f"[thymia_analyzer] 📊 Speech buffer status: {speech_duration:.1f}s / {required_duration:.1f}s "
                            f"(mode=demo_dual, phase={phase}, need {max(0, required_duration - speech_duration):.1f}s more)"
                        )

                    if self.audio_buffer.has_enough_speech(required_duration) and not self.apollo_analysis_running:
                        ten_env.log_info(
                            f"[thymia_analyzer] 🚀 Starting Apollo analysis (phase 2/2) "
                            f"({speech_duration:.1f}s speech collected)"
                        )
                        self.apollo_analysis_running = True
                        asyncio.create_task(self._run_apollo_phase(ten_env))

                else:
                    # Both phases complete
                    if self._audio_frame_count % 50 == 1:
                        ten_env.log_info(
                            f"[thymia_analyzer] 📊 Speech buffer status: {speech_duration:.1f}s "
                            f"(mode=demo_dual, phase=complete, all analysis done)"
                        )
        except Exception as e:
            import traceback

            ten_env.log_error(
                f"[thymia_analyzer] ❌ Error in on_audio_frame: {e}\n{traceback.format_exc()}"
            )

    def _split_pcm_by_duration(
        self, pcm_data: bytes, split_duration: float, sample_rate: int = 16000
    ) -> tuple[bytes, bytes]:
        """
        Split PCM data at a specific duration point.

        Args:
            pcm_data: Raw PCM audio data
            split_duration: Duration in seconds where to split
            sample_rate: Sample rate in Hz

        Returns:
            Tuple of (first_part, second_part)
        """
        # Calculate byte offset for split point
        # 16-bit samples = 2 bytes per sample, mono = 1 channel
        bytes_per_second = sample_rate * 2
        split_byte_offset = int(split_duration * bytes_per_second)

        # Ensure we don't exceed data length
        split_byte_offset = min(split_byte_offset, len(pcm_data))

        first_part = pcm_data[:split_byte_offset]
        second_part = pcm_data[split_byte_offset:]

        return first_part, second_part

    async def _run_hellos_only_analysis(self, ten_env: AsyncTenEnv):
        """Run Hellos analysis for hellos_only mode"""
        self.active_analysis = True

        ten_env.log_info(
            f"[HELLOS_ONLY] Starting analysis - "
            f"User={self.user_name}, DOB={self.user_dob}, Sex={self.user_sex}"
        )

        try:
            wav_data = self.audio_buffer.get_wav_data()
            if not wav_data:
                ten_env.log_warn("[HELLOS_ONLY] No audio data available")
                return

            ten_env.log_info(f"[HELLOS_ONLY] Starting API workflow ({len(wav_data)} bytes)")

            # Create session
            session_response = await self.api_client.create_session(
                user_label=self.user_name or "anonymous",
                date_of_birth=self.user_dob or "1990-01-01",
                birth_sex=self.user_sex or "UNSPECIFIED",
                locale=self.user_locale,
            )
            session_id = session_response["id"]
            upload_url = session_response["recordingUploadUrl"]

            ten_env.log_info(f"[HELLOS_ONLY] Created session: {session_id}")

            # Upload audio
            upload_success = await self.api_client.upload_audio(upload_url, wav_data)
            if not upload_success:
                ten_env.log_error("[HELLOS_ONLY] Failed to upload audio")
                return

            # Poll for results
            results = await self.api_client.poll_results(
                session_id,
                max_wait_seconds=self.poll_timeout,
                poll_interval=self.poll_interval,
            )

            if not results:
                ten_env.log_warn(f"[HELLOS_ONLY] Analysis timed out after {self.poll_timeout}s")
                return

            # Extract metrics
            sections = results.get("results", {}).get("sections", [])
            if not sections:
                ten_env.log_error("[HELLOS_ONLY] No sections found in response")
                return

            section = sections[0]
            self.latest_results = WellnessMetrics(
                distress=section.get("uniformDistress", {}).get("value", 0.0),
                stress=section.get("uniformStress", {}).get("value", 0.0),
                burnout=section.get("uniformExhaustion", {}).get("value", 0.0),
                fatigue=section.get("uniformSleepPropensity", {}).get("value", 0.0),
                low_self_esteem=section.get("uniformLowSelfEsteem", {}).get("value", 0.0),
                timestamp=time.time(),
                session_id=session_id,
            )

            self.analysis_count += 1
            self.last_analysis_time = time.time()

            ten_env.log_info(
                f"[HELLOS_ONLY] Analysis complete: "
                f"distress={self.latest_results.distress:.4f}, "
                f"stress={self.latest_results.stress:.4f}, "
                f"burnout={self.latest_results.burnout:.4f}, "
                f"fatigue={self.latest_results.fatigue:.4f}, "
                f"low_self_esteem={self.latest_results.low_self_esteem:.4f}"
            )

        except Exception as e:
            ten_env.log_error(f"[HELLOS_ONLY] Error: {e}")
            import traceback
            ten_env.log_error(traceback.format_exc())
        finally:
            self.active_analysis = False

    async def _run_hellos_phase(self, ten_env: AsyncTenEnv):
        """Run Hellos analysis for demo_dual mode (phase 1/2)"""
        ten_env.log_info(
            f"[HELLOS PHASE 1/2] Starting analysis - "
            f"User={self.user_name}, DOB={self.user_dob}, Sex={self.user_sex}"
        )

        try:
            wav_data = self.audio_buffer.get_wav_data()
            if not wav_data:
                ten_env.log_warn("[HELLOS PHASE 1/2] No audio data available")
                return

            ten_env.log_info(f"[HELLOS PHASE 1/2] Starting API workflow ({len(wav_data)} bytes)")

            # Create session
            session_response = await self.api_client.create_session(
                user_label=self.user_name or "anonymous",
                date_of_birth=self.user_dob or "1990-01-01",
                birth_sex=self.user_sex or "UNSPECIFIED",
                locale=self.user_locale,
            )
            session_id = session_response["id"]
            upload_url = session_response["recordingUploadUrl"]

            ten_env.log_info(f"[HELLOS PHASE 1/2] Created session: {session_id}")

            # Upload audio
            upload_success = await self.api_client.upload_audio(upload_url, wav_data)
            if not upload_success:
                ten_env.log_error("[HELLOS PHASE 1/2] Failed to upload audio")
                return

            # Poll for results
            results = await self.api_client.poll_results(
                session_id,
                max_wait_seconds=self.poll_timeout,
                poll_interval=self.poll_interval,
            )

            if not results:
                ten_env.log_warn(f"[HELLOS PHASE 1/2] Analysis timed out after {self.poll_timeout}s")
                return

            # Extract metrics
            sections = results.get("results", {}).get("sections", [])
            if not sections:
                ten_env.log_error("[HELLOS PHASE 1/2] No sections found in response")
                return

            section = sections[0]
            self.latest_results = WellnessMetrics(
                distress=section.get("uniformDistress", {}).get("value", 0.0),
                stress=section.get("uniformStress", {}).get("value", 0.0),
                burnout=section.get("uniformExhaustion", {}).get("value", 0.0),
                fatigue=section.get("uniformSleepPropensity", {}).get("value", 0.0),
                low_self_esteem=section.get("uniformLowSelfEsteem", {}).get("value", 0.0),
                timestamp=time.time(),
                session_id=session_id,
            )

            ten_env.log_info(
                f"[HELLOS PHASE 1/2] Complete: "
                f"distress={self.latest_results.distress:.4f}, "
                f"stress={self.latest_results.stress:.4f}, "
                f"burnout={self.latest_results.burnout:.4f}, "
                f"fatigue={self.latest_results.fatigue:.4f}, "
                f"low_self_esteem={self.latest_results.low_self_esteem:.4f}"
            )

            # Mark Hellos phase complete
            self.hellos_complete = True

        except Exception as e:
            ten_env.log_error(f"[HELLOS PHASE 1/2] Error: {e}")
            import traceback
            ten_env.log_error(traceback.format_exc())
        finally:
            self.hellos_analysis_running = False

    async def _run_apollo_phase(self, ten_env: AsyncTenEnv):
        """Run Apollo analysis for demo_dual mode (phase 2/2)"""
        ten_env.log_info(
            f"[APOLLO PHASE 2/2] Starting analysis - "
            f"User={self.user_name}, DOB={self.user_dob}, Sex={self.user_sex}"
        )

        try:
            # Get full PCM data
            if not self.audio_buffer.speech_buffer:
                ten_env.log_warn("[APOLLO PHASE 2/2] No audio data available")
                return

            full_pcm_data = b"".join(self.audio_buffer.speech_buffer)

            # Split audio into mood (first 22s) and read (next 22s)
            mood_pcm, read_pcm = self._split_pcm_by_duration(
                full_pcm_data, self.apollo_mood_duration
            )

            ten_env.log_info(
                f"[APOLLO PHASE 2/2] Split audio: mood={len(mood_pcm)} bytes "
                f"({self.apollo_mood_duration}s), read={len(read_pcm)} bytes"
            )

            # Call Apollo API
            apollo_result = await self.apollo_client.analyze(
                mood_audio_pcm=mood_pcm,
                read_aloud_audio_pcm=read_pcm,
                user_label=self.user_name or "anonymous",
                date_of_birth=self.user_dob or "1990-01-01",
                birth_sex=self.user_sex or "OTHER",
                sample_rate=16000,
                language="en-GB",
            )

            # Store Apollo results
            self.apollo_results = apollo_result

            if apollo_result.status == "COMPLETE_OK":
                ten_env.log_info(
                    f"[APOLLO PHASE 2/2] Complete: "
                    f"depression={apollo_result.depression_probability:.2%} "
                    f"({apollo_result.depression_severity}), "
                    f"anxiety={apollo_result.anxiety_probability:.2%} "
                    f"({apollo_result.anxiety_severity})"
                )
            else:
                ten_env.log_warn(
                    f"[APOLLO PHASE 2/2] Failed: {apollo_result.status} - "
                    f"{apollo_result.error_message}"
                )

            # Mark Apollo phase complete
            self.apollo_complete = True

        except Exception as e:
            ten_env.log_error(f"[APOLLO PHASE 2/2] Error: {e}")
            import traceback
            ten_env.log_error(traceback.format_exc())
        finally:
            self.apollo_analysis_running = False

    async def _run_analysis(self, ten_env: AsyncTenEnv):
        """Run Thymia analysis workflow in background (DEPRECATED - use specific methods)"""
        self.active_analysis = True

        ten_env.log_info(
            f"[ANALYSIS START] Mode={self.analysis_mode}, "
            f"User={self.user_name}, DOB={self.user_dob}, Sex={self.user_sex}"
        )

        try:
            # Get raw PCM data from buffer
            if not self.audio_buffer.speech_buffer:
                ten_env.log_warn("[ANALYSIS] No audio data available for analysis")
                return

            # Concatenate all PCM frames
            full_pcm_data = b"".join(self.audio_buffer.speech_buffer)

            # Get WAV data for Hellos API (existing behavior)
            wav_data = self.audio_buffer.get_wav_data()

            if not wav_data:
                ten_env.log_warn("No audio data available for analysis")
                return

            # Debug: Uncomment to save WAV files (WARNING: can fill disk quickly!)
            # timestamp = int(time.time())
            # wav_filename = f"/tmp/thymia_audio_{timestamp}_{self.user_name or 'unknown'}.wav"
            # try:
            #     with open(wav_filename, "wb") as f:
            #         f.write(wav_data)
            #     ten_env.log_info(f"Saved audio for debugging: {wav_filename}")
            # except Exception as e:
            #     ten_env.log_warn(f"Failed to save debug audio: {e}")

            ten_env.log_info(
                f"[HELLOS] Starting Thymia API workflow ({len(wav_data)} bytes)"
            )

            # Step 1: Create session with user info
            ten_env.log_info(
                f"[HELLOS] Creating session for user={self.user_name}, "
                f"dob={self.user_dob}, sex={self.user_sex}"
            )
            session_response = await self.api_client.create_session(
                user_label=self.user_name or "anonymous",
                date_of_birth=self.user_dob or "1990-01-01",
                birth_sex=self.user_sex or "UNSPECIFIED",
                locale=self.user_locale,
            )
            session_id = session_response["id"]
            upload_url = session_response["recordingUploadUrl"]

            ten_env.log_info(f"[HELLOS] Created session: {session_id}")

            # Step 2: Upload audio
            ten_env.log_info(f"[HELLOS] Uploading {len(wav_data)} bytes of audio...")
            upload_success = await self.api_client.upload_audio(
                upload_url, wav_data
            )

            if not upload_success:
                ten_env.log_error("[HELLOS] Failed to upload audio to Thymia")
                return

            ten_env.log_info(
                f"[HELLOS] Audio uploaded successfully, polling for results (timeout={self.poll_timeout}s)..."
            )

            # Step 3: Poll for results
            results = await self.api_client.poll_results(
                session_id,
                max_wait_seconds=self.poll_timeout,
                poll_interval=self.poll_interval,
            )

            if not results:
                ten_env.log_warn(
                    f"[HELLOS] Analysis timed out after {self.poll_timeout}s"
                )
                return

            ten_env.log_info(f"[HELLOS] Received results for session {session_id}")

            # Step 4: Extract metrics from results.sections[0]
            sections = results.get("results", {}).get("sections", [])
            if not sections:
                ten_env.log_error("No sections found in Thymia response")
                return

            section = sections[0]

            # Log RAW API response for debugging
            ten_env.log_info(
                f"[THYMIA_RAW_API_RESPONSE] Full section data: {json.dumps(section, indent=2)}"
            )

            # Extract individual metrics with detailed logging
            distress_val = section.get("uniformDistress", {}).get("value", 0.0)
            stress_val = section.get("uniformStress", {}).get("value", 0.0)
            burnout_val = section.get("uniformExhaustion", {}).get("value", 0.0)
            fatigue_val = section.get("uniformSleepPropensity", {}).get(
                "value", 0.0
            )
            low_self_esteem_val = section.get("uniformLowSelfEsteem", {}).get(
                "value", 0.0
            )

            ten_env.log_info(
                f"[THYMIA_PARSED_VALUES] "
                f"distress={distress_val}, "
                f"stress={stress_val}, "
                f"burnout={burnout_val}, "
                f"fatigue={fatigue_val}, "
                f"low_self_esteem={low_self_esteem_val}"
            )

            self.latest_results = WellnessMetrics(
                distress=distress_val,
                stress=stress_val,
                burnout=burnout_val,
                fatigue=fatigue_val,
                low_self_esteem=low_self_esteem_val,
                timestamp=time.time(),
                session_id=session_id,
            )

            self.analysis_count += 1
            self.last_analysis_time = time.time()

            ten_env.log_info(
                f"Hellos wellness analysis complete: "
                f"distress={self.latest_results.distress:.4f}, "
                f"stress={self.latest_results.stress:.4f}, "
                f"burnout={self.latest_results.burnout:.4f}, "
                f"fatigue={self.latest_results.fatigue:.4f}, "
                f"low_self_esteem={self.latest_results.low_self_esteem:.4f}"
            )

            # Run Apollo API in demo_dual mode
            if self.analysis_mode == "demo_dual" and self.apollo_client:
                ten_env.log_info(
                    "[APOLLO] Starting Apollo API analysis for depression/anxiety"
                )

                try:
                    # Split audio into mood and read segments
                    mood_pcm, read_pcm = self._split_pcm_by_duration(
                        full_pcm_data, self.apollo_mood_duration
                    )

                    ten_env.log_info(
                        f"[APOLLO] Split audio: mood={len(mood_pcm)} bytes "
                        f"({self.apollo_mood_duration}s), "
                        f"read={len(read_pcm)} bytes"
                    )

                    # Validate user info for Apollo
                    if not self.user_dob or not self.user_sex:
                        ten_env.log_warn(
                            "[APOLLO] Missing user DOB or sex, using defaults"
                        )

                    # Run Apollo analysis
                    ten_env.log_info(
                        f"[APOLLO] Starting depression/anxiety analysis for user={self.user_name}, "
                        f"dob={self.user_dob}, sex={self.user_sex}"
                    )
                    apollo_result = await self.apollo_client.analyze(
                        mood_audio_pcm=mood_pcm,
                        read_aloud_audio_pcm=read_pcm,
                        user_label=self.user_name or "anonymous",
                        date_of_birth=self.user_dob or "1990-01-01",
                        birth_sex=self.user_sex or "OTHER",
                        sample_rate=16000,
                        language="en-GB",
                    )

                    # Store Apollo results
                    self.apollo_results = apollo_result

                    if apollo_result.status == "COMPLETE_OK":
                        ten_env.log_info(
                            f"[APOLLO] Analysis complete: "
                            f"depression={apollo_result.depression_probability:.2%} "
                            f"({apollo_result.depression_severity}), "
                            f"anxiety={apollo_result.anxiety_probability:.2%} "
                            f"({apollo_result.anxiety_severity})"
                        )
                    else:
                        ten_env.log_warn(
                            f"[APOLLO] Analysis failed: {apollo_result.status} - "
                            f"{apollo_result.error_message}"
                        )

                except Exception as apollo_error:
                    ten_env.log_error(
                        f"[APOLLO] Apollo API failed: {apollo_error}"
                    )
                    # Continue with Hellos-only results

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
                "WELLNESS ANALYSIS COMPLETE: The user's wellness metrics are now available. "
                "Call get_wellness_metrics to retrieve and present them."
            )

            notification.set_property_string("text", message)

            # Send to main_control which will inject into LLM context
            await ten_env.send_data(notification)

            ten_env.log_info(
                "[thymia_analyzer] Sent wellness notification to LLM"
            )

        except Exception as e:
            ten_env.log_error(
                f"[thymia_analyzer] Failed to send notification: {e}"
            )

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        """Register wellness analysis tools"""
        ten_env.log_info("[TOOL_METADATA] get_tool_metadata called - defining set_user_info and get_wellness_metrics tools")
        return [
            LLMToolMetadata(
                name="set_user_info",
                description=(
                    "Set user information required for mental wellness analysis. "
                    "This should be called BEFORE analyzing wellness metrics. "
                    "Ask the user naturally for their name, year of birth, and sex. "
                    "Example: 'To better understand your wellness, may I ask your name, year of birth, and sex?'"
                ),
                parameters=[
                    {
                        "name": "name",
                        "type": "string",
                        "description": "User's first name or identifier",
                        "required": True,
                    },
                    {
                        "name": "year_of_birth",
                        "type": "string",
                        "description": "User's year of birth (e.g., '1974', '1990')",
                        "required": True,
                    },
                    {
                        "name": "sex",
                        "type": "string",
                        "description": "Biological sex: MALE, FEMALE, or OTHER",
                        "required": True,
                    },
                    {
                        "name": "locale",
                        "type": "string",
                        "description": "Locale/language code (e.g., en-US, en-GB). Defaults to en-US if not provided.",
                        "required": False,
                    },
                ],
            ),
            LLMToolMetadata(
                name="get_wellness_metrics",
                description=(
                    "Get user's mental wellness and clinical indicators from voice analysis. "
                    "Returns wellness metrics (stress, distress, burnout, fatigue, low_self_esteem) as PERCENTAGES from 0-100. "
                    "May also return clinical indicators (depression, anxiety) if available, as probabilities from 0-100 with severity levels. "
                    "Values are integers (e.g., stress: 27%, depression: 15%). "
                    "The analysis is based on speech patterns and provides insight into the user's emotional and mental health state. "
                    "IMPORTANT: Call this periodically to check if analysis has completed. "
                    "When status='insufficient_data', the message field contains progress info - only share this if user asks about progress. "
                    "When status='available', announce ALL results immediately with percentages. "
                    "Frame clinical indicators (depression/anxiety) as research indicators, not clinical diagnosis. "
                    "NOTE: User information must be set first using set_user_info."
                ),
                parameters=[],
            ),
        ]

    def _parse_date_to_iso(self, date_str: str) -> str:
        """Convert various date formats to YYYY-MM-DD format required by Thymia API"""
        if not date_str:
            return "1990-01-01"

        # Try common date formats
        date_formats = [
            "%Y-%m-%d",  # Already in correct format
            "%B %d, %Y",  # April 27, 1974
            "%b %d, %Y",  # Apr 27, 1974
            "%m/%d/%Y",  # 04/27/1974
            "%d/%m/%Y",  # 27/04/1974
            "%Y/%m/%d",  # 1974/04/27
            "%d-%m-%Y",  # 27-04-1974
            "%m-%d-%Y",  # 04-27-1974
        ]

        for fmt in date_formats:
            try:
                parsed_date = datetime.strptime(date_str, fmt)
                return parsed_date.strftime("%Y-%m-%d")
            except ValueError:
                continue

        # If all parsing fails, return default
        return "1990-01-01"

    async def run_tool(
        self, ten_env: AsyncTenEnv, name: str, args: dict
    ) -> LLMToolResult:
        """Handle LLM tool calls for wellness analysis"""
        ten_env.log_info(f"LLM called tool: {name}")

        try:
            # Handle set_user_info tool
            if name == "set_user_info":
                # Extract parameters
                self.user_name = args.get("name", "").strip()
                year_of_birth = args.get("year_of_birth", "").strip()
                self.user_sex = args.get("sex", "").strip().upper()
                self.user_locale = args.get("locale", "en-US").strip()

                # Convert year to YYYY-01-01 format (using Jan 1st)
                if year_of_birth:
                    # Extract just the 4-digit year
                    import re
                    year_match = re.search(r'(\d{4})', year_of_birth)
                    if year_match:
                        year = year_match.group(1)
                        self.user_dob = f"{year}-01-01"
                        ten_env.log_info(
                            f"Converted year '{year_of_birth}' to DOB '{self.user_dob}'"
                        )
                    else:
                        self.user_dob = "1990-01-01"  # Default fallback
                        ten_env.log_warn(
                            f"Could not parse year '{year_of_birth}', using default"
                        )
                else:
                    self.user_dob = ""

                # Validate required fields
                if not self.user_name or not self.user_dob or not self.user_sex:
                    return LLMToolResultLLMResult(
                        type="llmresult",
                        content=json.dumps(
                            {
                                "status": "error",
                                "message": "Missing required fields: name, year_of_birth, and sex are all required",
                            }
                        ),
                    )

                # Validate sex value
                if self.user_sex not in ["MALE", "FEMALE", "OTHER"]:
                    self.user_sex = "OTHER"

                ten_env.log_info(
                    f"User info set: {self.user_name}, {self.user_dob}, {self.user_sex}, {self.user_locale}"
                )

                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=json.dumps(
                        {
                            "status": "success",
                            "message": f"User information saved. Voice analysis will use: {self.user_name}, {self.user_dob}, {self.user_sex}",
                        }
                    ),
                )

            # Handle get_wellness_metrics tool
            if name == "get_wellness_metrics":
                ten_env.log_info(
                    f"[TOOL CALL] get_wellness_metrics - "
                    f"active_analysis={self.active_analysis}, "
                    f"has_results={self.latest_results is not None}, "
                    f"speech_duration={self.audio_buffer.speech_duration if self.audio_buffer else 0:.1f}s"
                )

                # Check if we have results
                if not self.latest_results:
                    # Calculate required duration based on mode
                    required_duration = self.min_speech_duration
                    if self.analysis_mode == "demo_dual":
                        required_duration = self.apollo_mood_duration + self.apollo_read_duration

                    # Determine status
                    if self.active_analysis:
                        status = "analyzing"
                        message = "Voice analysis in progress. Results will be available soon."
                    elif (
                        self.audio_buffer
                        and self.audio_buffer.speech_duration > 0
                    ):
                        status = "insufficient_data"
                        message = f"Collecting speech for analysis ({self.audio_buffer.speech_duration:.1f}s / {required_duration:.1f}s needed)"
                    else:
                        status = "no_data"
                        message = "No speech data collected yet for analysis"

                    return LLMToolResultLLMResult(
                        type="llmresult",
                        content=json.dumps(
                            {"status": status, "message": message}
                        ),
                    )

                # Return available metrics
                age_seconds = time.time() - self.latest_results.timestamp

                # Log RAW values before rounding
                ten_env.log_info(
                    f"[THYMIA_BEFORE_ROUNDING] "
                    f"distress={self.latest_results.distress}, "
                    f"stress={self.latest_results.stress}, "
                    f"burnout={self.latest_results.burnout}, "
                    f"fatigue={self.latest_results.fatigue}, "
                    f"low_self_esteem={self.latest_results.low_self_esteem}"
                )

                response_data = {
                    "status": "available",
                    "metrics": {
                        "distress": round(
                            round(self.latest_results.distress, 2) * 100
                        ),
                        "stress": round(
                            round(self.latest_results.stress, 2) * 100
                        ),
                        "burnout": round(
                            round(self.latest_results.burnout, 2) * 100
                        ),
                        "fatigue": round(
                            round(self.latest_results.fatigue, 2) * 100
                        ),
                        "low_self_esteem": round(
                            round(self.latest_results.low_self_esteem, 2) * 100
                        ),
                    },
                    "analyzed_seconds_ago": int(age_seconds),
                    "speech_duration": (
                        round(self.audio_buffer.speech_duration, 1)
                        if self.audio_buffer
                        else 0
                    ),
                }

                # Add Apollo clinical indicators if available
                if (
                    self.apollo_results
                    and self.apollo_results.status == "COMPLETE_OK"
                ):
                    response_data["clinical_indicators"] = {
                        "depression": {
                            "probability": round(
                                self.apollo_results.depression_probability * 100
                            ),
                            "severity": self.apollo_results.depression_severity,
                        },
                        "anxiety": {
                            "probability": round(
                                self.apollo_results.anxiety_probability * 100
                            ),
                            "severity": self.apollo_results.anxiety_severity,
                        },
                    }
                    ten_env.log_info(
                        f"[APOLLO_RETURNED_TO_LLM] Added clinical indicators: "
                        f"depression={response_data['clinical_indicators']['depression']}, "
                        f"anxiety={response_data['clinical_indicators']['anxiety']}"
                    )

                # Log what's being returned to LLM
                ten_env.log_info(
                    f"[THYMIA_RETURNED_TO_LLM] {json.dumps(response_data)}"
                )

                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=json.dumps(response_data),
                )

        except Exception as e:
            ten_env.log_error(f"Error in run_tool: {e}")
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps(
                    {
                        "status": "error",
                        "message": "Wellness analysis service temporarily unavailable",
                    }
                ),
            )
