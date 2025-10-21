#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import time
import traceback
from pathlib import Path
from typing_extensions import override

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
)

import azure.cognitiveservices.speech as speechsdk
from .config import AzureTTSConfig
from .azure_tts import AzureTTS


class AzureTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: AzureTTSConfig | None = None
        self.client: AzureTTS | None = None

        self.current_request_id: str | None = None
        self.request_start_ts: float = 0
        self.first_chunk_ts: float = 0
        self.audio_dumper: Dumper | dict[str, Dumper] | None = None
        self.flush_request_id: str | None = None
        self.last_end_request_id: str | None = None
        self.request_total_audio_duration: int = 0
        self.request_done: asyncio.Event = asyncio.Event()
        self.request_task: asyncio.Task | None = None
        self.request_done.set()

    @override
    def vendor(self) -> str:
        return "azure"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        config_json, _ = await ten_env.get_property_to_json()
        try:
            self.config = AzureTTSConfig.model_validate_json(config_json)
            ten_env.log_info(
                f"config: {self.config.model_dump_json()}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            if self.config.dump:
                self.audio_dumper = {}

            self.client = AzureTTS(
                self.config.params, chunk_size=self.config.chunk_size
            )
            asyncio.create_task(
                self.client.start_connection(
                    pre_connect=self.config.pre_connect
                )
            )
        except Exception as e:
            ten_env.log_error(
                f"vendor_status: tts on_init error: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            self.config = None
            self.client = None
            await self.send_tts_error(
                self.current_request_id,
                ModuleError(
                    module="tts",
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    async def _wait_until_connected(self, timeout: float = 30.0) -> None:
        start_time = time.time()
        while not self.client.is_connected:
            await asyncio.sleep(0.1)
            if time.time() - start_time > timeout:
                raise TimeoutError(
                    f"wait for connected to the speech service timeout: {timeout}s"
                )

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        ten_env.log_debug(
            "vendor_status: on_stop", category=LOG_CATEGORY_VENDOR
        )
        if self.client:
            await self.client.stop_connection()
        if isinstance(self.audio_dumper, Dumper):
            dumper: Dumper = self.audio_dumper
            await dumper.stop()  # pylint: disable=no-member
        elif isinstance(self.audio_dumper, dict):
            for dumper in self.audio_dumper.values():
                await dumper.stop()

    @override
    async def cancel_tts(self) -> None:
        self.ten_env.log_info(
            f"cancel_tts current_request_id: {self.current_request_id}"
        )
        if self.current_request_id is not None:
            self.flush_request_id = self.current_request_id
        if self.request_task is not None:
            self.request_task.cancel()
        await self.request_done.wait()

    async def _async_synthesize(self, text_input: TTSTextInput):
        assert self.client is not None
        text = text_input.text
        request_id = text_input.request_id
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
            async for chunk in await self.client.synthesize_with_retry(
                text, max_retries=5, retry_delay=1.0
            ):
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
                    await self.send_tts_audio_start(request_id)
                    if is_new_request:
                        # send ttfb metrics for new request
                        self.first_chunk_ts = time.time()
                        elapsed_time = int(
                            (self.first_chunk_ts - self.request_start_ts) * 1000
                        )
                        await self.send_tts_ttfb_metrics(
                            request_id=request_id,
                            ttfb_ms=elapsed_time,
                            extra_metadata={
                                "voice_name": self.client.speech_config.speech_synthesis_voice_name,
                            },
                        )

                if request_id == self.flush_request_id:
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
                            dump_file_path / f"azure_tts_in_{request_id}.pcm"
                        )
                        dump_file_path.parent.mkdir(parents=True, exist_ok=True)
                        _dumper = Dumper(str(dump_file_path))
                        await _dumper.start()
                        await _dumper.push_bytes(chunk)
                        self.audio_dumper[request_id] = _dumper
        except asyncio.CancelledError:
            # interrupt by cancel_tts
            pass
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

        if (
            text_input_end or request_id == self.flush_request_id
        ) and self.first_chunk_ts > 0:
            self.last_end_request_id = request_id
            reason = TTSAudioEndReason.REQUEST_END
            if request_id == self.flush_request_id:
                reason = TTSAudioEndReason.INTERRUPTED
            request_event_interval = int(
                (time.time() - self.first_chunk_ts) * 1000
            )
            await self.send_tts_audio_end(
                request_id=request_id,
                request_event_interval_ms=request_event_interval,
                request_total_audio_duration_ms=self.request_total_audio_duration,
                reason=reason,
            )

    @override
    async def request_tts(self, t: TTSTextInput) -> None:
        try:
            await self._wait_until_connected()
        except Exception:
            self.ten_env.log_error(
                "vendor_status: tts client connection failed, ignoring TTS request",
                category=LOG_CATEGORY_VENDOR,
            )
            return
        # check if request_id is flushed
        if t.request_id == self.flush_request_id:
            self.ten_env.log_debug(
                f"Request ID {t.request_id} was flushed, ignoring TTS request"
            )
            return

        if t.request_id == self.last_end_request_id:
            self.ten_env.log_debug(
                f"Request ID {t.request_id} was ended, ignoring TTS request"
            )
            return

        try:
            self.request_done.clear()
            self.request_task = asyncio.create_task(self._async_synthesize(t))
            await self.request_task
        finally:
            self.request_done.set()
            self.request_task = None

    def synthesize_audio_sample_rate(self) -> int:
        if self.config is None:
            return 16000
        if (
            self.config.params.output_format
            == speechsdk.SpeechSynthesisOutputFormat.Raw8Khz16BitMonoPcm
        ):
            return 8000
        elif (
            self.config.params.output_format
            == speechsdk.SpeechSynthesisOutputFormat.Raw16Khz16BitMonoPcm
        ):
            return 16000
        elif (
            self.config.params.output_format
            == speechsdk.SpeechSynthesisOutputFormat.Raw24Khz16BitMonoPcm
        ):
            return 24000
        elif (
            self.config.params.output_format
            == speechsdk.SpeechSynthesisOutputFormat.Raw48Khz16BitMonoPcm
        ):
            return 48000
        else:
            raise ValueError(
                f"Unsupported output format: {self.config.params.output_format}"
            )

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
