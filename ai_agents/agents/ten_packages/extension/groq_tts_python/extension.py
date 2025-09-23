#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import time
import asyncio
import traceback
from pathlib import Path
from typing_extensions import override
from contextlib import suppress

from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
from ten_runtime import (
    AsyncTenEnv,
    Data,
)

from .config import GroqTTSConfig
from .groq_tts import GroqTTS


class GroqTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: GroqTTSConfig | None = None
        self.client: GroqTTS | None = None

        self.current_request_id: str | None = None
        self.request_start_ts: float = 0
        self.first_chunk_ts: float = 0
        self.current_turn_id: int = -1
        self.audio_dumper: Dumper | dict[str, Dumper] | None = None
        self.flush_request_ids: set[str] = set()
        self.last_end_request_ids: set[str] = set()
        self.request_total_audio_duration: int = 0
        self.heartbeat_task: asyncio.Task | None = None

    @override
    def vendor(self) -> str:
        return "groq"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        config_json, _ = await ten_env.get_property_to_json()
        try:
            self.config = GroqTTSConfig.model_validate_json(config_json)
            ten_env.log_info(
                f"config: {self.config.model_dump_json()}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            if self.config.dump:
                self.audio_dumper = {}

            self.client = GroqTTS(self.config.params)

            # pre connect and keep alive
            async for _chunk in self.client.synthesize_with_retry("G"):
                pass
            self.heartbeat_task = asyncio.create_task(self._heartbeat_task())

            self.ten_env.log_debug(
                "vendor_status: tts connect successfully",
                category=LOG_CATEGORY_VENDOR,
            )
        except Exception as e:
            ten_env.log_error(
                f"vendor_status: tts on_init error: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            self.client = None
            await self.send_tts_error(
                self.current_request_id,
                ModuleError(
                    module="tts",
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    async def _heartbeat_task(self) -> None:
        """
        Keep the connection alive by sending a HEAD request to the API.
        this effectively keeps the low latency of the connection.
        """
        while True:
            await asyncio.sleep(1)
            with suppress(Exception):
                if self.client is not None and self.client.client is not None:
                    # pylint: disable=protected-access
                    await self.client.client._client.request(
                        "HEAD", "https://api.groq.com"
                    )

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        ten_env.log_debug(
            "vendor_status: on_stop", category=LOG_CATEGORY_VENDOR
        )
        self.client = None
        if isinstance(self.audio_dumper, dict):
            for dumper in self.audio_dumper.values():
                await dumper.stop()
        if self.heartbeat_task is not None:
            self.heartbeat_task.cancel()

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        name = data.get_name()
        if name == "tts_flush":
            # get flush_id and record to flush_request_ids
            flush_id, _ = data.get_property_string("flush_id")
            if flush_id:
                self.flush_request_ids.add(flush_id)

            if self.first_chunk_ts > 0 and self.current_request_id is not None:
                request_event_interval = int(
                    (time.time() - self.first_chunk_ts) * 1000
                )
                await self.send_tts_audio_end(
                    self.current_request_id,
                    request_event_interval,
                    self.request_total_audio_duration,
                    self.current_turn_id,
                    TTSAudioEndReason.INTERRUPTED,
                )
        await super().on_data(ten_env, data)

    async def _async_synthesize(self, text_input: TTSTextInput):
        assert self.client is not None
        text = text_input.text
        request_id = text_input.request_id
        turn_id = text_input.metadata.get("turn_id", -1)
        text_input_end = text_input.text_input_end

        is_new_request = self.current_request_id != request_id
        self.current_request_id = request_id
        if is_new_request:
            self.request_total_audio_duration = 0
            self.request_start_ts = time.time()

        try:
            received_first_chunk = False
            if len(text.strip()) == 0:
                raise ValueError("text is empty")
            async for chunk in self.client.synthesize_with_retry(text):
                # calculate audio duration
                duration = self._calculate_audio_duration(
                    len(chunk),
                    self.synthesize_audio_sample_rate(),
                    self.synthesize_audio_channels(),
                    self.synthesize_audio_sample_width(),
                )
                self.ten_env.log_debug(
                    f"receive_audio: duration: {duration}ms of request_id: {self.current_request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )

                if not received_first_chunk:
                    received_first_chunk = True
                    await self.send_tts_audio_start(request_id, turn_id)
                    if is_new_request:
                        # send ttfb metrics for new request
                        self.first_chunk_ts = time.time()
                        elapsed_time = int(
                            (self.first_chunk_ts - self.request_start_ts) * 1000
                        )
                        await self.send_tts_ttfb_metrics(
                            request_id, elapsed_time, turn_id
                        )

                if request_id in self.flush_request_ids:
                    # flush request, break current synthesize task
                    break

                self.request_total_audio_duration += duration
                # send audio data to output
                self.ten_env.log_debug(
                    f"vendor_status: Sending audio data for request ID: {request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )
                await self.send_tts_audio_data(chunk)

                # dump audio data to file
                assert self.config is not None
                if self.config.dump:
                    assert isinstance(self.audio_dumper, dict)
                    _dumper = self.audio_dumper.get(request_id)
                    if _dumper is not None:
                        await _dumper.push_bytes(chunk)
                    else:
                        dump_file_path = Path(self.config.dump_path)
                        dump_file_path = (
                            dump_file_path / f"groq_tts_in_{request_id}.pcm"
                        )
                        dump_file_path.parent.mkdir(parents=True, exist_ok=True)
                        _dumper = Dumper(str(dump_file_path))
                        await _dumper.start()
                        await _dumper.push_bytes(chunk)
                        self.audio_dumper[request_id] = _dumper
        except ValueError:
            pass
        except Exception as e:
            self.ten_env.log_error(
                f"vendor_status: Error in request_tts: {traceback.format_exc()}. text: {text}",
                category=LOG_CATEGORY_VENDOR,
            )
            await self.send_tts_error(
                request_id,
                ModuleError(
                    message=str(e),
                    module="tts",
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                ),
            )

        if text_input_end and self.first_chunk_ts > 0:
            self.last_end_request_ids.add(request_id)
            reason = TTSAudioEndReason.REQUEST_END
            if request_id in self.flush_request_ids:
                reason = TTSAudioEndReason.INTERRUPTED
            request_event_interval = int(
                (time.time() - self.first_chunk_ts) * 1000
            )
            await self.send_tts_audio_end(
                request_id,
                request_event_interval,
                self.request_total_audio_duration,
                turn_id,
                reason,
            )

    @override
    async def request_tts(self, t: TTSTextInput) -> None:
        if self.client is None:
            self.ten_env.log_error(
                "tts client is not initialized, ignoring TTS request"
            )
            return
        self.ten_env.log_debug(
            f"vendor_status: Requesting tts for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}",
            category=LOG_CATEGORY_VENDOR,
        )
        # check if request_id is in flush_request_ids
        if t.request_id in self.flush_request_ids:
            error_msg = (
                f"Request ID {t.request_id} was flushed, ignoring TTS request"
            )
            self.ten_env.log_debug(error_msg)
            return

        if t.request_id in self.last_end_request_ids:
            await self.send_tts_error(
                t.request_id,
                ModuleError(
                    message=f"End request ID: {t.request_id} is already ended, ignoring TTS request",
                    module="tts",
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                ),
            )
            return

        await self._async_synthesize(t)

    def synthesize_audio_sample_rate(self) -> int:
        assert self.config is not None
        assert self.config.params.sample_rate is not None
        return self.config.params.sample_rate

    def _calculate_audio_duration(
        self,
        bytes_length: int,
        sample_rate: int,
        channels: int = 1,
        sample_width: int = 2,
    ) -> int:
        """
        Calculate audio duration in milliseconds.

        Parameters:
        - bytes_length: Length of the audio data in bytes
        - sample_rate: Sample rate in Hz (e.g., 16000)
        - channels: Number of audio channels (default: 1 for mono)
        - sample_width: Number of bytes per sample (default: 2 for 16-bit PCM)

        Returns:
        - Duration in milliseconds (rounded down to nearest int)
        """
        bytes_per_second = sample_rate * channels * sample_width
        duration_seconds = bytes_length / bytes_per_second
        return int(duration_seconds * 1000)
