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
from ten_ai_base.struct import TTSTextInput, TTSTextResult
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
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
        self.request_start_ts: float | None = None
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
                f"KEYPOINT vendor_config: {self.config.model_dump_json()}"
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
        except Exception as e:
            ten_env.log_error(f"invalid property: {e}")
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
        ten_env.log_debug("on_stop")
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
            ten_env.log_info(f"Received tts_flush data: {name}")

            # get flush_id and record to flush_request_ids
            flush_id, _ = data.get_property_string("flush_id")
            if flush_id:
                self.flush_request_ids.add(flush_id)
                ten_env.log_info(
                    f"Added request_id {flush_id} to flush_request_ids set"
                )

            # if current request is flushed, send audio_end
            if (
                self.current_request_id
                and self.request_start_ts is not None
                and self.current_request_id in self.flush_request_ids
            ):
                request_event_interval = int(
                    (time.time() - self.request_start_ts) * 1000
                )
                await self.send_tts_audio_end(
                    self.current_request_id,
                    request_event_interval,
                    self.request_total_audio_duration,
                    self.current_turn_id,
                    TTSAudioEndReason.INTERRUPTED,
                )
                ten_env.log_info(
                    f"Sent tts_audio_end with INTERRUPTED reason for request_id: {self.current_request_id}"
                )
        await super().on_data(ten_env, data)

    @override
    async def request_tts(self, t: TTSTextInput) -> None:
        if self.client is None:
            return
        self.ten_env.log_info(
            f"KEYPOINT Requesting TTS for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}"
        )
        # check if request_id is in flush_request_ids
        if t.request_id in self.flush_request_ids:
            error_msg = (
                f"Request ID {t.request_id} was flushed, ignoring TTS request"
            )
            self.ten_env.log_warn(error_msg)
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
            self.ten_env.log_info(
                f"KEYPOINT end request ID: {t.request_id} is already ended, ignoring TTS request"
            )
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
        if t.request_id != self.current_request_id:
            self.ten_env.log_info(
                f"KEYPOINT New TTS request with ID: {t.request_id}"
            )
            if (
                self.current_request_id is not None
                and self.request_start_ts is not None
            ):
                request_event_interval = int(
                    (time.time() - self.request_start_ts) * 1000
                )
                reason = TTSAudioEndReason.REQUEST_END
                if self.current_request_id in self.flush_request_ids:
                    reason = TTSAudioEndReason.INTERRUPTED
                    self.flush_request_ids.remove(self.current_request_id)
                await self.send_tts_audio_end(
                    self.current_request_id,
                    request_event_interval,
                    self.request_total_audio_duration,
                    self.current_turn_id,
                    reason,
                )

            self.current_request_id = t.request_id
            if t.metadata is not None:
                self.current_turn_id = t.metadata.get("turn_id", -1)
            self.request_start_ts = time.time()
            self.request_total_audio_duration = 0

        first_chunk = False
        try:
            async for chunk in self.client.async_synthesize_speech(text):
                if not first_chunk:
                    first_chunk = True
                    if self.request_start_ts is not None:
                        await self.send_tts_audio_start(
                            t.request_id, self.current_turn_id
                        )
                        elapsed_time = int(
                            (time.time() - self.request_start_ts) * 1000
                        )
                        await self.send_tts_ttfb_metrics(
                            t.request_id, elapsed_time, self.current_turn_id
                        )
                        self.ten_env.log_info(
                            f"KEYPOINT Sent TTFB metrics for request ID: {t.request_id}, elapsed time: {elapsed_time}ms"
                        )

                if self.current_request_id in self.flush_request_ids:
                    continue

                # calculate audio duration
                self.request_total_audio_duration += (
                    self._calculate_audio_duration(
                        len(chunk),
                        self.synthesize_audio_sample_rate(),
                        self.synthesize_audio_channels(),
                        self.synthesize_audio_sample_width(),
                    )
                )

                # send audio data to output
                await self.send_tts_audio_data(chunk)
                await self.send_tts_text_result(
                    TTSTextResult(
                        request_id=self.current_request_id or "",
                        text="",
                        start_ms=0,
                        duration_ms=self.request_total_audio_duration,
                        words=[],
                        metadata={},
                    )
                )

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

            if t.text_input_end:
                self.last_end_request_ids.add(t.request_id)
                if (
                    self.current_request_id is not None
                    and self.request_start_ts is not None
                ):
                    reason = TTSAudioEndReason.REQUEST_END
                    if self.current_request_id in self.flush_request_ids:
                        reason = TTSAudioEndReason.INTERRUPTED
                    request_event_interval = int(
                        (time.time() - self.request_start_ts) * 1000
                    )
                    await self.send_tts_audio_end(
                        self.current_request_id,
                        request_event_interval,
                        self.request_total_audio_duration,
                        self.current_turn_id,
                        reason,
                    )
                    self.ten_env.log_info(
                        f"KEYPOINT Sent TTS audio end for request ID: {self.current_request_id} reason: {reason}"
                    )
                    self.current_request_id = None
                    self.request_start_ts = None
                    self.request_total_audio_duration = 0
                    self.current_turn_id = -1
        except NoCredentialsError as e:
            self.ten_env.log_error(f"invalid credentials: {e}")
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
                f"Error in request_tts: {traceback.format_exc()}. text: {t.text}"
            )
            await self.send_tts_error(
                self.current_request_id,
                ModuleError(
                    message=str(e),
                    module="tts",
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                ),
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
