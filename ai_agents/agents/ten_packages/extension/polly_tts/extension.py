#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import time
import traceback
from pathlib import Path
from typing_extensions import override

from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
from ten_runtime import (
    AsyncTenEnv,
    Data,
)
from botocore.exceptions import NoCredentialsError
from .config import PollyTTSConfig
from .polly_tts import PollyTTS


class PollyTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: PollyTTSConfig | None = None
        self.client: PollyTTS | None = None

        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.audio_dumper: Dumper | dict[str, Dumper] | None = None
        self.request_start_ts: float = 0
        self.first_chunk_ts: float = 0
        self.request_total_audio_duration: int = 0
        self.flush_request_ids: set[str] = set()
        self.last_end_request_ids: set[str] = set()

    @override
    def vendor(self) -> str:
        return "amazon"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        config_json, _ = await ten_env.get_property_to_json()
        try:
            self.config = PollyTTSConfig.model_validate_json(config_json)
            ten_env.log_info(
                f"config: {self.config.model_dump_json()}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            if self.config.dump:
                self.audio_dumper = {}

            self.client = PollyTTS(
                self.config.params,
                timeout=self.config.timeout,
                max_retries=self.config.max_retries,
                retry_delay=self.config.retry_delay,
                chunk_interval_ms=self.config.chunk_interval_ms,
            )
            # test and preconnect to aws polly
            # this can effectively reduce latency.
            async for _chunk in self.client.async_synthesize_speech("P"):
                ...
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

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        ten_env.log_debug(
            "vendor_status: on_stop", category=LOG_CATEGORY_VENDOR
        )
        if self.client:
            self.client.close()
        if isinstance(self.audio_dumper, Dumper):
            dumper: Dumper = self.audio_dumper
            await dumper.stop()  # pylint: disable=no-member
        elif isinstance(self.audio_dumper, dict):
            for dumper in self.audio_dumper.values():
                await dumper.stop()

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        name = data.get_name()
        if name == "tts_flush":
            # get flush_id and record to flush_request_ids
            flush_id, _ = data.get_property_string("flush_id")
            if flush_id:
                self.flush_request_ids.add(flush_id)

            # if current request is flushed, send audio_end
            if (
                self.current_request_id
                and self.first_chunk_ts > 0
                and self.current_request_id in self.flush_request_ids
            ):
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

    @override
    async def request_tts(self, t: TTSTextInput) -> None:
        if self.client is None:
            return
        # check if request_id is in flush_request_ids
        if t.request_id in self.flush_request_ids:
            error_msg = (
                f"Request ID {t.request_id} was flushed, ignoring TTS request"
            )
            self.ten_env.log_debug(error_msg)
            await self.send_tts_error(
                t.request_id,
                ModuleError(
                    message=error_msg,
                    module="tts",
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                ),
            )
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

        text = t.text
        request_id = t.request_id
        turn_id = t.metadata.get("turn_id", -1)
        text_input_end = t.text_input_end

        is_new_request = self.current_request_id != request_id
        self.current_request_id = request_id
        if is_new_request:
            self.request_total_audio_duration = 0
            self.request_start_ts = time.time()

        try:
            received_first_chunk = False
            if len(text.strip()) == 0:
                raise ValueError("text is empty")
            async for chunk in self.client.async_synthesize_speech(text):
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
                    _dumper = self.audio_dumper.get(t.request_id)
                    if _dumper is not None:
                        await _dumper.push_bytes(chunk)
                    else:
                        dump_file_path = Path(self.config.dump_path)
                        dump_file_path = (
                            dump_file_path / f"aws_polly_in_{t.request_id}.pcm"
                        )
                        dump_file_path.parent.mkdir(parents=True, exist_ok=True)
                        _dumper = Dumper(str(dump_file_path))
                        await _dumper.start()
                        await _dumper.push_bytes(chunk)
                        self.audio_dumper[t.request_id] = _dumper
        except ValueError:
            pass
        except NoCredentialsError as e:
            self.ten_env.log_error(
                f"vendor_status: invalid credentials {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            await self.send_tts_error(
                self.current_request_id,
                ModuleError(
                    message=str(e),
                    module="tts",
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    vendor_info=ModuleErrorVendorInfo(
                        vendor="aws_polly",
                        code="NoCredentialsError",
                        message=str(e),
                    ),
                ),
            )
        except Exception as e:
            self.ten_env.log_error(
                f"vendor_status: Error in request_tts: {traceback.format_exc()}. text: {t.text}",
                category=LOG_CATEGORY_VENDOR,
            )
            await self.send_tts_error(
                self.current_request_id,
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

    def synthesize_audio_sample_rate(self) -> int:
        assert self.config is not None
        return int(self.config.params.sample_rate)

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
