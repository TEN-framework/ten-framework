#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
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
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
    GradiumTTSClient,
    GradiumTTSConnectionException,
)


class GradiumTTSExtension(AsyncTTS2BaseExtension):
    """Gradium TTS extension using the websocket streaming API.

    Each ``tts_text_input`` segment is forwarded to the vendor immediately as
    it arrives (no local batching), and ``text_input_end`` only finalizes the
    request. This matches Gradium's LLM-to-TTS streaming guidance and the
    behaviour of the other websocket TTS extensions, so the base class owns
    queuing/ordering and we do not override ``on_data``.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: GradiumTTSConfig | None = None
        self.client: GradiumTTSClient | None = None
        self.current_request_id: str | None = None
        self.sent_ts: datetime | None = None
        self.current_request_finished: bool = False
        self.total_audio_bytes: int = 0
        self._audio_start_sent: bool = False
        self._ttfb_sent: bool = False
        self.current_request_sample_rate: int | None = None
        self._session_started: bool = False
        self._reader_task: asyncio.Task | None = None
        self.recorder_map: dict[str, PCMWriter] = {}

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError(
                    "Configuration is empty. "
                    "Required parameter 'api_key' is missing."
                )

            self.config = GradiumTTSConfig.model_validate_json(config_json_str)
            self.config.update_params()
            self.config.validate()
            ten_env.log_info(
                f"config: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            self.client = GradiumTTSClient(self.config, ten_env)
            await self.client.start()
        except Exception as exc:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    message=f"Initialization failed: {exc}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await self._cancel_reader_task()

        if self.client:
            try:
                await self.client.clean()
            except Exception as exc:
                ten_env.log_warn(f"Error cleaning client: {exc}")
            self.client = None

        for request_id, recorder in list(self.recorder_map.items()):
            try:
                await recorder.flush()
                ten_env.log_debug(
                    f"Flushed PCMWriter for request_id: {request_id}"
                )
            except Exception as exc:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id "
                    f"{request_id}: {exc}"
                )

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    async def cancel_tts(self) -> None:
        """Stop in-progress synthesis on flush/interrupt."""
        await self._cancel_reader_task()

        if self.client:
            await self.client.cancel()

        if self.current_request_id:
            request_id = self.current_request_id
            if not self._audio_start_sent:
                await self.send_tts_audio_start(request_id=request_id)
                self._audio_start_sent = True
            request_event_interval = self._request_interval_ms()
            duration_ms = self._calculate_audio_duration_ms()
            if request_id in self.recorder_map:
                await self.recorder_map[request_id].flush()
            await self.send_tts_audio_end(
                request_id=request_id,
                request_event_interval_ms=request_event_interval,
                request_total_audio_duration_ms=duration_ms,
                reason=TTSAudioEndReason.INTERRUPTED,
            )

        self._reset_request_state()

    def vendor(self) -> str:
        return "gradium"

    def synthesize_audio_sample_rate(self) -> int:
        if self.current_request_sample_rate:
            return self.current_request_sample_rate
        if self.config:
            return self.config.get_sample_rate()
        return 24000

    async def request_tts(self, t: TTSTextInput) -> None:
        try:
            if self.client is None:
                self.client = GradiumTTSClient(
                    self.config, self.ten_env
                )  # type: ignore[arg-type]

            if t.request_id != self.current_request_id:
                self._reset_request_state()
                self.current_request_id = t.request_id
                await self._setup_recorder(t.request_id)
            elif self.current_request_finished:
                self.ten_env.log_error(
                    f"Received a message for a finished request_id "
                    f"'{t.request_id}'."
                )
                return

            text = t.text or ""
            if text.strip():
                if not self._session_started:
                    await self.client.start_session()
                    self.current_request_sample_rate = (
                        self.client.get_ready_sample_rate()
                    )
                    self.sent_ts = datetime.now()
                    self._session_started = True
                    self._reader_task = asyncio.create_task(
                        self._read_audio(t.request_id)
                    )
                self.metrics_add_output_characters(len(text))
                await self.client.send_text(text)

            if t.text_input_end:
                self.current_request_finished = True
                if self._session_started:
                    # The reader task finalizes the request when the vendor
                    # closes the stream after end_of_stream.
                    await self.client.end_input()
                else:
                    # No text was ever sent for this request — finalize now.
                    await self._finalize_request(
                        t.request_id,
                        TTSAudioEndReason.REQUEST_END,
                    )

        except GradiumTTSConnectionException as exc:
            # A write/setup failure can leave the reader task running; stop it
            # before finalizing so it can't outlive the request.
            await self._cancel_reader_task()
            await self._handle_connection_error(t.request_id, exc)
        except Exception as exc:
            await self._cancel_reader_task()
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}"
            )
            error = ModuleError(
                message=str(exc),
                module=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            )
            await self._finalize_request(
                t.request_id,
                TTSAudioEndReason.ERROR,
                error=error,
            )

    async def _read_audio(self, request_id: str) -> None:
        """Forward audio/metrics from the vendor stream until it ends."""
        try:
            async for data_msg, event_status in self.client.audio_events():
                if event_status == EVENT_TTS_RESPONSE:
                    if not isinstance(data_msg, bytes) or len(data_msg) == 0:
                        continue
                    if self.current_request_sample_rate is None:
                        self.current_request_sample_rate = (
                            self.client.get_ready_sample_rate()
                        )
                    self.total_audio_bytes += len(data_msg)
                    self.metrics_add_recv_audio_chunks(data_msg)
                    await self._write_dump(request_id, data_msg)
                    await self.send_tts_audio_data(data_msg)

                elif event_status == EVENT_TTS_TTFB_METRIC:
                    if isinstance(data_msg, int):
                        if not self._audio_start_sent:
                            await self.send_tts_audio_start(
                                request_id=request_id,
                            )
                            self._audio_start_sent = True
                        if not self._ttfb_sent:
                            await self.send_tts_ttfb_metrics(
                                request_id=request_id,
                                ttfb_ms=data_msg,
                                extra_metadata=(
                                    self.client.get_extra_metadata()
                                ),
                            )
                            self._ttfb_sent = True

                elif event_status == EVENT_TTS_END:
                    await self._finalize_request(
                        request_id,
                        TTSAudioEndReason.REQUEST_END,
                    )
                    return

                elif event_status == EVENT_TTS_ERROR:
                    error_msg = (
                        data_msg.decode("utf-8")
                        if isinstance(data_msg, bytes)
                        else str(data_msg)
                    )
                    error_code = (
                        ModuleErrorCode.FATAL_ERROR
                        if self._is_auth_error(error_msg)
                        else ModuleErrorCode.NON_FATAL_ERROR
                    )
                    error = ModuleError(
                        message=error_msg,
                        module=ModuleType.TTS,
                        code=error_code,
                        vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                    )
                    await self._finalize_request(
                        request_id,
                        TTSAudioEndReason.ERROR,
                        error=error,
                    )
                    return
        except asyncio.CancelledError:
            return
        except Exception:
            self.ten_env.log_error(
                f"Error in audio reader: {traceback.format_exc()}"
            )
            error = ModuleError(
                message="Gradium audio reader failed",
                module=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            )
            await self._finalize_request(
                request_id,
                TTSAudioEndReason.ERROR,
                error=error,
            )

    async def _handle_connection_error(
        self, request_id: str, error: GradiumTTSConnectionException
    ) -> None:
        error_code = (
            ModuleErrorCode.FATAL_ERROR
            if error.status_code in {401, 403}
            else ModuleErrorCode.NON_FATAL_ERROR
        )
        module_error = ModuleError(
            message=error.body,
            module=ModuleType.TTS,
            code=error_code,
            vendor_info=ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=str(error.status_code),
                message=error.body,
            ),
        )
        await self._finalize_request(
            request_id,
            TTSAudioEndReason.ERROR,
            error=module_error,
        )

    async def _finalize_request(
        self,
        request_id: str,
        reason: TTSAudioEndReason,
        error: ModuleError | None = None,
    ) -> None:
        if request_id != self.current_request_id:
            return

        self.current_request_finished = True

        if not self._audio_start_sent:
            await self.send_tts_audio_start(request_id=request_id)
            self._audio_start_sent = True

        request_event_interval = self._request_interval_ms()
        duration_ms = self._calculate_audio_duration_ms()
        await self.send_tts_audio_end(
            request_id=request_id,
            request_event_interval_ms=request_event_interval,
            request_total_audio_duration_ms=duration_ms,
            reason=reason,
        )

        if request_id in self.recorder_map:
            await self.recorder_map[request_id].flush()

        await self.finish_request(
            request_id=request_id,
            reason=reason,
            error=error,
        )
        self._reset_request_state()

    async def _cancel_reader_task(self) -> None:
        task = self._reader_task
        self._reader_task = None
        if task and not task.done():
            task.cancel()
            # CancelledError is a BaseException (not Exception) in py3.8+, so it
            # must be caught explicitly or it would propagate out of cancel_tts.
            try:
                await task
            except (asyncio.CancelledError, Exception):
                pass

    def _reset_request_state(self) -> None:
        self.current_request_id = None
        self.current_request_finished = False
        self.total_audio_bytes = 0
        self.sent_ts = None
        self._audio_start_sent = False
        self._ttfb_sent = False
        self.current_request_sample_rate = None
        self._session_started = False

    async def _setup_recorder(self, request_id: str) -> None:
        if not (self.config and self.config.dump):
            return

        for old_request_id in [
            rid for rid in self.recorder_map.keys() if rid != request_id
        ]:
            try:
                await self.recorder_map[old_request_id].flush()
                del self.recorder_map[old_request_id]
            except Exception as exc:
                self.ten_env.log_error(
                    f"Error cleaning up PCMWriter for request_id "
                    f"{old_request_id}: {exc}"
                )

        if request_id not in self.recorder_map:
            os.makedirs(self.config.dump_path, exist_ok=True)
            dump_file_path = os.path.join(
                self.config.dump_path,
                f"gradium_dump_{request_id}.pcm",
            )
            self.recorder_map[request_id] = PCMWriter(dump_file_path)

    async def _write_dump(self, request_id: str, data: bytes) -> None:
        if self.config and self.config.dump and request_id in self.recorder_map:
            try:
                await self.recorder_map[request_id].write(data)
            except Exception as exc:
                self.ten_env.log_error(f"Dump write failed: {exc}")

    def _request_interval_ms(self) -> int:
        if not self.sent_ts:
            return 0
        return int((datetime.now() - self.sent_ts).total_seconds() * 1000)

    def _calculate_audio_duration_ms(self) -> int:
        sample_rate = (
            self.current_request_sample_rate
            or self.synthesize_audio_sample_rate()
        )
        if sample_rate <= 0:
            return 0

        # Gradium streams 16-bit mono PCM.
        bytes_per_sample = 2
        channels = 1
        duration_sec = self.total_audio_bytes / (
            sample_rate * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)

    @staticmethod
    def _is_auth_error(message: str) -> bool:
        lowered = message.lower()
        return any(
            token in lowered
            for token in ("401", "403", "unauthorized", "forbidden")
        )
