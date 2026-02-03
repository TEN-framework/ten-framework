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
from collections import deque
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

# Sentinel WebSocket API imports
from .sentinel_client import SentinelClient
from .sentinel_protocol import (
    SentinelConfig,
    PolicyResult,
    StatusMessage,
    ErrorMessage,
    BiomarkerSummary,
)
from .result_mapper import (
    ResultMapper,
    WellnessMetricsCompat,
    SafetyClassification,
)

# Minimum time between any announcements (Hellos or Apollo)
ANNOUNCEMENT_MIN_SPACING_SECONDS = 15.0

# Phase duration settings (actual speech, not including padding/silence)
MOOD_PHASE_DURATION_SECONDS = (
    30.0  # Hellos API requires this much actual speech
)
READING_PHASE_DURATION_SECONDS = (
    30.0  # Additional actual speech for Apollo (total = mood + reading)
)


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
        self.speech_duration = (
            0.0  # Total duration including pre-speech padding
        )
        self.actual_speech_duration = (
            0.0  # Only frames where volume > threshold
        )
        self.max_speech_duration = 300.0  # 5 minutes safety limit

        # Circular buffer for 0.5 second of recent audio (pre-speech capture)
        # Using deque for O(1) popleft() performance
        self.circular_buffer = deque()
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

            # Trim circular buffer to 0.5 seconds (O(1) with deque)
            while (
                self._get_circular_buffer_duration()
                > self.circular_buffer_max_duration
            ):
                self.circular_buffer.popleft()

            # Check for speech onset
            if is_speech:
                # Speech detected! Add circular buffer contents (pre-speech context)
                for buffered_frame in self.circular_buffer:
                    if self.speech_duration < self.max_speech_duration:
                        self.speech_buffer.append(buffered_frame)
                        self.speech_duration += len(buffered_frame) / (
                            self.sample_rate * self.channels * 2
                        )
                        # NOTE: Pre-speech padding NOT counted in actual_speech_duration

                self.is_speaking = True
                self.circular_buffer.clear()  # Clear after using contents

        else:
            # Currently speaking
            if is_speech:
                # Continue speaking - add frame to speech buffer
                # Check max duration to prevent unbounded memory growth
                if self.speech_duration < self.max_speech_duration:
                    # Also flush any accumulated silence frames (they were part of speech)
                    for silence_frame in self.silence_frames:
                        self.speech_buffer.append(silence_frame)
                        self.speech_duration += len(silence_frame) / (
                            self.sample_rate * self.channels * 2
                        )
                        # NOTE: Brief silence during speech NOT counted in actual_speech_duration
                    self.silence_frames.clear()
                    self.silence_duration = 0.0

                    # Add current frame (THIS IS ACTUAL SPEECH)
                    self.speech_buffer.append(pcm_data)
                    self.speech_duration += frame_duration
                    self.actual_speech_duration += (
                        frame_duration  # Count actual speech!
                    )
                else:
                    # Buffer full - clear silence frames but don't add more audio
                    self.silence_frames.clear()
                    self.silence_duration = 0.0

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
                    self.circular_buffer = deque([pcm_data])

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
        """
        Check if we have enough ACTUAL speech for analysis.
        Uses actual_speech_duration (excludes pre-speech padding and silence).
        """
        return self.actual_speech_duration >= min_duration

    def clear_buffer(self):
        """Clear speech buffer after audio has been sent to APIs"""
        self.speech_buffer.clear()
        self.speech_duration = 0.0
        self.actual_speech_duration = 0.0
        print(
            "[THYMIA_BUFFER_CLEAR] Speech buffer cleared (all audio sent to APIs)",
            flush=True,
        )

    def get_wav_data(self, max_duration_seconds: float = None) -> bytes:
        """
        Convert buffered PCM data to WAV format.

        Args:
            max_duration_seconds: Maximum duration in seconds to extract. If None, extracts all.
                                 If specified, extracts only the first N seconds of speech.
        """
        start_time = time.time()

        print(
            f"[THYMIA_BUFFER_GET_WAV] get_wav_data() called - buffer has {len(self.speech_buffer)} frames, "
            f"actual_speech={self.actual_speech_duration:.1f}s, total_with_padding={self.speech_duration:.1f}s, "
            f"max_duration={max_duration_seconds}",
            flush=True,
        )

        if not self.speech_buffer:
            print(
                "[THYMIA_BUFFER_GET_WAV] Empty buffer, returning empty bytes",
                flush=True,
            )
            return b""

        # Determine how many frames to extract
        if max_duration_seconds is not None:
            # Calculate frames needed for the requested duration
            # Each frame is 10ms (320 bytes at 16kHz mono 16-bit)
            frames_needed = int(
                max_duration_seconds * 100
            )  # 100 frames per second
            frames_to_use = min(frames_needed, len(self.speech_buffer))
            frames_list = self.speech_buffer[:frames_to_use]
            print(
                f"[THYMIA_BUFFER_GET_WAV] Limiting to {frames_to_use} frames (~{frames_to_use/100.0:.1f}s) out of {len(self.speech_buffer)} available",
                flush=True,
            )
        else:
            frames_list = self.speech_buffer
            print(
                f"[THYMIA_BUFFER_GET_WAV] Using all {len(self.speech_buffer)} frames",
                flush=True,
            )

        # Concatenate PCM frames
        print(
            f"[THYMIA_BUFFER_GET_WAV] Starting to concatenate {len(frames_list)} frames...",
            flush=True,
        )
        pcm_data = b"".join(frames_list)
        concat_time = time.time() - start_time
        print(
            f"[THYMIA_BUFFER_GET_WAV] Concatenation took {concat_time:.3f}s, total PCM: {len(pcm_data)} bytes",
            flush=True,
        )

        # Convert to WAV
        print("[THYMIA_BUFFER_GET_WAV] Starting WAV conversion...", flush=True)
        wav_data = self.pcm_to_wav(pcm_data, self.sample_rate, self.channels)
        total_time = time.time() - start_time

        # Calculate actual audio duration (16kHz, mono, 16-bit = 32000 bytes/sec)
        audio_duration = len(pcm_data) / 32000.0
        print(
            f"[THYMIA_BUFFER_GET_WAV] WAV conversion complete in {total_time:.3f}s - returning {len(wav_data)} bytes ({audio_duration:.1f}s audio)",
            flush=True,
        )

        return wav_data

    @staticmethod
    def pcm_to_wav(
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
            connector = aiohttp.TCPConnector(
                force_close=True, enable_cleanup_closed=True
            )
            self.session = aiohttp.ClientSession(
                connector=connector,
                headers={
                    "x-api-key": self.api_key,
                    "Content-Type": "application/json",
                    "User-Agent": "TEN-Thymia-Client/1.0",
                },
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
        locale: str = "en-GB",  # Use en-GB for better Thymia speech detection
    ) -> dict:
        """
        Create a new Thymia analysis session.

        Returns: {
            "id": "session_id",
            "recordingUploadUrl": "presigned_s3_url"
        }
        """
        payload = {
            "user": {
                "userLabel": user_label,
                "dateOfBirth": date_of_birth,
                "birthSex": birth_sex,
            },
            "language": locale,
        }

        # Use curl subprocess to make the request (exactly like manual tests)
        curl_cmd = [
            "curl",
            "-X",
            "POST",
            f"{self.base_url}/v1/models/mental-wellness",
            "-H",
            f"x-api-key: {self.api_key}",
            "-H",
            "Content-Type: application/json",
            "-d",
            json.dumps(payload),
            "-s",
            "-w",
            "\n%{http_code}",  # Silent mode, write HTTP code at end
        ]

        # Log full curl command for manual testing (properly escaped)
        curl_parts = []
        skip_next = False
        for i, arg in enumerate(curl_cmd[:-2]):  # Exclude -w and http_code
            if skip_next:
                skip_next = False
                continue
            if arg == "-d" and i + 1 < len(curl_cmd):
                # Use single quotes for JSON payload to preserve internal double quotes
                curl_parts.append(f"-d '{curl_cmd[i + 1]}'")
                skip_next = True
            elif " " in arg:
                curl_parts.append(f'"{arg}"')
            else:
                curl_parts.append(arg)
        curl_cmd_str = " ".join(curl_parts)
        # Mask API key in log output
        masked_key = (
            f"{self.api_key[:8]}...{self.api_key[-4:]}"
            if len(self.api_key) > 12
            else "***"
        )
        curl_cmd_logged = curl_cmd_str.replace(self.api_key, masked_key)
        print(
            f"[THYMIA_HELLOS_CURL_CREATE] {curl_cmd_logged}",
            flush=True,
        )

        # Run curl asynchronously
        process = await asyncio.create_subprocess_exec(
            *curl_cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, _stderr = await process.communicate()

        # Parse response
        stdout_str = stdout.decode("utf-8")

        # Extract HTTP code from last line and body from rest
        lines = stdout_str.strip().split("\n")
        http_code = int(lines[-1]) if lines else 0
        response_body = "\n".join(lines[:-1]) if len(lines) > 1 else ""

        if http_code not in (200, 201):
            print(
                f"[THYMIA_API_ERROR] create_session failed: {http_code} - {response_body}",
                flush=True,
            )
            raise ThymiaAPIError(
                f"Failed to create session: {http_code} - {response_body}"
            )

        response_data = json.loads(response_body)
        return response_data

    async def upload_audio(self, upload_url: str, wav_data: bytes) -> bool:
        """Upload WAV audio to presigned S3 URL using aiohttp (in-memory, no disk I/O)"""
        await self._ensure_session()

        headers = {"Content-Type": "audio/wav"}

        try:
            async with self.session.put(
                upload_url, data=wav_data, headers=headers
            ) as response:
                if response.status not in (200, 201, 204):
                    error_text = await response.text()
                    print(
                        f"[THYMIA_HELLOS_UPLOAD_ERROR] Upload failed: status={response.status}, error={error_text}",
                        flush=True,
                    )
                    return False

                print(
                    f"[THYMIA_HELLOS_UPLOAD] Upload successful: status={response.status}",
                    flush=True,
                )
                return True
        except Exception as e:
            print(
                f"[THYMIA_HELLOS_UPLOAD_ERROR] Exception during upload: {e}",
                flush=True,
            )
            return False

    async def get_results(self, session_id: str) -> Optional[dict]:
        """
        Poll for analysis results.

        Returns None if still processing, dict with results if complete.
        """
        await self._ensure_session()

        # Log equivalent curl command for manual testing (API key masked)
        masked_key = (
            f"{self.api_key[:8]}...{self.api_key[-4:]}"
            if len(self.api_key) > 12
            else "***"
        )
        curl_equivalent = f'curl -X GET "{self.base_url}/v1/models/mental-wellness/{session_id}" -H "x-api-key: {masked_key}"'
        print(
            f"[THYMIA_HELLOS_CURL_GET_RESULTS] {curl_equivalent}",
            flush=True,
        )

        async with self.session.get(
            f"{self.base_url}/v1/models/mental-wellness/{session_id}"
        ) as response:
            if response.status != 200:
                error_text = await response.text()
                print(
                    f"[THYMIA_API_ERROR] get_results failed: status={response.status}, error={error_text}",
                    flush=True,
                )
                raise ThymiaAPIError(
                    f"Failed to get results: {response.status} - {error_text}"
                )

            data = await response.json()
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
            try:
                results = await self.get_results(session_id)
                if results:
                    return results
            except Exception as e:
                print(
                    f"[THYMIA_API_ERROR] Error polling session {session_id}: {e}",
                    flush=True,
                )

            await asyncio.sleep(poll_interval)

        print(
            f"[THYMIA_API_ERROR] Timeout after {max_wait_seconds}s for session {session_id}",
            flush=True,
        )
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
        self.min_speech_duration: float = MOOD_PHASE_DURATION_SECONDS
        self.silence_threshold: float = 0.02
        self.continuous_analysis: bool = True
        self.min_interval_seconds: int = 60
        self.max_analyses_per_session: int = 10
        self.poll_timeout: int = 60  # Stop polling after 1 minute
        self.poll_interval: int = 5

        # Analysis mode configuration (for backwards compatibility)
        self.analysis_mode: str = "hellos_only"  # "hellos_only" or "demo_dual"
        self.apollo_mood_duration: float = (
            MOOD_PHASE_DURATION_SECONDS  # Duration of mood audio for Apollo
        )
        self.apollo_read_duration: float = (
            READING_PHASE_DURATION_SECONDS  # Duration of reading audio for Apollo
        )

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
        self.hellos_success: bool = (
            False  # True only if Hellos completed with COMPLETE_OK
        )
        self.apollo_complete: bool = False
        self.hellos_analysis_running: bool = False
        self.apollo_analysis_running: bool = False

        # API session tracking
        self.hellos_session_id: Optional[str] = None
        self.hellos_session_start_time: float = 0.0
        self.apollo_session_id: Optional[str] = None
        self.apollo_session_start_time: float = 0.0

        # Announcement retry tracking (prevent spam)
        self.hellos_last_announcement_time: float = 0.0
        self.apollo_last_announcement_time: float = 0.0
        self.announcement_retry_interval: float = 30.0  # Retry every 30s
        self.hellos_retry_count: int = 0  # Track retry attempts
        self.apollo_retry_count: int = 0  # Track retry attempts
        self.max_announcement_retries: int = 3  # Give up after 3 retries
        self.announcement_timeout: float = 90.0  # Force-complete after 90s

        # Input phase tracking (independent of API state)
        self.mood_phase_complete: bool = False  # 30s mood speech collected
        self.reading_phase_complete: bool = (
            False  # 60s reading speech collected
        )

        # Trigger tracking (whether we've sent async message to LLM)
        self.hellos_trigger_sent: bool = False
        self.apollo_trigger_sent: bool = False

        # Announcement confirmation tracking (whether LLM confirmed announcing to user)
        self.hellos_shared_with_user: bool = False
        self.apollo_shared_with_user: bool = False

        # Activity tracking (to avoid interrupting user or agent)
        self.last_user_speech_time: float = 0.0
        self.user_currently_speaking: bool = False
        self.agent_currently_speaking: bool = False  # Track TTS playback state
        self.agent_speaking_until: float = (
            0.0  # Timestamp when agent will finish speaking
        )
        self.last_agent_speech_end_time: float = 0.0
        self.response_start_time: float = (
            0.0  # When current TTS response started (with 500ms buffer)
        )

        # User information for Thymia API
        self.user_name: Optional[str] = None
        self.user_dob: Optional[str] = None
        self.user_sex: Optional[str] = None
        self.user_locale: str = (
            "en-GB"  # Use en-GB for better Thymia speech detection
        )

        # ============ SENTINEL MODE STATE ============
        # API mode: "rest_batch" (default, existing REST APIs) or "sentinel" (WebSocket)
        self.api_mode: str = "rest_batch"

        # Sentinel configuration
        self.ws_url: str = "wss://ws.thymia.ai"
        self.biomarkers: list[str] = ["helios", "apollo"]
        self.policies: list[str] = ["passthrough", "safety_analysis"]
        self.forward_transcripts: bool = True
        self.stream_agent_audio: bool = True
        self.auto_reconnect: bool = True

        # Sentinel client and state
        self.sentinel_client: Optional[SentinelClient] = None
        self.sentinel_latest_result: Optional[PolicyResult] = None
        self.sentinel_wellness: Optional[WellnessMetricsCompat] = None
        self.sentinel_apollo: Optional[ApolloResult] = None
        self.sentinel_safety: Optional[SafetyClassification] = None
        self.sentinel_results_count: int = 0
        self.sentinel_results_announced: bool = False
        # Track previous values to detect changes
        self.sentinel_prev_wellness: Optional[WellnessMetricsCompat] = None
        self.sentinel_prev_apollo: Optional[ApolloResult] = None
        # Deferred announcement queue for Sentinel mode (when someone is speaking)
        self.sentinel_deferred_result: Optional[PolicyResult] = None

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        """Called when extension starts"""
        ten_env.log_info("[THYMIA_START] ThymiaAnalyzerExtension starting...")

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

            # Load API mode (defaults to rest_batch for backwards compatibility)
            try:
                api_mode_result = await ten_env.get_property_string("api_mode")
                self.api_mode = (
                    api_mode_result[0]
                    if isinstance(api_mode_result, tuple)
                    else api_mode_result
                )
            except Exception:
                self.api_mode = "rest_batch"

            # Load Sentinel-specific configuration if in sentinel mode
            if self.api_mode == "sentinel":
                try:
                    ws_url_result = await ten_env.get_property_string("ws_url")
                    self.ws_url = (
                        ws_url_result[0]
                        if isinstance(ws_url_result, tuple)
                        else ws_url_result
                    ) or "wss://ws.thymia.ai"
                except Exception:
                    self.ws_url = "wss://ws.thymia.ai"

                try:
                    biomarkers_result = await ten_env.get_property_string("biomarkers")
                    biomarkers_str = (
                        biomarkers_result[0]
                        if isinstance(biomarkers_result, tuple)
                        else biomarkers_result
                    )
                    self.biomarkers = (
                        [b.strip() for b in biomarkers_str.split(",")]
                        if biomarkers_str
                        else ["helios", "apollo"]
                    )
                except Exception:
                    self.biomarkers = ["helios", "apollo"]

                try:
                    policies_result = await ten_env.get_property_string("policies")
                    policies_str = (
                        policies_result[0]
                        if isinstance(policies_result, tuple)
                        else policies_result
                    )
                    self.policies = (
                        [p.strip() for p in policies_str.split(",")]
                        if policies_str
                        else ["passthrough", "safety_analysis"]
                    )
                except Exception:
                    self.policies = ["passthrough", "safety_analysis"]

                try:
                    self.forward_transcripts = await ten_env.get_property_bool(
                        "forward_transcripts"
                    )
                except Exception:
                    self.forward_transcripts = True

                try:
                    self.stream_agent_audio = await ten_env.get_property_bool(
                        "stream_agent_audio"
                    )
                except Exception:
                    self.stream_agent_audio = True

                try:
                    self.auto_reconnect = await ten_env.get_property_bool(
                        "auto_reconnect"
                    )
                except Exception:
                    self.auto_reconnect = True

                ten_env.log_info(
                    f"[THYMIA_SENTINEL_CONFIG] Loaded Sentinel config: "
                    f"ws_url={self.ws_url}, biomarkers={self.biomarkers}, "
                    f"policies={self.policies}, forward_transcripts={self.forward_transcripts}, "
                    f"stream_agent_audio={self.stream_agent_audio}"
                )

            # Load Apollo-specific durations if specified
            try:
                ten_env.log_info(
                    f"[THYMIA_PROPERTY_LOAD] Attempting to load apollo_mood_duration (default={self.apollo_mood_duration})"
                )
                apollo_mood_result = await ten_env.get_property_float(
                    "apollo_mood_duration"
                )
                ten_env.log_info(
                    f"[THYMIA_PROPERTY_LOAD] Raw result={apollo_mood_result}"
                )

                # Check if result is valid (not an error tuple)
                if isinstance(apollo_mood_result, tuple):
                    value, error = apollo_mood_result
                    if error is None and value > 0:
                        self.apollo_mood_duration = value
                        ten_env.log_info(
                            f"[THYMIA_PROPERTY_LOAD] Loaded apollo_mood_duration: {self.apollo_mood_duration}"
                        )
                    else:
                        ten_env.log_warn(
                            f"[THYMIA_PROPERTY_LOAD] Property returned error={error}, apollo_mood_duration"
                        )
                else:
                    if apollo_mood_result > 0:
                        self.apollo_mood_duration = apollo_mood_result
                        ten_env.log_info(
                            f"[THYMIA_PROPERTY_LOAD] Loaded apollo_mood_duration: {self.apollo_mood_duration}"
                        )
            except Exception as e:
                ten_env.log_warn(
                    f"[THYMIA_PROPERTY_LOAD] Failed to load apollo_mood_duration, using default {self.apollo_mood_duration}: {e}"
                )
                import traceback

                ten_env.log_warn(
                    f"[THYMIA_PROPERTY_LOAD] Traceback: {traceback.format_exc()}"
                )

            try:
                ten_env.log_info(
                    f"[THYMIA_PROPERTY_LOAD] Attempting to load apollo_read_duration (default={self.apollo_read_duration})"
                )
                apollo_read_result = await ten_env.get_property_float(
                    "apollo_read_duration"
                )
                ten_env.log_info(
                    f"[THYMIA_PROPERTY_LOAD] Raw result={apollo_read_result}"
                )

                # Check if result is valid (not an error tuple)
                if isinstance(apollo_read_result, tuple):
                    value, error = apollo_read_result
                    if error is None and value > 0:
                        self.apollo_read_duration = value
                        ten_env.log_info(
                            f"[THYMIA_PROPERTY_LOAD] Loaded apollo_read_duration: {self.apollo_read_duration}"
                        )
                    else:
                        ten_env.log_warn(
                            f"[THYMIA_PROPERTY_LOAD] Property returned error={error}, apollo_read_duration"
                        )
                else:
                    if apollo_read_result > 0:
                        self.apollo_read_duration = apollo_read_result
                        ten_env.log_info(
                            f"[THYMIA_PROPERTY_LOAD] Loaded apollo_read_duration: {self.apollo_read_duration}"
                        )
            except Exception as e:
                ten_env.log_warn(
                    f"[THYMIA_PROPERTY_LOAD] Failed to load apollo_read_duration, using default {self.apollo_read_duration}: {e}"
                )
                import traceback

                ten_env.log_warn(
                    f"[THYMIA_PROPERTY_LOAD] Traceback: {traceback.format_exc()}"
                )

            ten_env.log_info(
                f"[THYMIA_CONFIG] Loaded config: analysis_mode={self.analysis_mode}, "
                f"silence_threshold={self.silence_threshold}, "
                f"min_speech_duration={self.min_speech_duration}"
            )
            # Explicitly log durations to verify property loading
            ten_env.log_info(
                f"[THYMIA_DURATION_CHECK] apollo_mood_duration={self.apollo_mood_duration}s, "
                f"apollo_read_duration={self.apollo_read_duration}s, "
                f"total_required={self.apollo_mood_duration + self.apollo_read_duration}s"
            )
        except Exception as e:
            ten_env.log_warn(
                f"[THYMIA_CONFIG] Failed to load some properties, using defaults: {e}"
            )

        # Validate API key
        if not self.api_key:
            ten_env.log_error(
                "[THYMIA_ERROR] Thymia API key not configured - extension will be disabled"
            )
            await super().on_start(ten_env)
            return

        # Initialize components based on API mode
        if self.api_mode == "sentinel":
            # ============ SENTINEL MODE ============
            # No local audio buffer needed - server handles buffering
            ten_env.log_info(
                f"[THYMIA_START] Initializing in SENTINEL mode "
                f"(WebSocket: {self.ws_url})"
            )

            # Create log callback for Sentinel client
            def sentinel_log(level: str, message: str):
                if level == "info":
                    ten_env.log_info(f"[SENTINEL] {message}")
                elif level == "warn":
                    ten_env.log_warn(f"[SENTINEL] {message}")
                elif level == "error":
                    ten_env.log_error(f"[SENTINEL] {message}")
                else:
                    ten_env.log_debug(f"[SENTINEL] {message}")

            # Create Sentinel client
            self.sentinel_client = SentinelClient(
                api_key=self.api_key,
                server_url=self.ws_url,
                on_policy_result=lambda r: asyncio.create_task(
                    self._on_sentinel_policy_result(ten_env, r)
                ),
                on_status=lambda s: self._on_sentinel_status(s),
                on_error=lambda e: self._on_sentinel_error(ten_env, e),
                auto_reconnect=self.auto_reconnect,
                log_callback=sentinel_log,
            )

            ten_env.log_info(
                f"[THYMIA_START] ThymiaAnalyzerExtension started in SENTINEL mode "
                f"(biomarkers={self.biomarkers}, policies={self.policies})"
            )

            # Connect immediately with placeholder values
            # User info can be updated later when provided
            import uuid
            # Don't connect immediately - wait for user info (name, DOB, sex)
            # Connection will happen in check_phase_progress when user provides info
            ten_env.log_info(
                "[SENTINEL_CONNECT] Waiting for user info before connecting to Sentinel. "
                "Connection will be established when user provides name, sex, and year of birth."
            )

        else:
            # ============ REST BATCH MODE (existing behavior) ============
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
                    f"[THYMIA_START] ThymiaAnalyzerExtension started in DEMO_DUAL mode "
                    f"(Hellos + Apollo, mood={self.apollo_mood_duration}s, "
                    f"read={self.apollo_read_duration}s)"
                )
            else:
                ten_env.log_info(
                    f"[THYMIA_START] ThymiaAnalyzerExtension started in HELLOS_ONLY mode "
                    f"(min_speech={self.min_speech_duration}s)"
                )

        # Register as LLM tool (parent class handles this)
        await super().on_start(ten_env)

        # Log tool registration
        tools = self.get_tool_metadata(ten_env)
        tool_names = [t.name for t in tools]
        ten_env.log_info(
            f"[THYMIA_TOOL_REGISTRATION] Registered {len(tools)} tools: {', '.join(tool_names)}"
        )

        # Start unified results poller (REST mode only - Sentinel uses callbacks)
        if self.api_mode != "sentinel":
            asyncio.create_task(self._unified_results_poller(ten_env))

        # TEST: Send a test announcement after 5 seconds to verify text_data mechanism
        # asyncio.create_task(self._test_announcement_after_delay(ten_env))  # DISABLED - test passed

    async def _test_announcement_after_delay(self, ten_env: AsyncTenEnv):
        """TEST: Send announcement after 5 seconds to verify LLM receives text_data"""
        try:
            await asyncio.sleep(5)
            test_message = "[TEST MESSAGE] Please respond with the word PINEAPPLE to confirm you received this test announcement."

            ten_env.log_info(
                f"[THYMIA_TEST] Sending test announcement after 5s: {test_message}"
            )

            text_data = Data.create("text_data")
            text_data.set_property_string("text", test_message)
            text_data.set_property_bool("end_of_segment", True)
            await ten_env.send_data(text_data)

            ten_env.log_info(
                "[THYMIA_TEST] Test announcement sent - check if LLM responds with PINEAPPLE"
            )
        except Exception as e:
            ten_env.log_error(f"[THYMIA_TEST] Test announcement failed: {e}")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        """Called when extension stops"""
        ten_env.log_info("[THYMIA_STOP] ThymiaAnalyzerExtension stopping...")

        # Close Sentinel client if in sentinel mode
        if self.sentinel_client:
            await self.sentinel_client.disconnect()

        if self.api_client:
            await self.api_client.close()

        if self.apollo_client:
            await self.apollo_client.close()

        await super().on_stop(ten_env)

    # ============ SENTINEL MODE METHODS ============

    async def _connect_sentinel_with_user_info(self, ten_env: AsyncTenEnv) -> bool:
        """
        Connect to Sentinel server once user info is available.

        Called when user info is first set via check_phase_progress tool.
        """
        if not self.sentinel_client:
            ten_env.log_error(
                "[SENTINEL_CONNECT] Sentinel client not initialized"
            )
            return False

        if self.sentinel_client.is_connected:
            ten_env.log_debug(
                "[SENTINEL_CONNECT] Already connected to Sentinel"
            )
            return True

        if not self.user_name or not self.user_dob or not self.user_sex:
            ten_env.log_warn(
                "[SENTINEL_CONNECT] Cannot connect - missing user info "
                f"(name={self.user_name}, dob={self.user_dob}, sex={self.user_sex})"
            )
            return False

        ten_env.log_info(
            f"[SENTINEL_CONNECT] Connecting with user: {self.user_name}, "
            f"DOB: {self.user_dob}, sex: {self.user_sex}"
        )

        config = SentinelConfig(
            api_key=self.api_key,
            user_label=self.user_name or "anonymous",
            date_of_birth=self.user_dob or "1990-01-01",
            birth_sex=self.user_sex or "FEMALE",
            language=self.user_locale,
            biomarkers=self.biomarkers,
            policies=self.policies,
        )

        success = await self.sentinel_client.connect(config)
        if success:
            ten_env.log_info("[SENTINEL_CONNECT] Connected to Sentinel server")
        else:
            ten_env.log_error("[SENTINEL_CONNECT] Failed to connect to Sentinel")

        return success

    async def _connect_sentinel_immediately(
        self, ten_env: AsyncTenEnv, session_id: str
    ) -> bool:
        """
        Connect to Sentinel server immediately with placeholder user info.

        Called on startup to begin streaming audio right away.
        Real user info can be provided later via check_phase_progress.
        """
        if not self.sentinel_client:
            ten_env.log_error(
                "[SENTINEL_CONNECT] Sentinel client not initialized"
            )
            return False

        if self.sentinel_client.is_connected:
            ten_env.log_debug(
                "[SENTINEL_CONNECT] Already connected to Sentinel"
            )
            return True

        ten_env.log_info(
            f"[SENTINEL_CONNECT] Connecting immediately with session: {session_id}"
        )

        # Use placeholder values - biomarkers work on voice, not demographics
        config = SentinelConfig(
            api_key=self.api_key,
            user_label=session_id,
            date_of_birth="1980-01-01",  # Placeholder
            birth_sex="FEMALE",  # Placeholder (API only accepts MALE/FEMALE)
            language="en-GB",
            biomarkers=self.biomarkers,
            policies=self.policies,
        )

        success = await self.sentinel_client.connect(config)
        if success:
            ten_env.log_info(
                "[SENTINEL_CONNECT] Connected to Sentinel server (streaming audio now)"
            )
        else:
            ten_env.log_error(
                "[SENTINEL_CONNECT] Failed to connect to Sentinel - will retry"
            )

        return success

    async def _on_sentinel_policy_result(
        self, ten_env: AsyncTenEnv, result: PolicyResult
    ):
        """
        Handle PolicyResult from Sentinel server.

        Maps result to backward-compatible formats and triggers LLM announcement.
        """
        self.sentinel_latest_result = result
        self.sentinel_results_count += 1

        ten_env.log_info(
            f"[SENTINEL_RESULT] Received policy result #{self.sentinel_results_count}: "
            f"policy={result.policy}, analysis_type={result.analysis_type}"
        )

        # Map to backward-compatible formats
        wellness, apollo, safety = ResultMapper.extract_all(
            result,
            session_id=f"sentinel-{self.sentinel_results_count}",
        )

        # Save previous values before updating (for change detection)
        if wellness:
            self.sentinel_prev_wellness = self.sentinel_wellness
        if apollo:
            self.sentinel_prev_apollo = self.sentinel_apollo

        # Update state
        if wellness:
            self.sentinel_wellness = wellness
            # Also update the existing latest_results for tool compatibility
            self.latest_results = WellnessMetrics(
                distress=wellness.distress,
                stress=wellness.stress,
                burnout=wellness.burnout,
                fatigue=wellness.fatigue,
                low_self_esteem=wellness.low_self_esteem,
                timestamp=wellness.timestamp,
                session_id=wellness.session_id,
            )
            ten_env.log_info(
                f"[SENTINEL_RESULT] Wellness metrics: "
                f"distress={wellness.distress:.2%}, stress={wellness.stress:.2%}, "
                f"burnout={wellness.burnout:.2%}, fatigue={wellness.fatigue:.2%}, "
                f"low_self_esteem={wellness.low_self_esteem:.2%}"
            )

        if apollo:
            self.sentinel_apollo = apollo
            # Also update the existing apollo_results for tool compatibility
            self.apollo_results = apollo
            ten_env.log_info(
                f"[SENTINEL_RESULT] Apollo metrics: "
                f"depression={apollo.depression_probability:.2%} ({apollo.depression_severity}), "
                f"anxiety={apollo.anxiety_probability:.2%} ({apollo.anxiety_severity})"
            )

        if safety:
            self.sentinel_safety = safety
            ten_env.log_info(
                f"[SENTINEL_RESULT] Safety classification: "
                f"level={safety.level}, alert={safety.alert}, "
                f"urgency={safety.urgency}, concerns={safety.concerns}"
            )

            # Handle high-risk classifications
            if safety.is_high_risk():
                ten_env.log_warn(
                    f"[SENTINEL_SAFETY_ALERT] High risk detected! "
                    f"Level={safety.level}, Alert={safety.alert}"
                )
                # Send urgent system message to LLM
                await self._trigger_sentinel_safety_alert(ten_env, safety)

        # Always trigger LLM announcement when new results arrive
        # The LLM will announce to user and can track if values have changed
        if wellness or apollo:
            await self._trigger_sentinel_results_announcement(ten_env, result)

    def _on_sentinel_status(self, status: StatusMessage):
        """Handle status update from Sentinel server."""
        # Just store for check_phase_progress tool
        pass

    def _on_sentinel_error(self, ten_env: AsyncTenEnv, error: ErrorMessage):
        """Handle error from Sentinel server."""
        ten_env.log_error(
            f"[SENTINEL_ERROR] {error.error_code}: {error.message}"
        )
        if error.details:
            ten_env.log_error(f"[SENTINEL_ERROR] Details: {error.details}")

    async def _trigger_sentinel_results_announcement(
        self, ten_env: AsyncTenEnv, result: PolicyResult
    ):
        """Send announcement to LLM when Sentinel results are ready."""
        try:
            # Send system message immediately - LLM will have data for next response
            # No deferral needed: TTS won't interrupt user, and LLM needs data in context

            # Determine if this is initial or update
            is_initial = result.analysis_type == "initial" or (
                self.sentinel_prev_wellness is None
                and self.sentinel_prev_apollo is None
            )

            # Build list of what's available and what changed
            wellness_changes = []
            apollo_changes = []

            if self.sentinel_wellness:
                w = self.sentinel_wellness
                if is_initial:
                    # First result - show key wellness metrics only (stress, burnout, fatigue)
                    wellness_changes = [
                        f"stress={round(w.stress * 100)}%",
                        f"burnout={round(w.burnout * 100)}%",
                        f"fatigue={round(w.fatigue * 100)}%",
                    ]
                elif self.sentinel_prev_wellness:
                    # Update - show only significant changes (>15% difference)
                    prev = self.sentinel_prev_wellness
                    threshold = 0.15
                    if abs(w.stress - prev.stress) > threshold:
                        wellness_changes.append(
                            f"stress: {round(prev.stress * 100)}%→{round(w.stress * 100)}%"
                        )
                    if abs(w.burnout - prev.burnout) > threshold:
                        wellness_changes.append(
                            f"burnout: {round(prev.burnout * 100)}%→{round(w.burnout * 100)}%"
                        )
                    if abs(w.fatigue - prev.fatigue) > threshold:
                        wellness_changes.append(
                            f"fatigue: {round(prev.fatigue * 100)}%→{round(w.fatigue * 100)}%"
                        )

            if self.sentinel_apollo:
                a = self.sentinel_apollo
                if is_initial or self.sentinel_prev_apollo is None:
                    # First result - show all values
                    apollo_changes = [
                        f"depression={round(a.depression_probability * 100)}% ({a.depression_severity})",
                        f"anxiety={round(a.anxiety_probability * 100)}% ({a.anxiety_severity})",
                    ]
                else:
                    # Update - show only significant changes (>15% difference)
                    prev = self.sentinel_prev_apollo
                    threshold = 0.15
                    if abs(a.depression_probability - prev.depression_probability) > threshold:
                        apollo_changes.append(
                            f"depression: {round(prev.depression_probability * 100)}%→{round(a.depression_probability * 100)}%"
                        )
                    if abs(a.anxiety_probability - prev.anxiety_probability) > threshold:
                        apollo_changes.append(
                            f"anxiety: {round(prev.anxiety_probability * 100)}%→{round(a.anxiety_probability * 100)}%"
                        )

            # Skip if no changes to report (for updates)
            if not is_initial and not wellness_changes and not apollo_changes:
                ten_env.log_debug(
                    "[SENTINEL_ANNOUNCE] No significant changes to announce"
                )
                return

            # Build announcement message
            if is_initial:
                if wellness_changes and apollo_changes:
                    hint_text = (
                        f"[SYSTEM ALERT] INITIAL ANALYSIS READY. "
                        f"Wellness: {', '.join(wellness_changes)}. "
                        f"Clinical: {', '.join(apollo_changes)}. "
                        f"Call get_wellness_metrics and share insights naturally - don't list numbers."
                    )
                elif wellness_changes:
                    hint_text = (
                        f"[SYSTEM ALERT] INITIAL WELLNESS ANALYSIS READY. "
                        f"Results: {', '.join(wellness_changes)}. "
                        f"Call get_wellness_metrics and share insights naturally - don't list numbers."
                    )
                elif apollo_changes:
                    hint_text = (
                        f"[SYSTEM ALERT] CLINICAL ANALYSIS READY. "
                        f"Results: {', '.join(apollo_changes)}. "
                        f"Call get_wellness_metrics and mention depression/anxiety status naturally."
                    )
                else:
                    ten_env.log_debug("[SENTINEL_ANNOUNCE] No results to announce")
                    return
            else:
                # Update announcement - only mention what changed
                changes = wellness_changes + apollo_changes
                hint_text = (
                    f"[SYSTEM ALERT] ANALYSIS UPDATE. "
                    f"Significant changes: {', '.join(changes)}. "
                    f"Weave this into the conversation naturally."
                )

            ten_env.log_info(f"[SENTINEL_ANNOUNCE] Sending: {hint_text}")

            text_data = Data.create("text_data")
            text_data.set_property_string("text", hint_text)
            text_data.set_property_bool("end_of_segment", True)
            text_data.set_property_string("role", "system")
            await ten_env.send_data(text_data)

            self.sentinel_results_announced = True
            ten_env.log_info("[SENTINEL_ANNOUNCE] Announcement sent to LLM")

        except Exception as e:
            ten_env.log_error(f"[SENTINEL_ANNOUNCE] Failed: {e}")

    async def _trigger_sentinel_safety_alert(
        self, ten_env: AsyncTenEnv, safety: SafetyClassification
    ):
        """Send urgent safety alert to LLM."""
        try:
            # Build alert message
            actions_text = ""
            if safety.recommended_actions and safety.recommended_actions.get("for_agent"):
                actions_text = f" Recommended action: {safety.recommended_actions['for_agent']}"

            concerns_text = ""
            if safety.concerns:
                concerns_text = f" Concerns: {', '.join(safety.concerns)}."

            if safety.requires_immediate_action():
                alert_text = (
                    f"[URGENT SAFETY ALERT] Crisis level detected ({safety.alert}). "
                    f"Urgency: {safety.urgency}.{concerns_text}{actions_text}"
                )
            else:
                alert_text = (
                    f"[SAFETY ALERT] Elevated risk level ({safety.alert}). "
                    f"Urgency: {safety.urgency}.{concerns_text}{actions_text}"
                )

            ten_env.log_warn(f"[SENTINEL_SAFETY] Sending alert: {alert_text}")

            text_data = Data.create("text_data")
            text_data.set_property_string("text", alert_text)
            text_data.set_property_bool("end_of_segment", True)
            text_data.set_property_string("role", "system")
            await ten_env.send_data(text_data)

        except Exception as e:
            ten_env.log_error(f"[SENTINEL_SAFETY] Failed to send alert: {e}")

    async def _process_deferred_sentinel_announcement(self, ten_env: AsyncTenEnv):
        """Process any deferred Sentinel announcement when speaking stops."""
        if self.api_mode != "sentinel" or self.sentinel_deferred_result is None:
            return

        # Check if it's now safe to announce
        if self.user_currently_speaking or self.agent_currently_speaking:
            ten_env.log_debug(
                "[SENTINEL_DEFERRED] Still speaking, keeping deferred result"
            )
            return

        ten_env.log_info(
            "[SENTINEL_DEFERRED] Processing deferred announcement now"
        )
        result = self.sentinel_deferred_result
        self.sentinel_deferred_result = None  # Clear before processing
        await self._trigger_sentinel_results_announcement(ten_env, result)

    async def _delayed_sentinel_check(
        self, ten_env: AsyncTenEnv, delay_seconds: float
    ):
        """Wait for delay then check for deferred Sentinel announcements."""
        try:
            await asyncio.sleep(delay_seconds)
            ten_env.log_debug(
                f"[SENTINEL_DELAYED_CHECK] Checking for deferred announcements after {delay_seconds:.1f}s delay"
            )
            await self._process_deferred_sentinel_announcement(ten_env)
        except asyncio.CancelledError:
            pass  # Task cancelled, ignore
        except Exception as e:
            ten_env.log_error(
                f"[SENTINEL_DELAYED_CHECK] Error in delayed check: {e}"
            )

    # ============ END SENTINEL MODE METHODS ============

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        """Handle incoming data messages (e.g., TTS state events)"""
        try:
            data_name = data.get_name()
            # Debug logging - useful for troubleshooting TTS message routing
            # ten_env.log_debug(f"[THYMIA_ON_DATA] Received data message: {data_name}")

            if data_name == "tts_audio_start":
                # Agent started speaking - don't send announcements during this time
                self.agent_currently_speaking = True

                # Record when response started (with 500ms buffer for audio to start playing)
                self.response_start_time = time.time() + 0.5

                # Get full payload for analysis
                json_str, _ = data.get_property_to_json(None)
                ten_env.log_info(
                    f"[THYMIA_TTS_START] tts_audio_start received. "
                    f"Response will start playing at timestamp: {self.response_start_time:.2f}. "
                    f"Payload: {json_str}"
                )

            elif data_name == "tts_audio_end":
                # Get full payload for analysis
                json_str, _ = data.get_property_to_json(None)
                payload = json.loads(json_str) if json_str else {}

                request_id = payload.get("request_id", "unknown")
                reason = payload.get("reason", "unknown")
                duration_ms = payload.get("request_total_audio_duration_ms", 0)
                interval_ms = payload.get("request_event_interval_ms", 0)

                ten_env.log_info(
                    f"[THYMIA_TTS_END] tts_audio_end received. "
                    f"request_id={request_id}, reason={reason}, "
                    f"audio_duration={duration_ms}ms, generation_time={interval_ms}ms. "
                    f"Full payload: {json_str}"
                )

                if reason == 1:
                    # TTS generation complete - calculate when audio will finish playing
                    # Use response_start_time (tts_audio_start + 500ms) + total duration
                    if self.response_start_time > 0:
                        self.agent_speaking_until = self.response_start_time + (
                            duration_ms / 1000.0
                        )
                        ten_env.log_info(
                            f"[THYMIA_TTS_END] TTS generation complete (reason=1). "
                            f"Total audio duration: {duration_ms}ms. "
                            f"Started at: {self.response_start_time:.2f}, "
                            f"will finish at: {self.agent_speaking_until:.2f}"
                        )
                    else:
                        # Fallback if response_start_time wasn't set
                        ten_env.log_warn(
                            "[THYMIA_TTS_END] response_start_time not set, using fallback calculation"
                        )
                        self.agent_speaking_until = (
                            time.time() + (duration_ms / 1000.0) + 1.0
                        )

                    # Clear state and schedule check after audio finishes
                    self.agent_currently_speaking = False
                    self.response_start_time = 0.0

                    # Schedule check for pending announcements after audio finishes playing
                    # This ensures Hellos gets announced even if Apollo went first
                    delay_seconds = (duration_ms / 1000.0) + 0.5
                    asyncio.create_task(
                        self._delayed_announcement_check(ten_env, delay_seconds)
                    )
                    # Also check for deferred Sentinel announcements after audio finishes
                    asyncio.create_task(
                        self._delayed_sentinel_check(ten_env, delay_seconds)
                    )

                    # Set last speech end time when audio will finish
                    self.last_agent_speech_end_time = time.time() + (
                        duration_ms / 1000.0
                    )
                elif reason == 2:
                    # Playback interrupted or complete - stop immediately
                    self.agent_speaking_until = 0.0
                    self.agent_currently_speaking = False
                    self.response_start_time = 0.0
                    self.last_agent_speech_end_time = time.time()
                    ten_env.log_info(
                        "[THYMIA_TTS_END] Playback ended (reason=2). Agent stopped speaking."
                    )
                    # Check for pending announcements immediately
                    await self._check_and_trigger_ready_announcements(ten_env)
                    # Also check for deferred Sentinel announcements
                    await self._process_deferred_sentinel_announcement(ten_env)

            # ============ SENTINEL MODE: Forward transcripts ============
            # Handle ASR results (from STT extension) for Sentinel mode
            elif data_name == "asr_result" and self.api_mode == "sentinel":
                if self.forward_transcripts and self.sentinel_client and self.sentinel_client.is_connected:
                    try:
                        # ASR result comes as JSON with text and final fields
                        asr_json, _ = data.get_property_to_json(None)
                        asr_data = json.loads(asr_json) if asr_json else {}

                        text = asr_data.get("text", "")
                        is_final = asr_data.get("final", False)

                        if text and is_final:
                            ten_env.log_info(
                                f"[SENTINEL_TRANSCRIPT] Forwarding ASR: '{text[:100]}...'"
                            )
                            await self.sentinel_client.send_transcript(
                                speaker="user",
                                text=text,
                                is_final=True,
                            )
                    except Exception as e:
                        ten_env.log_warn(
                            f"[SENTINEL_TRANSCRIPT] Failed to forward ASR result: {e}"
                        )

            # Handle text_data (alternative transcript format) for Sentinel mode
            elif data_name == "text_data" and self.api_mode == "sentinel":
                if self.forward_transcripts and self.sentinel_client and self.sentinel_client.is_connected:
                    try:
                        # Try to extract transcript text
                        text = data.get_property_string("text") if hasattr(data, "get_property_string") else None
                        if text and isinstance(text, tuple):
                            text = text[0]

                        is_final = True
                        try:
                            is_final_result = data.get_property_bool("is_final")
                            if isinstance(is_final_result, tuple):
                                is_final = is_final_result[0]
                            else:
                                is_final = is_final_result
                        except Exception:
                            pass

                        # Determine speaker - ASR transcripts are usually from user
                        # Agent transcripts come through different mechanism
                        speaker = "user"

                        if text and is_final:
                            ten_env.log_info(
                                f"[SENTINEL_TRANSCRIPT] Forwarding text_data: '{text[:100]}...'"
                            )
                            await self.sentinel_client.send_transcript(
                                speaker=speaker,
                                text=text,
                                is_final=is_final,
                            )
                    except Exception as e:
                        ten_env.log_debug(
                            f"[SENTINEL_TRANSCRIPT] Could not forward transcript: {e}"
                        )

        except Exception as e:
            ten_env.log_error(
                f"[THYMIA_TTS] Error handling data '{data.get_name()}': {e}"
            )

    # Counter for periodic logging
    _audio_frame_count = 0

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        """Process incoming audio frames"""
        try:
            self._audio_frame_count += 1

            # Get PCM data from audio frame
            buf = audio_frame.lock_buf()
            pcm_data = bytes(buf)
            audio_frame.unlock_buf(buf)

            # ============ SENTINEL MODE: Stream audio immediately ============
            if self.api_mode == "sentinel":
                # In Sentinel mode, stream every frame immediately (no local buffering)
                if self.sentinel_client and self.sentinel_client.is_connected:
                    # Stream user audio
                    await self.sentinel_client.send_audio(pcm_data, track="user")

                    # Log periodically
                    if self._audio_frame_count % 500 == 1:
                        status = self.sentinel_client.last_status
                        if status:
                            ten_env.log_info(
                                f"[SENTINEL_AUDIO] Streaming: "
                                f"buffer={status.buffer_duration:.1f}s, "
                                f"speech={status.speech_duration:.1f}s, "
                                f"results={self.sentinel_results_count}"
                            )
                        else:
                            ten_env.log_debug(
                                f"[SENTINEL_AUDIO] Streaming audio frame #{self._audio_frame_count}"
                            )
                else:
                    # Not connected yet - connection is async, should be ready soon
                    if self._audio_frame_count % 500 == 1:
                        ten_env.log_debug(
                            "[SENTINEL_AUDIO] Waiting for Sentinel connection to establish..."
                        )
                return  # Don't process with REST logic

            # ============ REST BATCH MODE (existing behavior) ============

            if not self.audio_buffer:
                ten_env.log_warn("[THYMIA_INIT] Audio buffer not initialized")
                return

            if not self.api_client:
                ten_env.log_warn("[THYMIA_INIT] API client not initialized")
                return

            # Add to buffer with VAD
            prev_speech_duration = self.audio_buffer.speech_duration
            speech_duration = self.audio_buffer.add_frame(pcm_data)
            actual_speech_duration = self.audio_buffer.actual_speech_duration

            # Track user speech activity (to avoid interrupting user with triggers)
            was_user_speaking = self.user_currently_speaking
            if speech_duration > prev_speech_duration:
                # User is actively speaking
                self.last_user_speech_time = time.time()
                self.user_currently_speaking = True
            elif time.time() - self.last_user_speech_time > 2.0:
                # No speech detected for 2 seconds - user finished speaking
                self.user_currently_speaking = False
                # If user just stopped speaking, check for deferred Sentinel announcements
                if was_user_speaking and self.api_mode == "sentinel":
                    asyncio.create_task(
                        self._process_deferred_sentinel_announcement(ten_env)
                    )

            # === SEPARATE LOGIC FOR EACH MODE ===

            if self.analysis_mode in ("hellos_only", "demo_dual"):
                # ============ DEMO_DUAL MODE (PARALLEL PHASES) ============
                # Hellos: Triggered at 30s mood speech
                # Apollo: Triggered at 52s total speech (30s mood + 22s reading)
                # These run independently - Apollo doesn't wait for Hellos to complete

                # Log buffer status every 5 seconds (stop when both analyses complete)
                if self._audio_frame_count % 500 == 1 and not (
                    self.hellos_complete and self.apollo_complete
                ):
                    hellos_status = (
                        "complete"
                        if self.hellos_complete
                        else (
                            "running"
                            if self.hellos_analysis_running
                            else "pending"
                        )
                    )
                    apollo_status = (
                        "complete"
                        if self.apollo_complete
                        else (
                            "running"
                            if self.apollo_analysis_running
                            else "pending"
                        )
                    )
                    ten_env.log_info(
                        f"[THYMIA_BUFFER] Actual speech: {actual_speech_duration:.1f}s (total with padding: {speech_duration:.1f}s) "
                        f"- hellos={hellos_status}, apollo={apollo_status}, "
                        f"mood_phase={self.mood_phase_complete}, reading_phase={self.reading_phase_complete}"
                    )

                # Check Hellos phase (uses min_speech_duration from config)
                if not self.hellos_complete:
                    required_duration = self.min_speech_duration

                    if (
                        self.audio_buffer.has_enough_speech(required_duration)
                        and not self.hellos_analysis_running
                    ):
                        # Mark mood phase complete
                        mood_phase_just_completed = False
                        if not self.mood_phase_complete:
                            self.mood_phase_complete = True
                            mood_phase_just_completed = True
                            ten_env.log_info(
                                f"[THYMIA_PHASE] Mood phase complete ({actual_speech_duration:.1f}s actual speech, {speech_duration:.1f}s total with padding)"
                            )

                        # Validate user info before starting
                        if (
                            not self.user_name
                            or not self.user_dob
                            or not self.user_sex
                        ):
                            if (
                                self._audio_frame_count % 100 == 1
                            ):  # Log every 1 second
                                ten_env.log_warn(
                                    f"[THYMIA_USERINFO] Waiting for user info before Hellos "
                                    f"(have: name={self.user_name}, dob={self.user_dob}, sex={self.user_sex})"
                                )
                        else:
                            ten_env.log_info(
                                f"[THYMIA_ANALYSIS_START] Starting Hellos analysis ({self.apollo_mood_duration}s mood threshold) "
                                f"({actual_speech_duration:.1f}s actual speech collected, {speech_duration:.1f}s total with padding)"
                            )
                            self.hellos_analysis_running = True
                            asyncio.create_task(self._run_hellos_phase(ten_env))

                        # If mood phase just completed, check if any APIs need triggering
                        if mood_phase_just_completed:
                            asyncio.create_task(
                                self._check_and_trigger_ready_announcements(
                                    ten_env
                                )
                            )

                # Check Apollo phase - INDEPENDENT of Hellos (skip in hellos_only mode)
                if self.analysis_mode == "hellos_only":
                    # In hellos_only mode, mark Apollo and reading phase as complete
                    # so we don't wait for them or send reading phase reminders
                    # Also mark apollo_trigger_sent and apollo_shared_with_user to prevent
                    # any Apollo announcements or retries
                    if not self.apollo_complete:
                        self.apollo_complete = True
                        self.reading_phase_complete = True
                        self.apollo_trigger_sent = (
                            True  # Prevent Apollo trigger
                        )
                        self.apollo_shared_with_user = (
                            True  # Prevent Apollo retries
                        )
                        ten_env.log_info(
                            "[THYMIA_MODE] hellos_only mode - skipping Apollo and reading phase"
                        )
                elif not self.apollo_complete:
                    required_duration = (
                        self.apollo_mood_duration + self.apollo_read_duration
                    )  # mood + read durations

                    if (
                        self.audio_buffer.has_enough_speech(required_duration)
                        and not self.apollo_analysis_running
                    ):
                        # Mark reading phase complete
                        reading_phase_just_completed = False
                        if not self.reading_phase_complete:
                            self.reading_phase_complete = True
                            reading_phase_just_completed = True
                            ten_env.log_info(
                                f"[THYMIA_PHASE] Reading phase complete ({actual_speech_duration:.1f}s actual speech, {speech_duration:.1f}s total with padding)"
                            )

                        # Check if we need to wait for Hellos to finish uploading first (avoid cancellation)
                        hellos_delay_needed = False
                        if (
                            hasattr(self, "hellos_session_start_time")
                            and self.hellos_session_start_time
                        ):
                            time_since_hellos_upload = (
                                time.time() - self.hellos_session_start_time
                            )
                            if time_since_hellos_upload < 5.0:
                                wait_time = 5.0 - time_since_hellos_upload
                                ten_env.log_info(
                                    f"[THYMIA_APOLLO_DELAY] Waiting {wait_time:.1f}s after Hellos upload to avoid API cancellation"
                                )
                                hellos_delay_needed = True

                        # Validate user info before starting
                        if (
                            not self.user_name
                            or not self.user_dob
                            or not self.user_sex
                        ):
                            if self._audio_frame_count % 100 == 1:
                                ten_env.log_warn(
                                    f"[THYMIA_USERINFO] Waiting for user info before Apollo "
                                    f"(have: name={self.user_name}, dob={self.user_dob}, sex={self.user_sex})"
                                )
                        elif not hellos_delay_needed:
                            ten_env.log_info(
                                f"[THYMIA_ANALYSIS_START] Starting Apollo analysis (52s total: 30s mood + 22s reading) "
                                f"({actual_speech_duration:.1f}s actual speech collected, {speech_duration:.1f}s total with padding)"
                            )
                            self.apollo_analysis_running = True
                            asyncio.create_task(self._run_apollo_phase(ten_env))

                        # If reading phase just completed, check if any APIs need triggering
                        if reading_phase_just_completed:
                            asyncio.create_task(
                                self._check_and_trigger_ready_announcements(
                                    ten_env
                                )
                            )
        except Exception as e:
            import traceback

            ten_env.log_error(
                f"[THYMIA_ERROR] Error in on_audio_frame: {e}\n{traceback.format_exc()}"
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

    def _calculate_actual_speech_seconds(
        self,
        pcm_data: bytes,
        sample_rate: int = 16000,
        silence_threshold: float = 500.0,
    ) -> float:
        """
        Calculate actual speech seconds in a PCM chunk based on RMS volume.

        Args:
            pcm_data: Raw PCM audio data
            sample_rate: Sample rate in Hz
            silence_threshold: RMS threshold for speech detection

        Returns:
            Seconds of actual speech (RMS > threshold)
        """
        import numpy as np

        if not pcm_data or len(pcm_data) < 2:
            return 0.0

        # Process in 20ms chunks (same as audio buffer frame size)
        chunk_size = int(sample_rate * 2 * 0.02)  # 20ms at 16kHz = 640 bytes
        actual_speech_seconds = 0.0

        for i in range(0, len(pcm_data) - chunk_size + 1, chunk_size):
            chunk = pcm_data[i : i + chunk_size]
            if len(chunk) % 2 != 0:
                chunk = chunk[:-1]
            if not chunk:
                continue

            # Calculate RMS
            samples = np.frombuffer(chunk, dtype=np.int16)
            rms = np.sqrt(np.mean(samples.astype(np.float64) ** 2))

            if rms > silence_threshold:
                actual_speech_seconds += 0.02  # 20ms

        return actual_speech_seconds

    async def _run_hellos_only_analysis(self, ten_env: AsyncTenEnv):
        """Run Hellos analysis for hellos_only mode"""
        self.active_analysis = True

        ten_env.log_info(
            f"[THYMIA_HELLOS_ONLY] Starting analysis - "
            f"User={self.user_name}, DOB={self.user_dob}, Sex={self.user_sex}"
        )

        try:
            wav_data = self.audio_buffer.get_wav_data()
            if not wav_data:
                ten_env.log_warn("[THYMIA_HELLOS_ONLY] No audio data available")
                return

            ten_env.log_info(
                f"[THYMIA_HELLOS_ONLY] Starting API workflow ({len(wav_data)} bytes)"
            )

            # Create session
            session_response = await self.api_client.create_session(
                user_label=self.user_name or "anonymous",
                date_of_birth=self.user_dob or "1990-01-01",
                birth_sex=self.user_sex or "UNSPECIFIED",
                locale=self.user_locale,
            )
            session_id = session_response["id"]
            upload_url = session_response["recordingUploadUrl"]

            ten_env.log_info(
                f"[THYMIA_HELLOS_ONLY] Created session: {session_id}"
            )

            # Upload audio
            upload_success = await self.api_client.upload_audio(
                upload_url, wav_data
            )
            if not upload_success:
                ten_env.log_error("[THYMIA_HELLOS_ONLY] Failed to upload audio")
                return

            # Poll for results
            results = await self.api_client.poll_results(
                session_id,
                max_wait_seconds=self.poll_timeout,
                poll_interval=self.poll_interval,
            )

            if not results:
                ten_env.log_warn(
                    f"[THYMIA_HELLOS_ONLY] Analysis timed out after {self.poll_timeout}s"
                )
                return

            # Extract metrics
            sections = results.get("results", {}).get("sections", [])
            if not sections:
                ten_env.log_error(
                    "[THYMIA_HELLOS_ONLY] No sections found in response"
                )
                return

            section = sections[0]
            self.latest_results = WellnessMetrics(
                distress=section.get("uniformDistress", {}).get("value", 0.0),
                stress=section.get("uniformStress", {}).get("value", 0.0),
                burnout=section.get("uniformExhaustion", {}).get("value", 0.0),
                fatigue=section.get("uniformSleepPropensity", {}).get(
                    "value", 0.0
                ),
                low_self_esteem=section.get("uniformLowSelfEsteem", {}).get(
                    "value", 0.0
                ),
                timestamp=time.time(),
                session_id=session_id,
            )

            self.analysis_count += 1
            self.last_analysis_time = time.time()

            ten_env.log_info(
                f"[THYMIA_HELLOS_ONLY] Analysis complete: "
                f"distress={self.latest_results.distress:.4f}, "
                f"stress={self.latest_results.stress:.4f}, "
                f"burnout={self.latest_results.burnout:.4f}, "
                f"fatigue={self.latest_results.fatigue:.4f}, "
                f"low_self_esteem={self.latest_results.low_self_esteem:.4f}"
            )

        except Exception as e:
            ten_env.log_error(f"[THYMIA_HELLOS_ONLY] Error: {e}")
            import traceback

            ten_env.log_error(traceback.format_exc())
        finally:
            self.active_analysis = False

    async def _run_hellos_phase(self, ten_env: AsyncTenEnv):
        """Start Hellos analysis session (create + upload). Polling handled by unified poller."""
        ten_env.log_info(
            f"[THYMIA_HELLOS_START] User={self.user_name}, DOB={self.user_dob}, Sex={self.user_sex}"
        )

        try:
            wav_data = self.audio_buffer.get_wav_data()

            if not wav_data:
                ten_env.log_warn(
                    "[THYMIA_HELLOS_PHASE_1] No audio data available"
                )
                return

            ten_env.log_info(
                f"[THYMIA_HELLOS_PHASE_1] Prepared {len(wav_data)} bytes of WAV data for upload (in-memory, no disk I/O)"
            )

            # Create session
            api_start_time = time.time()
            session_response = await self.api_client.create_session(
                user_label=self.user_name or "anonymous",
                date_of_birth=self.user_dob or "1990-01-01",
                birth_sex=self.user_sex or "UNSPECIFIED",
                locale=self.user_locale,
            )
            session_id = session_response["id"]
            upload_url = session_response["recordingUploadUrl"]
            session_time = time.time() - api_start_time

            ten_env.log_info(
                f"[THYMIA_HELLOS_PHASE_1] Created session: {session_id} (took {session_time:.2f}s)"
            )

            # Upload directly from memory (no disk I/O)
            upload_start_time = time.time()
            upload_success = await self.api_client.upload_audio(
                upload_url, wav_data
            )
            upload_time = time.time() - upload_start_time

            if not upload_success:
                ten_env.log_error(
                    "[THYMIA_HELLOS_PHASE_1] Failed to upload audio"
                )
                return

            ten_env.log_info(
                f"[THYMIA_HELLOS_PHASE_1] Uploaded audio (took {upload_time:.2f}s) - unified poller will check for results"
            )

            # Store session ID and start time - unified poller will poll for results
            self.hellos_session_id = session_id
            self.hellos_session_start_time = time.time()

            # Clear buffer if Apollo also sent (or is currently sending)
            if self.apollo_complete or self.apollo_analysis_running:
                ten_env.log_info(
                    "[THYMIA_BUFFER] Hellos uploaded and Apollo sent/sending - clearing buffer"
                )
                self.audio_buffer.clear_buffer()

            # IMPORTANT: Don't clear hellos_analysis_running here!
            # Keep it True to prevent re-triggering while unified poller waits for results.
            # The unified poller will clear it when hellos_complete=True.

        except Exception as e:
            ten_env.log_error(f"[THYMIA_HELLOS_PHASE_1] Error: {e}")
            import traceback

            ten_env.log_error(traceback.format_exc())
            # On error, clear flag so it can be retried
            self.hellos_analysis_running = False

    async def _run_apollo_phase(self, ten_env: AsyncTenEnv):
        """Run Apollo analysis for demo_dual mode (phase 2/2)"""
        ten_env.log_info(
            f"[THYMIA_APOLLO_START] User={self.user_name}, DOB={self.user_dob}, Sex={self.user_sex}"
        )

        try:
            # Split the main speech buffer using hardcoded duration (30s)
            # This avoids property loading issues that caused apollo_mood_duration to be 0.0
            if not self.audio_buffer.speech_buffer:
                ten_env.log_error(
                    "[THYMIA_APOLLO_PHASE_2] No audio data available"
                )
                return

            # Use config-based split duration
            mood_duration = self.apollo_mood_duration
            full_pcm_data = b"".join(self.audio_buffer.speech_buffer)
            mood_pcm, read_pcm = self._split_pcm_by_duration(
                full_pcm_data, mood_duration
            )

            # Calculate actual speech in each chunk
            mood_actual_speech = self._calculate_actual_speech_seconds(mood_pcm)
            read_actual_speech = self._calculate_actual_speech_seconds(read_pcm)
            mood_duration_total = len(mood_pcm) / 32000  # bytes to seconds
            read_duration_total = len(read_pcm) / 32000
            total_actual_speech = mood_actual_speech + read_actual_speech

            ten_env.log_info(
                f"[THYMIA_APOLLO_PHASE_2] Split at {mood_duration}s: "
                f"mood={mood_actual_speech:.1f}s speech, read={read_actual_speech:.1f}s speech, "
                f"TOTAL={total_actual_speech:.1f}s actual speech sent to Apollo"
            )

            # Call Apollo API directly with PCM data (no disk I/O needed)
            api_start_time = time.time()
            apollo_result = await self.apollo_client.analyze(
                mood_audio_pcm=mood_pcm,
                read_aloud_audio_pcm=read_pcm,
                user_label=self.user_name or "anonymous",
                date_of_birth=self.user_dob or "1990-01-01",
                birth_sex=self.user_sex or "FEMALE",
                sample_rate=16000,
                language="en-GB",
            )
            api_time = time.time() - api_start_time

            # Store Apollo results
            self.apollo_results = apollo_result

            if apollo_result.status == "COMPLETE_OK":
                ten_env.log_info(
                    f"[THYMIA_APOLLO_DONE] depression={apollo_result.depression_probability:.2%} (severity={apollo_result.depression_severity!r}), "
                    f"anxiety={apollo_result.anxiety_probability:.2%} (severity={apollo_result.anxiety_severity!r}) (API took {api_time:.1f}s)"
                )
            else:
                ten_env.log_warn(
                    f"[THYMIA_APOLLO_FAIL] {apollo_result.status}: {apollo_result.error_message} (took {api_time:.1f}s)"
                )

            # Mark Apollo API complete
            self.apollo_complete = True

            # Clear buffer if Hellos also sent
            if self.hellos_session_id:
                ten_env.log_info(
                    "[THYMIA_BUFFER] Apollo uploaded and Hellos sent - clearing buffer"
                )
                self.audio_buffer.clear_buffer()

            # Check if ready to announce
            if apollo_result.status == "COMPLETE_OK":
                ten_env.log_info(
                    "[THYMIA_APOLLO_DONE] Apollo API completed successfully - checking if ready to announce"
                )
                await self._check_and_trigger_ready_announcements(ten_env)

        except Exception as e:
            ten_env.log_error(f"[THYMIA_APOLLO_PHASE_2] Error: {e}")
            import traceback

            ten_env.log_error(traceback.format_exc())
        finally:
            self.apollo_analysis_running = False

    async def _run_analysis(self, ten_env: AsyncTenEnv):
        """Run Thymia analysis workflow in background (DEPRECATED - use specific methods)"""
        self.active_analysis = True

        ten_env.log_info(
            f"[THYMIA_ANALYSIS_START] Mode={self.analysis_mode}, "
            f"User={self.user_name}, DOB={self.user_dob}, Sex={self.user_sex}"
        )

        try:
            # Get raw PCM data from buffer
            if not self.audio_buffer.speech_buffer:
                ten_env.log_warn(
                    "[THYMIA_ANALYSIS] No audio data available for analysis"
                )
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
                f"[THYMIA_HELLOS] Starting Thymia API workflow ({len(wav_data)} bytes)"
            )

            # Step 1: Create session with user info
            ten_env.log_info(
                f"[THYMIA_HELLOS] Creating session for user={self.user_name}, "
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

            ten_env.log_info(f"[THYMIA_HELLOS] Created session: {session_id}")

            # Step 2: Upload audio
            ten_env.log_info(
                f"[THYMIA_HELLOS] Uploading {len(wav_data)} bytes of audio..."
            )
            upload_success = await self.api_client.upload_audio(
                upload_url, wav_data
            )

            if not upload_success:
                ten_env.log_error(
                    "[THYMIA_HELLOS] Failed to upload audio to Thymia"
                )
                return

            ten_env.log_info(
                f"[THYMIA_HELLOS] Audio uploaded successfully, polling for results (timeout={self.poll_timeout}s)..."
            )

            # Step 3: Poll for results
            results = await self.api_client.poll_results(
                session_id,
                max_wait_seconds=self.poll_timeout,
                poll_interval=self.poll_interval,
            )

            if not results:
                ten_env.log_warn(
                    f"[THYMIA_HELLOS] Analysis timed out after {self.poll_timeout}s"
                )
                return

            ten_env.log_info(
                f"[THYMIA_HELLOS] Received results for session {session_id}"
            )

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
                    "[THYMIA_APOLLO] Starting Apollo API analysis for depression/anxiety"
                )

                try:
                    # Split audio into mood and read segments
                    mood_pcm, read_pcm = self._split_pcm_by_duration(
                        full_pcm_data, self.apollo_mood_duration
                    )

                    ten_env.log_info(
                        f"[THYMIA_APOLLO] Split audio: mood={len(mood_pcm)} bytes "
                        f"({self.apollo_mood_duration}s), "
                        f"read={len(read_pcm)} bytes"
                    )

                    # Validate user info for Apollo
                    if not self.user_dob or not self.user_sex:
                        ten_env.log_warn(
                            "[THYMIA_APOLLO] Missing user DOB or sex, using defaults"
                        )

                    # Run Apollo analysis
                    ten_env.log_info(
                        f"[THYMIA_APOLLO] Starting depression/anxiety analysis for user={self.user_name}, "
                        f"dob={self.user_dob}, sex={self.user_sex}"
                    )
                    apollo_result = await self.apollo_client.analyze(
                        mood_audio_pcm=mood_pcm,
                        read_aloud_audio_pcm=read_pcm,
                        user_label=self.user_name or "anonymous",
                        date_of_birth=self.user_dob or "1990-01-01",
                        birth_sex=self.user_sex or "FEMALE",
                        sample_rate=16000,
                        language="en-GB",
                    )

                    # Store Apollo results
                    self.apollo_results = apollo_result

                    if apollo_result.status == "COMPLETE_OK":
                        ten_env.log_info(
                            f"[THYMIA_APOLLO] Analysis complete: "
                            f"depression={apollo_result.depression_probability:.2%} "
                            f"({apollo_result.depression_severity}), "
                            f"anxiety={apollo_result.anxiety_probability:.2%} "
                            f"({apollo_result.anxiety_severity})"
                        )
                    else:
                        ten_env.log_warn(
                            f"[THYMIA_APOLLO] Analysis failed: {apollo_result.status} - "
                            f"{apollo_result.error_message}"
                        )

                except Exception as apollo_error:
                    ten_env.log_error(
                        f"[THYMIA_APOLLO] Apollo API failed: {apollo_error}"
                    )
                    # Continue with Hellos-only results

            # Send proactive notification to LLM
            await self._notify_llm_of_results(ten_env)

            # Clear buffer if not doing continuous analysis
            if self.continuous_analysis:
                self.audio_buffer.clear()

        except Exception as e:
            ten_env.log_error(f"[THYMIA_ANALYZER] Thymia analysis failed: {e}")
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
                "[THYMIA_ANALYZER] Sent wellness notification to LLM"
            )

        except Exception as e:
            ten_env.log_error(
                f"[THYMIA_ANALYZER] Failed to send notification: {e}"
            )

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        """Register wellness analysis tools"""
        ten_env.log_info(
            f"[THYMIA_TOOL_METADATA] get_tool_metadata called - defining wellness analysis tools "
            f"(api_mode={self.api_mode}, analysis_mode={self.analysis_mode})"
        )

        # Different tool description based on API and analysis mode
        if self.api_mode == "sentinel":
            # Sentinel mode - real-time streaming, server handles buffering
            get_wellness_description = (
                "Get user's voice analysis results. "
                "Returns 5 KEY METRICS: "
                "- Wellness (from voice): stress, burnout, fatigue "
                "- Clinical (if available): depression, anxiety "
                "IMPORTANT: Call this when you receive a [SYSTEM ALERT]. "
                "INTERPRET NATURALLY - don't list numbers. Examples: "
                "- 'Your voice sounds relaxed, no signs of significant stress' "
                "- 'I'm picking up some fatigue - that matches what you said about sleep' "
                "- 'No indicators of depression or anxiety in your voice' "
                "If safety_classification shows alert='professional_referral' or 'crisis', follow recommended_actions. "
                "NOTE: Analysis starts after user provides name, sex, and year of birth via start_session."
            )
        elif self.analysis_mode == "hellos_only":
            get_wellness_description = (
                "Get user's mental wellness metrics from voice analysis. "
                "Returns wellness metrics (stress, distress, burnout, fatigue, low_self_esteem) as PERCENTAGES from 0-100. "
                "Values are integers (e.g., stress: 27%). "
                "The analysis is based on speech patterns and provides insight into the user's emotional and mental health state. "
                "IMPORTANT: Call this when the [SYSTEM ALERT] indicates wellness metrics are ready. "
                "Response status field indicates results: "
                "- 'insufficient_data': Still collecting - only mention if user asks about progress. "
                "- 'available': Wellness metrics ready. Announce all 5 metrics to the user. "
                "- 'analyzing': Analysis in progress - wait for results. "
                "CRITICAL: Do NOT use markdown formatting (no **, *, _, etc.) - use plain numbered lists or paragraphs only. "
                "CRITICAL: After announcing results, IMMEDIATELY call confirm_announcement tool with phase='hellos'. "
                "NOTE: User information must be set first using check_phase_progress with name, year_of_birth, and sex."
            )
        else:
            get_wellness_description = (
                "Get user's mental wellness and clinical indicators from voice analysis. "
                "Returns wellness metrics (stress, distress, burnout, fatigue, low_self_esteem) as PERCENTAGES from 0-100. "
                "May also return clinical indicators (depression, anxiety) if available, as probabilities from 0-100 with severity levels. "
                "Values are integers (e.g., stress: 27%, depression: 15%). "
                "The analysis is based on speech patterns and provides insight into the user's emotional and mental health state. "
                "IMPORTANT: Call this periodically to check if analysis has completed. "
                "Response status field indicates results: "
                "- 'insufficient_data': Still collecting - only mention if user asks about progress. "
                "- 'available': Wellness metrics ready. If 'clinical_indicators' field is PRESENT, announce all 7 metrics. If 'clinical_indicators' field is MISSING, Apollo isn't ready yet - announce ONLY wellness metrics and WAIT for [SYSTEM ALERT] about clinical indicators. DO NOT make up values when field is missing. "
                "- 'partial': WELLNESS METRICS UNAVAILABLE (API failed). ONLY clinical indicators available. "
                "  When status='partial', you MUST tell user: 'Wellness metrics (stress, distress, burnout, fatigue, low self-esteem) are currently unavailable due to API limitations.' "
                "  Then announce ONLY the clinical indicators (depression, anxiety) that ARE available. "
                "  DO NOT make up or infer wellness metric values - if status='partial' and wellness_metrics is null, they are UNAVAILABLE. "
                "Frame clinical indicators (depression/anxiety) as research indicators, not clinical diagnosis. "
                "CRITICAL: Do NOT use markdown formatting (no **, *, _, etc.) - use plain numbered lists or paragraphs only. "
                "CRITICAL: After announcing results, IMMEDIATELY call confirm_announcement tool with appropriate phase. "
                "NOTE: User information must be set first using set_user_info."
            )

        return [
            LLMToolMetadata(
                name="get_wellness_metrics",
                description=get_wellness_description,
                parameters=[],
            ),
            LLMToolMetadata(
                name="confirm_announcement",
                description=(
                    "REQUIRED: Call this immediately AFTER announcing wellness or clinical results to the user. "
                    "This confirms that you have delivered the results and allows the system to track completion. "
                    "Call with phase='hellos' after announcing the 5 wellness metrics (stress, distress, burnout, fatigue, low_self_esteem). "
                    "Call with phase='apollo' after announcing the 2 clinical indicators (depression, anxiety). "
                    "You MUST call this after every announcement - the system needs this confirmation."
                ),
                parameters=[
                    {
                        "name": "phase",
                        "type": "string",
                        "description": "Which results were announced: 'hellos' for wellness metrics, 'apollo' for clinical indicators",
                        "required": True,
                    },
                ],
            ),
            LLMToolMetadata(
                name="check_phase_progress" if self.api_mode != "sentinel" else "start_session",
                description=(
                    # Sentinel mode: simple session start
                    "REQUIRED: Call this once you have the user's name, sex, and year of birth. "
                    "This starts the voice analysis session with their demographic info. "
                    "Analysis will not begin until this is called with all three parameters. "
                    "Returns connection status and streaming progress."
                ) if self.api_mode == "sentinel" else (
                    # REST mode: phase tracking
                    "CRITICAL: Call this BEFORE moving to the next phase or declaring you're processing responses. "
                    "Returns current phase status (mood/reading), speech collected so far, and whether phase is complete. "
                    "You MUST check this before: "
                    "1) Moving from mood questions to reading phase "
                    "2) Saying 'I'm processing your responses' or similar "
                    "If phase is NOT complete, keep asking questions to collect more speech. "
                    "Only proceed when phase_complete=true. "
                    "IMPORTANT: When checking before reading phase, include user info parameters (name, year_of_birth, sex) for analysis."
                ),
                parameters=[
                    {
                        "name": "name",
                        "type": "string",
                        "description": "User's first name (required for Sentinel, optional for REST)",
                        "required": self.api_mode == "sentinel",
                    },
                    {
                        "name": "year_of_birth",
                        "type": "string",
                        "description": "User's year of birth (e.g. '1974', '1990')",
                        "required": self.api_mode == "sentinel",
                    },
                    {
                        "name": "sex",
                        "type": "string",
                        "description": "MALE, FEMALE, or OTHER",
                        "required": self.api_mode == "sentinel",
                    },
                    {
                        "name": "locale",
                        "type": "string",
                        "description": "Locale code (optional, e.g. en-US, en-GB)",
                        "required": False,
                    },
                ],
            ),
            LLMToolMetadata(
                name="test_announcement_system",
                description=(
                    "TEST TOOL: Send a test announcement to verify the text_data mechanism is working. "
                    "When called, sends a text_data message with code 'TEST-12345' that you should acknowledge. "
                    "Only call this when explicitly requested by the user to test the system."
                ),
                parameters=[],
            ),
        ]

    async def _trigger_hellos_announcement(self, ten_env: AsyncTenEnv) -> bool:
        """
        Send async trigger to LLM to announce Hellos results (success or failure).
        Returns True if announcement was sent, False if skipped.
        """
        try:
            await asyncio.sleep(1)  # Small delay to avoid race conditions

            # Conditional hint text based on whether Hellos succeeded
            if self.hellos_success:
                hint_text = "[SYSTEM ALERT] Wellness metrics ready. IMMEDIATELY call get_wellness_metrics and announce all 5 results (stress, distress, burnout, fatigue, low_self_esteem) to user."
            else:
                # Hellos failed - skip announcement entirely
                # LLM will discover wellness unavailable when it calls get_wellness_metrics
                ten_env.log_info(
                    "[THYMIA_TRIGGER] Skipping Hellos announcement - API failed, LLM will discover via get_wellness_metrics"
                )
                return False  # No announcement sent

            ten_env.log_info(
                f"[THYMIA_TRIGGER] Sending Hellos announcement to LLM: {hint_text}"
            )

            text_data = Data.create("text_data")
            text_data.set_property_string("text", hint_text)
            text_data.set_property_bool("end_of_segment", True)
            text_data.set_property_string(
                "role", "system"
            )  # Send as system message for better LLM compliance
            ten_env.log_info(
                f"[THYMIA-ANNOUNCEMENT] Sending (role=system): {hint_text[:100]}..."
            )
            await ten_env.send_data(text_data)

            ten_env.log_info(
                "[THYMIA_TRIGGER_OK] Hellos announcement sent to LLM via text_data (role=system)"
            )
            return True  # Announcement sent
        except Exception as e:
            ten_env.log_error(f"[THYMIA_TRIGGER_FAIL] Hellos: {e}")
            return False

    async def _trigger_apollo_announcement(self, ten_env: AsyncTenEnv):
        """Send async trigger to LLM to announce Apollo results"""
        try:
            await asyncio.sleep(1)  # Small delay to avoid race conditions
            hint_text = "[SYSTEM ALERT] Clinical indicators ready. IMMEDIATELY call get_wellness_metrics and announce depression and anxiety probabilities with severity to user."

            ten_env.log_info(
                f"[THYMIA_TRIGGER] Sending Apollo announcement to LLM: {hint_text}"
            )

            text_data = Data.create("text_data")
            text_data.set_property_string("text", hint_text)
            text_data.set_property_bool("end_of_segment", True)
            text_data.set_property_string(
                "role", "system"
            )  # Send as system message for better LLM compliance
            ten_env.log_info(
                f"[THYMIA-ANNOUNCEMENT] Sending (role=system): {hint_text[:100]}..."
            )
            await ten_env.send_data(text_data)

            ten_env.log_info(
                "[THYMIA_TRIGGER_OK] Apollo announcement sent to LLM via text_data (role=system)"
            )
        except Exception as e:
            ten_env.log_error(f"[THYMIA_TRIGGER_FAIL] Apollo: {e}")

    async def _unified_results_poller(self, ten_env: AsyncTenEnv):
        """
        Unified poller that:
        1. Polls Hellos API for results
        2. Polls Apollo API for results
        3. Sends announcement triggers when results ready
        4. Retries announcements if LLM hasn't confirmed

        Runs every 5 seconds from extension startup.
        """
        ten_env.log_info(
            "[THYMIA_UNIFIED_POLLER] Starting unified results poller (polls both APIs + handles announcements)"
        )

        while True:
            try:
                await asyncio.sleep(5.0)  # Poll every 5 seconds

                # ===== POLL HELLOS API =====
                if self.hellos_session_id and not self.hellos_complete:
                    elapsed = time.time() - self.hellos_session_start_time

                    # Check timeout
                    if elapsed > self.poll_timeout:
                        ten_env.log_warn(
                            f"[THYMIA_UNIFIED_POLLER] Hellos timeout after {elapsed:.1f}s, session={self.hellos_session_id}"
                        )
                        self.hellos_complete = True  # Mark as complete
                        self.hellos_analysis_running = False
                        # Trigger announcement even on timeout
                        await self._check_and_trigger_ready_announcements(
                            ten_env
                        )
                        continue

                    # Poll API
                    try:
                        result = await self.api_client.get_results(
                            self.hellos_session_id
                        )
                        if result:
                            status = result.get("status", "")
                            if status in (
                                "COMPLETE_OK",
                                "COMPLETE_ERROR",
                                "FAILED",
                            ):
                                ten_env.log_info(
                                    f"[THYMIA_UNIFIED_POLLER] Hellos complete: status={status}, elapsed={elapsed:.1f}s"
                                )

                                if status == "COMPLETE_OK":
                                    # Parse and store results
                                    sections = result.get("results", {}).get(
                                        "sections", []
                                    )
                                    if sections:
                                        section = sections[0]
                                        self.latest_results = WellnessMetrics(
                                            distress=section.get(
                                                "uniformDistress", {}
                                            ).get("value", 0.0),
                                            stress=section.get(
                                                "uniformStress", {}
                                            ).get("value", 0.0),
                                            burnout=section.get(
                                                "uniformExhaustion", {}
                                            ).get("value", 0.0),
                                            fatigue=section.get(
                                                "uniformSleepPropensity", {}
                                            ).get("value", 0.0),
                                            low_self_esteem=section.get(
                                                "uniformLowSelfEsteem", {}
                                            ).get("value", 0.0),
                                            timestamp=time.time(),
                                            session_id=self.hellos_session_id,
                                        )
                                        ten_env.log_info(
                                            f"[THYMIA_UNIFIED_POLLER] Hellos metrics: distress={self.latest_results.distress:.2f}, "
                                            f"stress={self.latest_results.stress:.2f}, burnout={self.latest_results.burnout:.2f}, "
                                            f"fatigue={self.latest_results.fatigue:.2f}, low_self_esteem={self.latest_results.low_self_esteem:.2f}"
                                        )
                                elif status in ("COMPLETE_ERROR", "FAILED"):
                                    # API failed - log error details and notify LLM immediately
                                    error_reason = result.get(
                                        "errorReason", "Unknown error"
                                    )
                                    error_code = result.get(
                                        "errorCode", "UNKNOWN"
                                    )
                                    ten_env.log_error(
                                        f"[THYMIA_UNIFIED_POLLER] Hellos API FAILED: {error_code} - {error_reason}"
                                    )
                                    # Announcement will be sent via normal trigger flow

                                self.hellos_complete = True
                                self.hellos_analysis_running = (
                                    False  # Clear flag - allow next analysis
                                )

                                # Check if ready to announce (only if COMPLETE_OK)
                                if status == "COMPLETE_OK":
                                    self.hellos_success = (
                                        True  # Mark as successful
                                    )
                                    ten_env.log_info(
                                        "[THYMIA_HELLOS_DONE] Hellos API completed successfully - checking if ready to announce"
                                    )
                                    await self._check_and_trigger_ready_announcements(
                                        ten_env
                                    )
                    except Exception as e:
                        ten_env.log_error(
                            f"[THYMIA_UNIFIED_POLLER] Hellos polling error: {e}"
                        )

                # ===== APOLLO HANDLED BY ITS OWN ASYNC TASK =====
                # Apollo's analyze() method is self-contained (create+upload+poll in one call)
                # The _run_apollo_phase async task handles it completely and sets apollo_complete flag
                # No polling needed here - just check if results are ready to announce
                if (
                    self.apollo_complete
                    and self.apollo_results
                    and not self.apollo_trigger_sent
                ):
                    # Apollo just completed - check if ready to announce
                    await self._check_and_trigger_ready_announcements(ten_env)

                # ===== RETRY ANNOUNCEMENTS IF NEEDED (MAX ONCE EVERY 30s) =====
                current_time = time.time()

                # Check for timeout or max retries for Hellos
                if (
                    self.hellos_trigger_sent
                    and not self.hellos_shared_with_user
                    and self.latest_results is not None
                ):
                    time_since_first_announcement = (
                        current_time - self.hellos_last_announcement_time
                    )

                    # Timeout fallback - force complete after 90s
                    if (
                        time_since_first_announcement
                        >= self.announcement_timeout
                    ):
                        ten_env.log_warn(
                            f"[THYMIA_UNIFIED_POLLER] Hellos announcement timeout ({self.announcement_timeout}s) - marking as shared"
                        )
                        self.hellos_shared_with_user = True
                    # Max retries exceeded
                    elif (
                        self.hellos_retry_count >= self.max_announcement_retries
                    ):
                        ten_env.log_warn(
                            f"[THYMIA_UNIFIED_POLLER] Hellos max retries ({self.max_announcement_retries}) exceeded - giving up"
                        )
                        self.hellos_shared_with_user = True
                    # Retry if interval elapsed AND agent not speaking
                    elif (
                        not self.user_currently_speaking
                        and time_since_first_announcement
                        >= self.announcement_retry_interval
                    ):
                        # Check if agent is still speaking (using timestamp-based check)
                        if (
                            self.agent_speaking_until > 0
                            and current_time < self.agent_speaking_until
                        ):
                            remaining_seconds = (
                                self.agent_speaking_until - current_time
                            )
                            ten_env.log_info(
                                f"[THYMIA_UNIFIED_POLLER] Skipping Hellos retry - agent still speaking "
                                f"(will finish in {remaining_seconds:.1f}s)"
                            )
                        else:
                            ten_env.log_info(
                                f"[THYMIA_UNIFIED_POLLER] Retrying Hellos announcement (attempt {self.hellos_retry_count + 1}/{self.max_announcement_retries}, last sent {time_since_first_announcement:.0f}s ago)"
                            )
                            await self._trigger_hellos_announcement(ten_env)
                            self.hellos_last_announcement_time = current_time
                            self.hellos_retry_count += 1

                # Check for timeout or max retries for Apollo
                if (
                    self.apollo_trigger_sent
                    and not self.apollo_shared_with_user
                ):
                    time_since_first_announcement = (
                        current_time - self.apollo_last_announcement_time
                    )

                    # Timeout fallback - force complete after 90s
                    if (
                        time_since_first_announcement
                        >= self.announcement_timeout
                    ):
                        ten_env.log_warn(
                            f"[THYMIA_UNIFIED_POLLER] Apollo announcement timeout ({self.announcement_timeout}s) - marking as shared"
                        )
                        self.apollo_shared_with_user = True
                    # Max retries exceeded
                    elif (
                        self.apollo_retry_count >= self.max_announcement_retries
                    ):
                        ten_env.log_warn(
                            f"[THYMIA_UNIFIED_POLLER] Apollo max retries ({self.max_announcement_retries}) exceeded - giving up"
                        )
                        self.apollo_shared_with_user = True
                    # Retry if interval elapsed AND agent not speaking
                    elif (
                        not self.user_currently_speaking
                        and time_since_first_announcement
                        >= self.announcement_retry_interval
                    ):
                        # Check if agent is still speaking (using timestamp-based check)
                        if (
                            self.agent_speaking_until > 0
                            and current_time < self.agent_speaking_until
                        ):
                            remaining_seconds = (
                                self.agent_speaking_until - current_time
                            )
                            ten_env.log_info(
                                f"[THYMIA_UNIFIED_POLLER] Skipping Apollo retry - agent still speaking "
                                f"(will finish in {remaining_seconds:.1f}s)"
                            )
                        else:
                            ten_env.log_info(
                                f"[THYMIA_UNIFIED_POLLER] Retrying Apollo announcement (attempt {self.apollo_retry_count + 1}/{self.max_announcement_retries}, last sent {time_since_first_announcement:.0f}s ago)"
                            )
                            await self._trigger_apollo_announcement(ten_env)
                            self.apollo_last_announcement_time = current_time
                            self.apollo_retry_count += 1

                # ===== PERIODIC PROGRESS CHECKER =====
                # Check if user has been silent for a while and phases aren't complete
                # If so, send a hint to LLM to prompt user to continue speaking/reading
                if self.user_name is not None:  # Only if user info has been set
                    time_since_last_speech = (
                        current_time - self.last_user_speech_time
                    )

                    # Check if agent is still speaking (timestamp-based)
                    agent_still_speaking = (
                        self.agent_speaking_until > 0
                        and current_time < self.agent_speaking_until
                    ) or self.agent_currently_speaking

                    # Check if user has been silent for 15+ seconds AND agent is not speaking
                    if (
                        time_since_last_speech > 15.0
                        and not self.user_currently_speaking
                        and not agent_still_speaking
                    ):
                        # Check if mood phase is incomplete
                        total_required = (
                            self.apollo_mood_duration
                            + self.apollo_read_duration
                        )
                        current_speech = (
                            self.audio_buffer.speech_duration
                            if self.audio_buffer
                            else 0
                        )
                        if (
                            not self.mood_phase_complete
                            and current_speech < self.apollo_mood_duration
                        ):
                            speech_remaining = (
                                self.apollo_mood_duration - current_speech
                            )
                            hint_text = f"[PROGRESS CHECK] User has provided {current_speech:.0f}s of speech. Need {speech_remaining:.0f}s more for mood analysis. User has been silent for {time_since_last_speech:.0f}s. Ask them to continue sharing about their day, feelings, or interests."

                            ten_env.log_info(
                                f"[THYMIA_PROGRESS_CHECK] Sending mood phase progress reminder: {current_speech:.0f}s/{self.apollo_mood_duration:.0f}s collected"
                            )

                            text_data = Data.create("text_data")
                            text_data.set_property_string("text", hint_text)
                            text_data.set_property_bool("end_of_segment", True)
                            ten_env.log_info(
                                f"[THYMIA-ANNOUNCEMENT] Sending: {hint_text[:100]}..."
                            )
                            await ten_env.send_data(text_data)

                        # Check if reading phase is incomplete (mood phase done)
                        # Note: In hellos_only mode, reading_phase_complete is set to True early,
                        # so this block won't execute for hellos_only
                        elif (
                            self.mood_phase_complete
                            and not self.reading_phase_complete
                            and current_speech < total_required
                        ):
                            speech_remaining = total_required - current_speech
                            hint_text = f"[PROGRESS CHECK] User has provided {current_speech:.0f}s of speech. Need {speech_remaining:.0f}s more for reading analysis. User has been silent for {time_since_last_speech:.0f}s. Ask them to continue reading aloud from text on their screen."

                            ten_env.log_info(
                                f"[THYMIA_PROGRESS_CHECK] Sending reading phase progress reminder: {current_speech:.0f}s/{total_required:.0f}s collected"
                            )

                            text_data = Data.create("text_data")
                            text_data.set_property_string("text", hint_text)
                            text_data.set_property_bool("end_of_segment", True)
                            ten_env.log_info(
                                f"[THYMIA-ANNOUNCEMENT] Sending: {hint_text[:100]}..."
                            )
                            await ten_env.send_data(text_data)

            except asyncio.CancelledError:
                ten_env.log_info(
                    "[THYMIA_UNIFIED_POLLER] Task cancelled, stopping"
                )
                break
            except Exception as e:
                ten_env.log_error(
                    f"[THYMIA_UNIFIED_POLLER] Unexpected error: {e}"
                )
                import traceback

                ten_env.log_error(traceback.format_exc())
                # Continue running despite errors

    async def _delayed_announcement_check(
        self, ten_env: AsyncTenEnv, delay_seconds: float
    ):
        """
        Wait for delay then check for pending announcements.
        Used to trigger Hellos after Apollo finishes speaking.
        """
        try:
            await asyncio.sleep(delay_seconds)
            ten_env.log_debug(
                f"[THYMIA_DELAYED_CHECK] Checking for pending announcements after {delay_seconds:.1f}s delay"
            )
            await self._check_and_trigger_ready_announcements(ten_env)
        except asyncio.CancelledError:
            pass  # Task cancelled, ignore
        except Exception as e:
            ten_env.log_error(
                f"[THYMIA_DELAYED_CHECK] Error in delayed check: {e}"
            )

    async def _check_and_trigger_ready_announcements(
        self, ten_env: AsyncTenEnv
    ):
        """
        Check if APIs have completed and trigger announcements immediately.

        This is called:
        1. When Hellos API completes
        2. When Apollo API completes
        3. Periodically by unified poller

        IMPORTANT: Only sends triggers when user is NOT currently speaking to avoid interruption.
        IMPORTANT: Phases (mood/reading) are about AUDIO CAPTURE, not API completion!
        """
        # Log current state for debugging
        ten_env.log_debug(
            f"[THYMIA_TRIGGER_CHECK] State: hellos_complete={self.hellos_complete}, "
            f"apollo_complete={self.apollo_complete}, "
            f"hellos_trigger_sent={self.hellos_trigger_sent}, "
            f"apollo_trigger_sent={self.apollo_trigger_sent}, "
            f"user_speaking={self.user_currently_speaking}, "
            f"mood_phase={self.mood_phase_complete}, "
            f"reading_phase={self.reading_phase_complete}"
        )

        # Don't interrupt user or agent if they're currently speaking
        if self.user_currently_speaking:
            ten_env.log_info(
                "[THYMIA_TRIGGER_CHECK] Skipping trigger - user currently speaking"
            )
            return

        # Check if agent is still speaking (using timestamp-based check)
        current_time = time.time()
        if (
            self.agent_speaking_until > 0
            and current_time < self.agent_speaking_until
        ):
            remaining_seconds = self.agent_speaking_until - current_time
            ten_env.log_info(
                f"[THYMIA_TRIGGER_CHECK] Skipping trigger - agent still speaking "
                f"(will finish in {remaining_seconds:.1f}s, until timestamp {self.agent_speaking_until:.2f})"
            )
            return
        elif self.agent_currently_speaking:
            # Fallback for immediate speaking detection (tts_audio_start)
            ten_env.log_info(
                "[THYMIA_TRIGGER_CHECK] Skipping trigger - agent currently speaking (tts_audio_start)"
            )
            return

        # Trigger Hellos if ready and READING PHASE COMPLETE (both phases done)
        # Hellos API is called at mood phase, but results announced after both phases complete
        if (
            self.hellos_complete
            and self.reading_phase_complete
            and not self.hellos_trigger_sent
        ):
            # Check spacing from last announcement (15s minimum between any announcements)
            time_since_last = time.time() - max(
                self.hellos_last_announcement_time,
                self.apollo_last_announcement_time,
            )
            if time_since_last < ANNOUNCEMENT_MIN_SPACING_SECONDS:
                ten_env.log_debug(
                    f"[THYMIA_PHASE_TRIGGER] Hellos ready but waiting {ANNOUNCEMENT_MIN_SPACING_SECONDS - time_since_last:.1f}s after last announcement"
                )
            else:
                ten_env.log_info(
                    "[THYMIA_PHASE_TRIGGER] Triggering Hellos announcement (API complete, reading phase complete, user silent)"
                )
                self.hellos_trigger_sent = True  # Mark as processed before trigger to prevent race condition
                announcement_sent = await self._trigger_hellos_announcement(
                    ten_env
                )
                if announcement_sent:
                    self.hellos_last_announcement_time = (
                        time.time()
                    )  # Only set time if actually sent
                    # Apollo will trigger separately after 15s spacing
        elif self.hellos_complete and self.hellos_trigger_sent:
            ten_env.log_debug(
                "[THYMIA_TRIGGER_CHECK] Hellos trigger already sent previously"
            )
        elif not self.hellos_complete:
            ten_env.log_debug(
                "[THYMIA_TRIGGER_CHECK] Hellos API not yet complete"
            )

        # Trigger Apollo if ready and not yet triggered
        # Use consistent spacing between any announcements
        # Skip Apollo trigger in hellos_only mode - there's no Apollo analysis
        if (
            self.analysis_mode != "hellos_only"
            and self.apollo_complete
            and self.reading_phase_complete
            and not self.apollo_trigger_sent
        ):
            # Check spacing from last announcement (15s minimum between any announcements)
            time_since_last = time.time() - max(
                self.hellos_last_announcement_time,
                self.apollo_last_announcement_time,
            )

            if time_since_last >= ANNOUNCEMENT_MIN_SPACING_SECONDS:
                ten_env.log_info(
                    "[THYMIA_PHASE_TRIGGER] Triggering Apollo announcement (API complete, user silent)"
                )
                self.apollo_trigger_sent = (
                    True  # Set before trigger to prevent race condition
                )
                await self._trigger_apollo_announcement(ten_env)
                self.apollo_last_announcement_time = time.time()
            else:
                ten_env.log_debug(
                    f"[THYMIA_PHASE_TRIGGER] Apollo ready but waiting {ANNOUNCEMENT_MIN_SPACING_SECONDS - time_since_last:.1f}s after last announcement"
                )
        elif self.apollo_complete and self.apollo_trigger_sent:
            ten_env.log_debug(
                "[THYMIA_TRIGGER_CHECK] Apollo trigger already sent previously"
            )
        elif not self.apollo_complete:
            ten_env.log_debug(
                "[THYMIA_TRIGGER_CHECK] Apollo API not yet complete"
            )

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
        ten_env.log_info(f"[THYMIA_TOOL_CALL] {name}")

        try:
            # Handle get_wellness_metrics tool
            if name == "get_wellness_metrics":
                # ============ SENTINEL MODE ============
                if self.api_mode == "sentinel":
                    ten_env.log_info(
                        f"[SENTINEL_TOOL] get_wellness_metrics - "
                        f"results_count={self.sentinel_results_count}, "
                        f"has_wellness={self.sentinel_wellness is not None}, "
                        f"has_apollo={self.sentinel_apollo is not None}, "
                        f"has_safety={self.sentinel_safety is not None}"
                    )

                    # Get status from Sentinel client
                    status_msg = self.sentinel_client.last_status if self.sentinel_client else None
                    buffer_duration = status_msg.buffer_duration if status_msg else 0.0
                    speech_duration = status_msg.speech_duration if status_msg else 0.0

                    # Use ResultMapper to format response
                    response_data = ResultMapper.format_tool_response(
                        wellness=self.sentinel_wellness,
                        apollo=self.sentinel_apollo,
                        safety=self.sentinel_safety,
                        analysis_mode="real_time",
                        buffer_duration=buffer_duration,
                        speech_duration=speech_duration,
                        results_count=self.sentinel_results_count,
                        analysis_type=(
                            self.sentinel_latest_result.analysis_type
                            if self.sentinel_latest_result
                            else None
                        ),
                    )

                    ten_env.log_info(
                        f"[SENTINEL_TOOL_RESPONSE] {json.dumps(response_data)}"
                    )

                    return LLMToolResultLLMResult(
                        type="llmresult",
                        content=json.dumps(response_data),
                    )

                # ============ REST BATCH MODE (existing behavior) ============
                ten_env.log_info(
                    f"[THYMIA_TOOL_CALL] get_wellness_metrics - "
                    f"active_analysis={self.active_analysis}, "
                    f"has_results={self.latest_results is not None}, "
                    f"has_apollo={self.apollo_results is not None and self.apollo_results.status == 'COMPLETE_OK' if self.apollo_results else False}, "
                    f"speech_duration={self.audio_buffer.speech_duration if self.audio_buffer else 0:.1f}s"
                )

                # Check if we have ANY results (Hellos OR Apollo)
                has_hellos = self.latest_results is not None
                has_apollo = (
                    self.apollo_results is not None
                    and self.apollo_results.status == "COMPLETE_OK"
                )

                # If we have Apollo but not Hellos, return Apollo-only results
                if not has_hellos and has_apollo:
                    response_data = {
                        "status": "partial",
                        "message": "WELLNESS METRICS UNAVAILABLE (API failed). Only clinical indicators available.",
                        "wellness_metrics": None,
                        "clinical_indicators": {
                            "depression": {
                                "probability": round(
                                    self.apollo_results.depression_probability
                                    * 100
                                ),
                                "severity": self.apollo_results.depression_severity,
                            },
                            "anxiety": {
                                "probability": round(
                                    self.apollo_results.anxiety_probability
                                    * 100
                                ),
                                "severity": self.apollo_results.anxiety_severity,
                            },
                        },
                    }
                    ten_env.log_info(
                        "[THYMIA_APOLLO_ONLY] Returning Apollo-only results (Hellos FAILED - wellness metrics unavailable)"
                    )
                    # Log the exact JSON being returned to LLM for debugging
                    ten_env.log_info(
                        f"[THYMIA_TOOL_RESPONSE] JSON returned to LLM: {json.dumps(response_data)}"
                    )
                    return LLMToolResultLLMResult(
                        type="llmresult",
                        content=json.dumps(response_data),
                    )

                # Check if we have Hellos results
                if not has_hellos:
                    # Calculate required duration based on mode
                    required_duration = self.min_speech_duration
                    if self.analysis_mode == "demo_dual":
                        required_duration = (
                            self.apollo_mood_duration
                            + self.apollo_read_duration
                        )

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
                        "distress": round(self.latest_results.distress * 100),
                        "stress": round(self.latest_results.stress * 100),
                        "burnout": round(self.latest_results.burnout * 100),
                        "fatigue": round(self.latest_results.fatigue * 100),
                        "low_self_esteem": round(
                            self.latest_results.low_self_esteem * 100
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
                        f"[THYMIA_APOLLO_RETURNED_TO_LLM] Added clinical indicators: "
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

            # Handle confirm_announcement tool
            if name == "confirm_announcement":
                phase = args.get("phase", "").lower()
                ten_env.log_info(
                    f"[THYMIA_TOOL_CALL] confirm_announcement - phase={phase}"
                )

                if phase == "hellos":
                    self.hellos_shared_with_user = True
                    ten_env.log_info(
                        "[THYMIA_ANNOUNCEMENT_CONFIRMED] Hellos wellness metrics announced to user"
                    )
                elif phase == "apollo":
                    self.apollo_shared_with_user = True
                    ten_env.log_info(
                        "[THYMIA_ANNOUNCEMENT_CONFIRMED] Apollo clinical indicators announced to user"
                    )
                else:
                    ten_env.log_warn(
                        f"[THYMIA_ANNOUNCEMENT_CONFIRM_FAIL] Unknown phase: {phase}"
                    )

                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=json.dumps(
                        {
                            "status": "confirmed",
                            "phase": phase,
                            "message": f"Announcement confirmation recorded for {phase}",
                        }
                    ),
                )

            # Handle check_phase_progress tool (also accepts "start_session" alias for Sentinel mode)
            if name in ("check_phase_progress", "start_session"):
                ten_env.log_info(f"[THYMIA_TOOL_CALL] {name}")

                # Extract and store user info if provided
                if args.get("name"):
                    self.user_name = args.get("name", "").strip()
                    year_of_birth = args.get("year_of_birth", "").strip()
                    self.user_sex = args.get("sex", "").strip().upper()
                    self.user_locale = args.get("locale", "en-GB").strip()

                    # Convert year to YYYY-01-01 format
                    if year_of_birth:
                        import re

                        year_match = re.search(r"(\d{4})", year_of_birth)
                        if year_match:
                            year = year_match.group(1)
                            self.user_dob = f"{year}-01-01"
                            ten_env.log_info(
                                f"[THYMIA_USER_INFO] Captured from check_phase_progress: {self.user_name}, {self.user_dob}, {self.user_sex}"
                            )
                        else:
                            self.user_dob = "1990-01-01"

                    # Validate sex value
                    if self.user_sex and self.user_sex not in [
                        "MALE",
                        "FEMALE",
                        "OTHER",
                    ]:
                        self.user_sex = "OTHER"

                    # ============ SENTINEL MODE: Connect when user info is available ============
                    if self.api_mode == "sentinel" and self.sentinel_client:
                        if not self.sentinel_client.is_connected:
                            ten_env.log_info(
                                "[SENTINEL_CONNECT] User info available, connecting to Sentinel..."
                            )
                            asyncio.create_task(
                                self._connect_sentinel_with_user_info(ten_env)
                            )

                # ============ SENTINEL MODE: Return streaming status ============
                if self.api_mode == "sentinel":
                    status_msg = self.sentinel_client.last_status if self.sentinel_client else None
                    buffer_duration = status_msg.buffer_duration if status_msg else 0.0
                    speech_duration = status_msg.speech_duration if status_msg else 0.0
                    is_connected = self.sentinel_client.is_connected if self.sentinel_client else False

                    response = ResultMapper.format_phase_progress_response(
                        buffer_duration=buffer_duration,
                        speech_duration=speech_duration,
                        results_received=self.sentinel_results_count > 0,
                        analysis_type=(
                            self.sentinel_latest_result.analysis_type
                            if self.sentinel_latest_result
                            else None
                        ),
                        is_connected=is_connected,
                    )

                    ten_env.log_info(
                        f"[SENTINEL_PHASE_PROGRESS] {json.dumps(response)}"
                    )

                    return LLMToolResultLLMResult(
                        type="llmresult",
                        content=json.dumps(response),
                    )

                # ============ REST BATCH MODE (existing behavior) ============
                # Determine current phase and requirements
                if self.analysis_mode == "hellos_only":
                    required_duration = self.min_speech_duration
                    speech_collected = (
                        self.audio_buffer.actual_speech_duration
                        if self.audio_buffer
                        else 0.0
                    )

                    # Check if analysis already started (buffer may be cleared)
                    if self.hellos_analysis_running or self.mood_phase_complete:
                        current_phase = "analyzing"
                        phase_complete = True
                        message = "Analysis in progress - wait for results"
                    elif speech_collected >= required_duration:
                        current_phase = "collection"
                        phase_complete = True
                        message = "Phase complete - ready for analysis"
                    else:
                        current_phase = "collection"
                        phase_complete = False
                        message = f"Need {round(required_duration - speech_collected, 1)} more seconds of speech"

                    response = {
                        "mode": "hellos_only",
                        "current_phase": current_phase,
                        "speech_collected_seconds": round(speech_collected, 1),
                        "speech_required_seconds": round(required_duration, 1),
                        "speech_remaining_seconds": round(
                            max(0, required_duration - speech_collected), 1
                        ),
                        "phase_complete": phase_complete,
                        "message": message,
                    }

                elif self.analysis_mode == "demo_dual":
                    if not self.mood_phase_complete:
                        current_phase = "mood"
                        required_duration = self.min_speech_duration
                        speech_collected = (
                            self.audio_buffer.actual_speech_duration
                            if self.audio_buffer
                            else 0.0
                        )
                        phase_complete = speech_collected >= required_duration
                        next_action = "Keep asking about mood, feelings, interests, or day"

                        # Update mood phase flag and trigger Hellos when complete
                        if phase_complete and not self.mood_phase_complete:
                            self.mood_phase_complete = True
                            ten_env.log_info(
                                "[THYMIA_PHASE] Mood phase marked complete by check_phase_progress tool"
                            )

                            # Trigger Hellos analysis if user info ready
                            if (
                                self.user_name
                                and self.user_dob
                                and self.user_sex
                                and not self.hellos_analysis_running
                            ):
                                ten_env.log_info(
                                    f"[THYMIA_ANALYSIS_START] Starting Hellos from check_phase_progress ({speech_collected:.1f}s actual speech)"
                                )
                                self.hellos_analysis_running = True
                                asyncio.create_task(
                                    self._run_hellos_phase(ten_env)
                                )

                    elif not self.reading_phase_complete:
                        current_phase = "reading"
                        required_duration = (
                            self.apollo_mood_duration
                            + self.apollo_read_duration
                        )  # 60s total
                        speech_collected = (
                            self.audio_buffer.actual_speech_duration
                            if self.audio_buffer
                            else 0.0
                        )
                        phase_complete = speech_collected >= required_duration
                        next_action = (
                            "Ask user to read aloud text from screen or book"
                        )

                        # Update reading phase flag and trigger Apollo when complete
                        if phase_complete and not self.reading_phase_complete:
                            self.reading_phase_complete = True
                            ten_env.log_info(
                                f"[THYMIA_PHASE] Reading phase marked complete by check_phase_progress tool ({speech_collected:.1f}s total collected)"
                            )

                            # Trigger Apollo analysis if not already running
                            if not self.apollo_analysis_running:
                                ten_env.log_info(
                                    f"[THYMIA_ANALYSIS_START] Starting Apollo analysis (phase 2/2) ({speech_collected:.1f}s speech collected)"
                                )
                                self.apollo_analysis_running = True
                                asyncio.create_task(
                                    self._run_apollo_phase(ten_env)
                                )

                            # Check if any API results are ready for announcement
                            asyncio.create_task(
                                self._check_and_trigger_ready_announcements(
                                    ten_env
                                )
                            )

                    else:
                        current_phase = "complete"
                        required_duration = (
                            self.apollo_mood_duration
                            + self.apollo_read_duration
                        )
                        speech_collected = (
                            self.audio_buffer.actual_speech_duration
                            if self.audio_buffer
                            else 0.0
                        )
                        phase_complete = True
                        next_action = (
                            "Both phases complete - wait for analysis results"
                        )

                    response = {
                        "mode": "demo_dual",
                        "current_phase": current_phase,
                        "mood_phase_complete": self.mood_phase_complete,
                        "reading_phase_complete": self.reading_phase_complete,
                        "speech_collected_seconds": round(speech_collected, 1),
                        "speech_required_seconds": round(required_duration, 1),
                        "speech_remaining_seconds": round(
                            max(0, required_duration - speech_collected), 1
                        ),
                        "phase_complete": phase_complete,
                        "next_action": next_action,
                        "message": (
                            f"Phase '{current_phase}': need {round(required_duration - speech_collected, 1)} more seconds"
                            if not phase_complete
                            else f"Phase '{current_phase}' complete"
                        ),
                    }
                else:
                    response = {"error": "Unknown analysis mode"}

                ten_env.log_info(
                    f"[THYMIA_PHASE_PROGRESS] {json.dumps(response)}"
                )

                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=json.dumps(response),
                )

            # Handle test_announcement_system tool
            if name == "test_announcement_system":
                ten_env.log_info("[THYMIA_TEST] Sending test announcement")

                # Send test text_data message
                test_data = Data.create("text_data")
                test_data.set_property_string(
                    "text",
                    "TEST ANNOUNCEMENT: If you receive this message, please acknowledge with code TEST-12345.",
                )
                test_data.set_property_bool("end_of_segment", True)
                await ten_env.send_data(test_data)

                ten_env.log_info("[THYMIA_TEST] Test announcement sent")

                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=json.dumps(
                        {
                            "status": "sent",
                            "message": "Test announcement sent via text_data. Wait for LLM acknowledgment.",
                        }
                    ),
                )

        except Exception as e:
            ten_env.log_error(f"[THYMIA_ERROR] Error in run_tool: {e}")
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps(
                    {
                        "status": "error",
                        "message": "Wellness analysis service temporarily unavailable",
                    }
                ),
            )
