#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import os
import traceback
from datetime import datetime

from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleType,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
from ten_runtime import AsyncTenEnv

from .config import Qwen3TTSConfig
from .qwen3_tts import Qwen3TTSClient


class Qwen3TTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: Qwen3TTSConfig | None = None
        self.client: Qwen3TTSClient | None = None
        self.current_request_id: str | None = None
        self.request_start_ts: datetime | None = None
        self.request_total_audio_duration: int = 0
        self.recorder_map: dict[str, PCMWriter] = {}
        self.completed_request_ids: set[str] = set()
        self._cancel_event: asyncio.Event = asyncio.Event()

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            # Load configuration
            config_json, _ = await self.ten_env.get_property_to_json("")
            self.config = Qwen3TTSConfig.model_validate_json(config_json)
            self.config.update_params()

            ten_env.log_info(
                f"config: {self.config.to_str()}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            # Create client (model will be loaded lazily on first request)
            self.client = Qwen3TTSClient(self.config, ten_env)

        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info={},
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")

        # Close client
        if self.client:
            await self.client.close()
            self.client = None

        # Flush all PCM writers
        for request_id, recorder in self.recorder_map.items():
            try:
                await recorder.flush()
                ten_env.log_debug(
                    f"Flushed PCMWriter for request_id: {request_id}"
                )
            except Exception as e:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {e}"
                )

        await super().on_stop(ten_env)

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    def vendor(self) -> str:
        return "qwen3"

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else 24000

    def synthesize_audio_channels(self) -> int:
        return 1

    def synthesize_audio_sample_width(self) -> int:
        return 2  # 16-bit PCM

    async def request_tts(self, t: TTSTextInput) -> None:
        """Handle TTS request"""
        try:
            self.ten_env.log_info(
                f"Requesting TTS for text: {t.text}, "
                f"text_input_end: {t.text_input_end}, "
                f"request ID: {t.request_id}"
            )

            # Check if request has already been completed
            if t.request_id in self.completed_request_ids:
                self.ten_env.log_warn(
                    f"Request ID {t.request_id} has already been completed"
                )
                self.ten_env.log_debug(
                    f"skip_tts_text_input: {t.text} of request id: {t.request_id}",
                    category=LOG_CATEGORY_KEY_POINT,
                )
                return

            # Track request completion
            if t.text_input_end:
                self.completed_request_ids.add(t.request_id)

            # Handle new request ID
            if self.current_request_id != t.request_id:
                self.ten_env.log_debug(
                    f"New TTS request with ID: {t.request_id}"
                )
                self.current_request_id = t.request_id
                self.request_total_audio_duration = 0
                self._cancel_event.clear()

                # Set up audio dumping for new request
                if self.config.dump:
                    self._cleanup_old_recorders(t.request_id)
                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"qwen3_tts_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )
                        self.ten_env.log_info(
                            f"Created PCMWriter for request_id: {t.request_id}, "
                            f"file: {dump_file_path}"
                        )

            if self.client is None:
                self.ten_env.log_error(
                    "Client is not initialized, cannot process TTS request"
                )
                await self.send_tts_error(
                    request_id=t.request_id,
                    error=ModuleError(
                        message="TTS client is not initialized",
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.FATAL_ERROR,
                        vendor_info={"vendor": "qwen3"},
                    ),
                )
                return

            # Skip empty text
            if not t.text or not t.text.strip():
                if t.text_input_end:
                    await self._complete_request(TTSAudioEndReason.REQUEST_END)
                return

            # Record start time for TTFB
            if self.request_start_ts is None:
                self.request_start_ts = datetime.now()

            # Synthesize audio
            first_chunk = True
            async for audio_chunk in self.client.synthesize(t.text):
                # Check for cancellation
                if self._cancel_event.is_set():
                    self.ten_env.log_debug("TTS cancelled during synthesis")
                    break

                if first_chunk:
                    first_chunk = False
                    # Send audio start
                    await self.send_tts_audio_start(request_id=t.request_id)

                    # Calculate and send TTFB metrics
                    if self.request_start_ts:
                        ttfb_ms = int(
                            (
                                datetime.now() - self.request_start_ts
                            ).total_seconds()
                            * 1000
                        )
                        await self.send_tts_ttfb_metrics(
                            request_id=t.request_id,
                            ttfb_ms=ttfb_ms,
                            extra_metadata={
                                "model": self.config.model,
                                "mode": self.config.mode,
                                "speaker": self.config.speaker,
                            },
                        )
                        self.request_start_ts = None

                # Calculate audio duration
                cur_duration = self.calculate_audio_duration(
                    len(audio_chunk),
                    self.synthesize_audio_sample_rate(),
                    self.synthesize_audio_channels(),
                    self.synthesize_audio_sample_width(),
                )
                self.request_total_audio_duration += cur_duration

                self.ten_env.log_debug(
                    f"receive_audio: duration: {cur_duration}ms, "
                    f"total: {self.request_total_audio_duration}ms, "
                    f"request_id: {t.request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )

                # Dump audio if enabled
                if self.config.dump and t.request_id in self.recorder_map:
                    await self.recorder_map[t.request_id].write(audio_chunk)

                # Send audio data
                await self.send_tts_audio_data(audio_chunk)

            # Complete request if this was the last text input
            if t.text_input_end and not self._cancel_event.is_set():
                await self._complete_request(TTSAudioEndReason.REQUEST_END)

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}. text: {t.text}"
            )
            await self.send_tts_error(
                request_id=t.request_id,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info={"vendor": "qwen3"},
                ),
            )

            if t.text_input_end:
                await self._complete_request(TTSAudioEndReason.ERROR)

    async def cancel_tts(self) -> None:
        """Cancel ongoing TTS synthesis"""
        self.ten_env.log_info("Cancelling TTS request")
        self._cancel_event.set()
        await self._complete_request(TTSAudioEndReason.INTERRUPTED)

    async def _complete_request(self, reason: TTSAudioEndReason) -> None:
        """Complete the current request"""
        if self.current_request_id is None:
            return

        # Flush PCM writer
        if self.config.dump and self.current_request_id in self.recorder_map:
            try:
                await self.recorder_map[self.current_request_id].flush()
                self.ten_env.log_debug(
                    f"Flushed PCMWriter for request_id: {self.current_request_id}"
                )
            except Exception as e:
                self.ten_env.log_error(f"Error flushing PCMWriter: {e}")

        # Send audio end
        await self.send_tts_audio_end(
            request_id=self.current_request_id,
            request_event_interval_ms=0,
            request_total_audio_duration_ms=self.request_total_audio_duration,
            reason=reason,
        )

        self.ten_env.log_debug(
            f"Sent tts_audio_end with {reason.name} reason "
            f"for request_id: {self.current_request_id}"
        )

        # Finish request
        await self.finish_request(
            request_id=self.current_request_id,
            reason=reason,
        )

        # Reset state
        self.current_request_id = None
        self.request_start_ts = None
        self.request_total_audio_duration = 0

    def _cleanup_old_recorders(self, current_request_id: str) -> None:
        """Clean up old PCM recorders"""
        old_request_ids = [
            rid for rid in self.recorder_map.keys() if rid != current_request_id
        ]
        for old_rid in old_request_ids:
            try:
                del self.recorder_map[old_rid]
                self.ten_env.log_debug(
                    f"Cleaned up old PCMWriter for request_id: {old_rid}"
                )
            except Exception as e:
                self.ten_env.log_error(
                    f"Error cleaning up PCMWriter for request_id {old_rid}: {e}"
                )

    def calculate_audio_duration(
        self,
        bytes_length: int,
        sample_rate: int,
        channels: int = 1,
        sample_width: int = 2,
    ) -> int:
        """Calculate audio duration in milliseconds"""
        bytes_per_second = sample_rate * channels * sample_width
        duration_seconds = bytes_length / bytes_per_second
        return int(duration_seconds * 1000)
