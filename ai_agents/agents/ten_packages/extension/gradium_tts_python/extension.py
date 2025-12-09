#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from datetime import datetime
import os
import traceback

from ten_ai_base.const import LOG_CATEGORY_KEY_POINT
from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_runtime import AsyncTenEnv

from .config import GradiumTTSConfig
from .gradium_tts import (
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_FLUSH,
    EVENT_TTS_RESPONSE,
    GradiumTTSClient,
)


class GradiumTTSExtension(AsyncTTS2BaseExtension):
    """Gradium TTS extension using the websocket streaming API."""

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: GradiumTTSConfig | None = None
        self.client: GradiumTTSClient | None = None
        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.request_ts: datetime | None = None
        self.sent_ts: datetime | None = None
        self.current_request_finished: bool = False
        self.total_audio_bytes: int = 0
        self.first_chunk: bool = False
        self.recorder_map: dict[str, PCMWriter] = {}

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError(
                    "Configuration is empty. Required parameter 'params' is missing."
                )

            self.config = GradiumTTSConfig.model_validate_json(config_json_str)
            self.config.update_params()
            self.config.validate()

            ten_env.log_info(
                f"LOG_CATEGORY_KEY_POINT: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            self.client = GradiumTTSClient(self.config, ten_env)
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    message=f"Initialization failed: {e}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.client:
            try:
                await self.client.clean()
            except Exception as e:
                ten_env.log_warn(f"Error cleaning client: {e}")
            self.client = None

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
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    async def cancel_tts(self) -> None:
        self.current_request_finished = True
        if self.client:
            await self.client.cancel()

        if self.current_request_id and self.request_ts:
            request_event_interval = int(
                (datetime.now() - self.request_ts).total_seconds() * 1000
            )
            duration_ms = self._calculate_audio_duration_ms()
            await self.send_tts_audio_end(
                request_id=self.current_request_id,
                request_event_interval_ms=request_event_interval,
                request_total_audio_duration_ms=duration_ms,
                reason=TTSAudioEndReason.INTERRUPTED,
            )

    def vendor(self) -> str:
        return "gradium"

    def synthesize_audio_sample_rate(self) -> int:
        if self.config:
            return self.config.get_sample_rate()
        return 48000

    async def request_tts(self, t: TTSTextInput) -> None:
        try:
            if self.client is None:
                self.client = GradiumTTSClient(self.config, self.ten_env)  # type: ignore

            if t.request_id != self.current_request_id:
                self.first_chunk = True
                self.sent_ts = datetime.now()
                self.request_ts = None
                self.current_request_id = t.request_id
                self.current_request_finished = False
                self.total_audio_bytes = 0
                if t.metadata is not None:
                    self.session_id = t.metadata.get("session_id", "")
                    self.current_turn_id = t.metadata.get("turn_id", -1)

                if self.config and self.config.dump:
                    old_request_ids = [
                        rid
                        for rid in self.recorder_map.keys()
                        if rid != t.request_id
                    ]
                    for old_rid in old_request_ids:
                        try:
                            await self.recorder_map[old_rid].flush()
                            del self.recorder_map[old_rid]
                        except Exception as e:
                            self.ten_env.log_error(
                                f"Error cleaning up PCMWriter for request_id {old_rid}: {e}"
                            )

                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"gradium_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )
            elif self.current_request_finished:
                self.ten_env.log_error(
                    f"Received a message for a finished request_id '{t.request_id}' with text_input_end=False."
                )
                return

            if t.text_input_end:
                self.current_request_finished = True

            self.metrics_add_output_characters(len(t.text))

            async for audio_chunk, event_status in self.client.get(
                t.text, t.request_id, t.text_input_end
            ):
                if event_status == EVENT_TTS_RESPONSE:
                    if audio_chunk is None or len(audio_chunk) == 0:
                        continue

                    self.total_audio_bytes += len(audio_chunk)
                    self.metrics_add_recv_audio_chunks(audio_chunk)

                    if self.first_chunk:
                        self.request_ts = datetime.now()
                        if self.sent_ts and self.current_request_id:
                            await self.send_tts_audio_start(
                                request_id=self.current_request_id
                            )
                            ttfb = int(
                                (self.request_ts - self.sent_ts).total_seconds()
                                * 1000
                            )
                            await self.send_tts_ttfb_metrics(
                                request_id=self.current_request_id,
                                ttfb_ms=ttfb,
                                extra_metadata=self.client.get_extra_metadata(),
                            )
                        self.first_chunk = False

                    if (
                        self.config
                        and self.config.dump
                        and self.current_request_id
                        and self.current_request_id in self.recorder_map
                    ):
                        await self.recorder_map[self.current_request_id].write(
                            audio_chunk
                        )

                    await self.send_tts_audio_data(audio_chunk)

                elif event_status == EVENT_TTS_END:
                    if t.text_input_end:
                        await self._finalize_request(
                            request_id=self.current_request_id,
                            reason=TTSAudioEndReason.REQUEST_END,
                        )
                    break

                elif event_status == EVENT_TTS_FLUSH:
                    break

                elif event_status == EVENT_TTS_ERROR:
                    err_msg = (
                        audio_chunk.decode("utf-8")
                        if isinstance(audio_chunk, (bytes, bytearray))
                        else "Gradium TTS error"
                    )
                    await self._handle_error(err_msg, t.request_id)
                    break

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}"
            )
            await self.send_tts_error(
                request_id=self.current_request_id or t.request_id,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def _handle_error(self, message: str, request_id: str) -> None:
        await self.send_tts_error(
            request_id=request_id,
            error=ModuleError(
                message=message,
                module=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )
        if self.current_request_finished and self.current_request_id:
            await self._finalize_request(
                request_id=self.current_request_id,
                reason=TTSAudioEndReason.ERROR,
            )

    async def _finalize_request(
        self,
        request_id: str | None,
        reason: TTSAudioEndReason,
    ) -> None:
        if not request_id:
            return

        request_event_interval = 0
        if self.request_ts:
            request_event_interval = int(
                (datetime.now() - self.request_ts).total_seconds() * 1000
            )
        duration_ms = self._calculate_audio_duration_ms()

        await self.send_tts_audio_end(
            request_id=request_id,
            request_event_interval_ms=request_event_interval,
            request_total_audio_duration_ms=duration_ms,
            reason=reason,
        )

        if self.config and self.config.dump and request_id in self.recorder_map:
            try:
                await self.recorder_map[request_id].flush()
            except Exception as e:
                self.ten_env.log_error(
                    f"Failed flushing PCMWriter for request_id {request_id}: {e}"
                )

        await self.send_usage_metrics(request_id)
        self.sent_ts = None
        self.request_ts = None
        self.total_audio_bytes = 0

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0
        bytes_per_sample = self.synthesize_audio_sample_width()
        channels = self.synthesize_audio_channels()
        duration_sec = self.total_audio_bytes / (
            self.synthesize_audio_sample_rate() * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)
