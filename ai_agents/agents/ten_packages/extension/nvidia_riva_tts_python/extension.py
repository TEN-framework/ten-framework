#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
NVIDIA Riva TTS Extension

This extension implements text-to-speech using NVIDIA Riva Speech Skills.
It provides high-quality, GPU-accelerated speech synthesis.
"""

import time
import traceback
from typing import Optional

from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleType,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension, RequestState
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
from ten_runtime import AsyncTenEnv

from .config import NvidiaRivaTTSConfig
from .riva_tts import NvidiaRivaTTSClient


class NvidiaRivaTTSExtension(AsyncTTS2BaseExtension):
    """
    NVIDIA Riva TTS Extension implementation.

    Provides text-to-speech synthesis using NVIDIA Riva's gRPC API.
    Inherits all common TTS functionality from AsyncTTS2BaseExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: Optional[NvidiaRivaTTSConfig] = None
        self.client: Optional[NvidiaRivaTTSClient] = None
        self.current_request_id: Optional[str] = None
        self.request_start_ts: float = 0
        self.first_chunk_ts: float = 0
        self.request_total_audio_duration: int = 0
        self.flush_request_id: Optional[str] = None
        self.last_end_request_id: Optional[str] = None
        self.audio_start_sent: set[str] = set()

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        """Initialize the extension"""
        await super().on_init(ten_env)
        ten_env.log_debug("NVIDIA Riva TTS on_init")

        try:
            # Load configuration
            config_json, _ = await ten_env.get_property_to_json("")
            self.config = NvidiaRivaTTSConfig.model_validate_json(config_json)

            ten_env.log_info(
                f"config: {self.config.model_dump_json()}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            # Create client
            self.client = NvidiaRivaTTSClient(
                config=self.config,
                ten_env=ten_env,
            )

        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info={"vendor": "nvidia_riva"},
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        """Stop the extension"""
        await super().on_stop(ten_env)
        ten_env.log_debug("NVIDIA Riva TTS on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        """Deinitialize the extension"""
        await super().on_deinit(ten_env)
        ten_env.log_debug("NVIDIA Riva TTS on_deinit")

    def vendor(self) -> str:
        """Return vendor name"""
        return "nvidia_riva"

    def synthesize_audio_sample_rate(self) -> int:
        """Return audio sample rate"""
        return (
            self.config.params.get("sample_rate", 16000)
            if self.config
            else 16000
        )

    def synthesize_audio_channels(self) -> int:
        """Return number of audio channels"""
        return 1

    def synthesize_audio_sample_width(self) -> int:
        """Return sample width in bytes"""
        return 2  # 16-bit PCM

    async def request_tts(self, t: TTSTextInput) -> None:
        """Handle TTS request"""
        try:
            self.ten_env.log_info(
                f"TTS request: text_length={len(t.text)}, "
                f"text_input_end={t.text_input_end}, request_id={t.request_id}"
            )

            # Skip if request already completed
            if t.request_id == self.flush_request_id:
                self.ten_env.log_debug(
                    f"Request {t.request_id} was flushed, ignoring"
                )
                return

            if t.request_id == self.last_end_request_id:
                self.ten_env.log_debug(
                    f"Request {t.request_id} was ended, ignoring"
                )
                return

            # Handle new request
            is_new_request = self.current_request_id != t.request_id
            if is_new_request:
                self.ten_env.log_debug(f"New TTS request: {t.request_id}")
                self.current_request_id = t.request_id
                self.request_total_audio_duration = 0
                self.request_start_ts = time.time()

            if self.client is None:
                raise ValueError("TTS client not initialized")

            # Synthesize audio
            received_first_chunk = False
            async for chunk in self.client.synthesize(t.text, t.request_id):
                # Calculate audio duration
                duration = self._calculate_audio_duration(len(chunk))

                self.ten_env.log_debug(
                    f"receive_audio: duration={duration}ms, request_id={self.current_request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )

                if not received_first_chunk:
                    received_first_chunk = True
                    # Send audio start
                    if t.request_id not in self.audio_start_sent:
                        await self.send_tts_audio_start(t.request_id)
                        self.audio_start_sent.add(t.request_id)
                        if is_new_request:
                            # Send TTFB metrics
                            self.first_chunk_ts = time.time()
                            elapsed_time = int(
                                (self.first_chunk_ts - self.request_start_ts)
                                * 1000
                            )
                            await self.send_tts_ttfb_metrics(
                                request_id=t.request_id,
                                ttfb_ms=elapsed_time,
                                extra_metadata={
                                    "voice_name": self.config.params[
                                        "voice_name"
                                    ],
                                    "language_code": self.config.params[
                                        "language_code"
                                    ],
                                },
                            )

                if t.request_id == self.flush_request_id:
                    break

                self.request_total_audio_duration += duration
                await self.send_tts_audio_data(chunk)

            # Handle completion
            if t.text_input_end or t.request_id == self.flush_request_id:
                reason = TTSAudioEndReason.REQUEST_END
                if t.request_id == self.flush_request_id:
                    reason = TTSAudioEndReason.INTERRUPTED

                if self.first_chunk_ts > 0:
                    await self._handle_completed_request(reason)

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}"
            )
            await self.send_tts_error(
                request_id=t.request_id,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info={"vendor": "nvidia_riva"},
                ),
            )

            # Check if we've received text_input_end
            has_received_text_input_end = False
            if t.request_id and t.request_id in self.request_states:
                if self.request_states[t.request_id] == RequestState.FINALIZING:
                    has_received_text_input_end = True

            if has_received_text_input_end:
                await self._handle_completed_request(TTSAudioEndReason.ERROR)

    async def cancel_tts(self) -> None:
        """Cancel current TTS request"""
        self.ten_env.log_info(
            f"cancel_tts current_request_id: {self.current_request_id}"
        )
        if self.current_request_id is not None:
            self.flush_request_id = self.current_request_id

        if self.client:
            await self.client.cancel()

        if self.current_request_id and self.first_chunk_ts > 0:
            await self._handle_completed_request(TTSAudioEndReason.INTERRUPTED)

    async def _handle_completed_request(
        self, reason: TTSAudioEndReason
    ) -> None:
        """Handle completed TTS request"""
        if not self.current_request_id:
            return

        self.last_end_request_id = self.current_request_id

        # Calculate metrics
        request_event_interval = 0
        if self.first_chunk_ts > 0:
            request_event_interval = int(
                (time.time() - self.first_chunk_ts) * 1000
            )

        # Send audio end
        await self.send_tts_audio_end(
            request_id=self.current_request_id,
            request_event_interval_ms=request_event_interval,
            request_total_audio_duration_ms=self.request_total_audio_duration,
            reason=reason,
        )

        self.ten_env.log_debug(
            f"Sent tts_audio_end: reason={reason.name}, request_id={self.current_request_id}"
        )

        # Finish request
        await self.finish_request(
            request_id=self.current_request_id, reason=reason
        )

        # Reset state
        self.first_chunk_ts = 0
        self.audio_start_sent.discard(self.current_request_id)

    def _calculate_audio_duration(self, bytes_length: int) -> int:
        """Calculate audio duration in milliseconds"""
        bytes_per_second = (
            self.synthesize_audio_sample_rate()
            * self.synthesize_audio_channels()
            * self.synthesize_audio_sample_width()
        )
        return int((bytes_length / bytes_per_second) * 1000)
