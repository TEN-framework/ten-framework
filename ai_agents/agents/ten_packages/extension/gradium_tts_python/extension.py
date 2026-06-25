#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from dataclasses import dataclass
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
from ten_runtime import AsyncTenEnv, Data

from .config import GradiumTTSConfig
from .gradium_tts import (
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
    GradiumTTSClient,
    GradiumTTSConnectionException,
)


@dataclass
class RequestContext:
    pending_text: str = ""
    finished: bool = False
    total_audio_bytes: int = 0
    audio_start_sent: bool = False
    ttfb_sent: bool = False
    sample_rate: int | None = None
    sent_ts: datetime | None = None
    session_id: str = ""
    turn_id: int = -1


class GradiumTTSExtension(AsyncTTS2BaseExtension):
    """Gradium TTS extension using the websocket streaming API."""

    MIN_STREAM_FLUSH_CHARS = 80
    MAX_BUFFERED_CHARS = 120

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: GradiumTTSConfig | None = None
        self.client: GradiumTTSClient | None = None
        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.sent_ts: datetime | None = None
        self.current_request_finished: bool = False
        self.total_audio_bytes: int = 0
        self._audio_start_sent: bool = False
        self.current_request_sample_rate: int | None = None
        self.recorder_map: dict[str, PCMWriter] = {}
        self.request_contexts: dict[str, RequestContext] = {}
        self.ingress_messages: dict[str, list[TTSTextInput]] = {}

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

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        if data.get_name() == "tts_text_input":
            data_payload, err = data.get_property_to_json("")
            if err:
                raise RuntimeError(f"Failed to get data payload: {err}")

            try:
                t = TTSTextInput.model_validate_json(data_payload)
            except Exception:
                # Let the base class keep its normal invalid-payload handling.
                await super().on_data(ten_env, data)
                return

            self.ingress_messages.setdefault(t.request_id, []).append(t)

        await super().on_data(ten_env, data)

        if data.get_name() == "tts_flush":
            await self._clear_local_request_state()

    async def cancel_tts(self) -> None:
        if (
            self.current_request_id
            and self.current_request_id in self.request_contexts
        ):
            self.request_contexts[self.current_request_id].finished = True
        self.current_request_finished = True
        if self.current_request_id and self.client:
            await self.client.cancel()
            await self._finalize_request(
                self.current_request_id,
                TTSAudioEndReason.INTERRUPTED,
            )

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
            t = self._pop_next_ingress_message(t.request_id, t)
            if self.client is None:
                self.client = GradiumTTSClient(
                    self.config, self.ten_env
                )  # type: ignore[arg-type]

            is_new_request = t.request_id not in self.request_contexts
            context = self._get_request_context(t.request_id, t.metadata)
            self.current_request_id = t.request_id
            self.current_request_finished = context.finished
            self.total_audio_bytes = context.total_audio_bytes
            self.sent_ts = context.sent_ts
            self._audio_start_sent = context.audio_start_sent
            self.current_request_sample_rate = context.sample_rate
            self._apply_request_context(context)

            if is_new_request:
                await self._setup_recorder(t.request_id)
            elif context.finished:
                self.ten_env.log_error(
                    f"Received a message for a finished request_id "
                    f"'{t.request_id}' with text_input_end=False."
                )
                return

            if t.text_input_end:
                context.finished = True
                self.current_request_finished = True

            incoming_text = t.text or ""
            if incoming_text.strip():
                context.pending_text = f"{context.pending_text}{incoming_text}"

            if self._should_flush_pending_text(context.pending_text, t):
                text_to_send = context.pending_text.strip()
                context.pending_text = ""
                if text_to_send:
                    await self._process_tts_text(t.request_id, text_to_send, t)
            elif t.text_input_end:
                await self._finalize_request(
                    t.request_id,
                    TTSAudioEndReason.REQUEST_END,
                )

        except GradiumTTSConnectionException as exc:
            await self._handle_connection_error(t.request_id, exc)
        except Exception as exc:
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

    async def _process_tts_text(
        self, request_id: str, text: str, t: TTSTextInput
    ) -> None:
        context = self._get_request_context(request_id, t.metadata)
        self.current_request_id = request_id
        self._apply_request_context(context)
        self.metrics_add_output_characters(len(text))
        context.sent_ts = datetime.now()
        self.sent_ts = context.sent_ts

        async for data_msg, event_status in self.client.get(
            text, request_id, t.text_input_end
        ):
            if event_status == EVENT_TTS_RESPONSE:
                if not isinstance(data_msg, bytes) or len(data_msg) == 0:
                    continue

                ready_sample_rate = self.client.get_ready_sample_rate()
                if ready_sample_rate:
                    context.sample_rate = ready_sample_rate
                    self.current_request_sample_rate = ready_sample_rate

                context.total_audio_bytes += len(data_msg)
                self.total_audio_bytes = context.total_audio_bytes
                self.metrics_add_recv_audio_chunks(data_msg)
                await self._write_dump(request_id, data_msg)
                await self.send_tts_audio_data(data_msg)

            elif event_status == EVENT_TTS_TTFB_METRIC:
                if isinstance(data_msg, int):
                    context.sent_ts = datetime.now()
                    self.sent_ts = context.sent_ts
                    if not context.audio_start_sent:
                        await self.send_tts_audio_start(
                            request_id=request_id,
                        )
                        context.audio_start_sent = True
                        self._audio_start_sent = True
                    if not context.ttfb_sent:
                        await self.send_tts_ttfb_metrics(
                            request_id=request_id,
                            ttfb_ms=data_msg,
                            extra_metadata=self.client.get_extra_metadata(),
                        )
                        context.ttfb_sent = True

            elif event_status == EVENT_TTS_END:
                if t.text_input_end:
                    await self._finalize_request(
                        request_id,
                        TTSAudioEndReason.REQUEST_END,
                    )
                break

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
                if t.text_input_end:
                    await self._finalize_request(
                        request_id,
                        TTSAudioEndReason.ERROR,
                        error=error,
                    )
                else:
                    await self.send_tts_error(
                        request_id=request_id,
                        error=error,
                    )
                break

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
        context = self.request_contexts.get(request_id)
        if context is None:
            return

        self.current_request_id = request_id
        self.current_request_finished = True
        context.finished = True
        context.pending_text = ""
        self._apply_request_context(context)

        if not context.audio_start_sent:
            await self.send_tts_audio_start(
                request_id=request_id,
            )
            context.audio_start_sent = True
            self._audio_start_sent = True

        request_event_interval = self._current_request_interval_ms(context)
        duration_ms = self._calculate_audio_duration_ms(context)
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
        del self.request_contexts[request_id]
        self.ingress_messages.pop(request_id, None)

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

    def _current_request_interval_ms(self, context: RequestContext) -> int:
        if not context.sent_ts:
            return 0
        return int((datetime.now() - context.sent_ts).total_seconds() * 1000)

    def _calculate_audio_duration_ms(self, context: RequestContext) -> int:
        sample_rate = context.sample_rate or self.synthesize_audio_sample_rate()
        if sample_rate <= 0:
            return 0

        bytes_per_sample = 2
        channels = 1
        duration_sec = context.total_audio_bytes / (
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

    def _should_flush_pending_text(
        self, pending_text: str, t: TTSTextInput
    ) -> bool:
        if not pending_text.strip():
            return False

        if t.text_input_end:
            return True

        stripped = pending_text.rstrip()
        if not stripped:
            return False

        last_char = stripped[-1]
        if (
            last_char in {".", "!", "?", ";"}
            and len(stripped) >= self.MIN_STREAM_FLUSH_CHARS
        ):
            return True

        # Keep comma-delimited fragments together so counting/list responses
        # do not become many tiny independent Gradium requests.
        if last_char == ",":
            return False

        # Colons and multiline prefixes are often followed by a list; wait
        # for more text unless the buffer has grown large enough.
        if (
            last_char in {":", "\n"}
            and len(stripped) < self.MIN_STREAM_FLUSH_CHARS
        ):
            return False

        return len(stripped) >= self.MAX_BUFFERED_CHARS

    def _get_request_context(
        self,
        request_id: str,
        metadata: dict | None,
    ) -> RequestContext:
        context = self.request_contexts.get(request_id)
        if context is None:
            context = RequestContext()
            self.request_contexts[request_id] = context

        if metadata is not None:
            context.session_id = metadata.get("session_id", "")
            context.turn_id = metadata.get("turn_id", -1)
        return context

    def _apply_request_context(self, context: RequestContext) -> None:
        self.session_id = context.session_id
        self.current_turn_id = context.turn_id

    def _pop_next_ingress_message(
        self, request_id: str, fallback: TTSTextInput
    ) -> TTSTextInput:
        queued_messages = self.ingress_messages.get(request_id)
        if not queued_messages:
            return fallback

        next_message = queued_messages.pop(0)
        if not queued_messages:
            self.ingress_messages.pop(request_id, None)
        return next_message

    async def _clear_local_request_state(self) -> None:
        for recorder in list(self.recorder_map.values()):
            try:
                await recorder.flush()
            except Exception:
                pass

        self.recorder_map.clear()
        self.request_contexts.clear()
        self.ingress_messages.clear()
        self.current_request_id = None
        self.current_request_finished = False
        self.total_audio_bytes = 0
        self._audio_start_sent = False
        self.current_request_sample_rate = None
        self.sent_ts = None
