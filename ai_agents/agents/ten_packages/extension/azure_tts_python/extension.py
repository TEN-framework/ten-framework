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
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput, TTSTextResult
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_runtime import (
    AsyncTenEnv,
    Data,
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
        self.request_start_ts: float | None = None
        self.current_turn_id: int = -1
        self.audio_dumper: Dumper | dict[str, Dumper] | None = None
        self.flush_request_ids: set[str] = set()
        self.last_end_request_ids: set[str] = set()
        self.request_total_audio_duration: int = 0

    @override
    def vendor(self) -> str:
        return "azure"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        config_json, _ = await ten_env.get_property_to_json()
        try:
            ten_env.log_info(f"KEYPOINT tts config_json: {config_json}")
            self.config = AzureTTSConfig.model_validate_json(config_json)
            ten_env.log_info(
                f"KEYPOINT tts vendor_config: {self.config.model_dump_json()}"
            )

            if self.config.dump:
                self.audio_dumper = {}

            self.client = AzureTTS(
                self.config.params, chunk_size=self.config.chunk_size
            )
            await self.client.start_connection(
                pre_connect=self.config.pre_connect
            )
            ten_env.log_info("KEYPOINT tts connect successfully")
        except Exception as e:
            ten_env.log_error(f"KEYPOINT tts on_init error: {e}")
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

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")
        if self.client:
            await self.client.stop_connection()
        if isinstance(self.audio_dumper, Dumper):
            dumper: Dumper = self.audio_dumper
            await dumper.stop()  # pylint: disable=no-member
        elif isinstance(self.audio_dumper, dict):
            for dumper in self.audio_dumper.values():
                await dumper.stop()

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        name = data.get_name()
        if name == "tts_flush":
            ten_env.log_info(f"KEYPOINT tts Received tts_flush data: {name}")

            # get flush_id and record to flush_request_ids
            flush_id, _ = data.get_property_string("flush_id")
            if flush_id:
                self.flush_request_ids.add(flush_id)
                ten_env.log_info(
                    f"KEYPOINT tts Added request_id {flush_id} to flush_request_ids set"
                )
            if (
                self.request_start_ts is not None
                and self.current_request_id is not None
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
        await super().on_data(ten_env, data)

    async def _async_synthesize(self, text_input: TTSTextInput):
        assert self.client is not None
        text = text_input.text
        request_id = text_input.request_id
        turn_id = text_input.metadata.get("turn_id", -1)
        text_input_end = text_input.text_input_end

        first_chunk = False
        self.request_total_audio_duration = 0
        try:
            request_start_ts = time.time()
            self.request_start_ts = request_start_ts
            self.current_request_id = request_id
            self.ten_env.log_info(
                f"KEYPOINT ttsSynthesizing audio for request ID: {request_id}, text: {text}"
            )
            async for chunk in await self.client.synthesize_with_retry(
                text, max_retries=5, retry_delay=1.0
            ):
                if not first_chunk:
                    first_chunk = True
                    await self.send_tts_audio_start(request_id, turn_id)
                    elapsed_time = int((time.time() - request_start_ts) * 1000)
                    await self.send_tts_ttfb_metrics(
                        request_id, elapsed_time, turn_id
                    )
                    self.ten_env.log_info(
                        f"KEYPOINT tts Sent TTFB metrics for request ID: {request_id}, elapsed time: {elapsed_time}ms"
                    )

                if request_id in self.flush_request_ids:
                    # flush request, break current synthesize task
                    break

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
                        request_id=request_id,
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

            if text_input_end:
                self.last_end_request_ids.add(request_id)
                reason = TTSAudioEndReason.REQUEST_END
                if request_id in self.flush_request_ids:
                    reason = TTSAudioEndReason.INTERRUPTED
                request_event_interval = int(
                    (time.time() - request_start_ts) * 1000
                )
                await self.send_tts_audio_end(
                    request_id,
                    request_event_interval,
                    self.request_total_audio_duration,
                    turn_id,
                    reason,
                )
                self.ten_env.log_info(
                    f"KEYPOINT tts Sent TTS audio end for request ID: {request_id} reason: {reason}"
                )
        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}. text: {text}"
            )
            await self.send_tts_error(
                request_id,
                ModuleError(
                    message=str(e),
                    module="tts",
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                ),
            )

    @override
    async def request_tts(self, t: TTSTextInput) -> None:
        if self.client is None or not self.client.is_connected:
            self.ten_env.log_error(
                "KEYPOINT tts client is not initialized, ignoring TTS request"
            )
            return
        self.ten_env.log_info(
            f"KEYPOINT Requesting tts for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}"
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
                f"KEYPOINT tts end request ID: {t.request_id} is already ended, ignoring TTS request"
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

        # create a new task to synthesize the audio
        # asyncio.create_task(self._async_synthesize(t))
        await self._async_synthesize(t)

    def synthesize_audio_sample_rate(self) -> int:
        assert self.config is not None
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
