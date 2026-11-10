import asyncio
import copy
import json
import os
from collections import deque
from typing import Any, Optional

from typing_extensions import override
from websockets.exceptions import InvalidStatus

from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
    AsyncASRBaseExtension,
)
from ten_ai_base.const import (
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_TRANSCRIPTS,
    LOG_CATEGORY_VENDOR,
)
from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
)
from ten_ai_base.struct import ASRWord
from ten_ai_base.utils import redact_url
from ten_runtime import AsyncTenEnv, AudioFrame, Data

from .config import DEFAULT_MODEL_ID, ElevenLabsASRConfig
from .const import DUMP_FILE_NAME
from .recognition import (
    ElevenLabsWSRecognition,
    ElevenLabsWSRecognitionCallback,
)
from .reconnect_manager import ReconnectManager


class ElevenLabsASRExtension(
    AsyncASRBaseExtension, ElevenLabsWSRecognitionCallback
):
    """ElevenLabs Scribe Realtime v2 ASR extension."""

    _FATAL_VENDOR_CODES = {
        "401",
        "403",
        "400",
        "auth_error",
        "quota_exceeded",
        "unaccepted_terms",
        "invalid_request",
        "input_error",
        "invalid_response",
    }

    def __init__(self, name: str):
        super().__init__(name)
        self.recognition: ElevenLabsWSRecognition | None = None
        self.config: ElevenLabsASRConfig | None = None
        self.audio_dumper: Dumper | None = None
        self.reconnect_manager: ReconnectManager | None = None

        self.sent_user_audio_duration_ms_before_last_reset = 0
        self._connection_epoch = 0
        self._replacing = False
        self._stopping = False
        self._fatal_error = False
        self._init_failed = False
        self._reconnect_task: asyncio.Task | None = None
        self._reported_error_epochs: set[int] = set()
        self._vendor_session_id: str | None = None

        # Local timing is used only when ElevenLabs omits word timestamps.
        # These values are reset at every provider session boundary.
        self._session_audio_duration_ms = 0
        self._session_audio_sent = False
        self._local_segment_start_ms = 0
        self._deferred_reconnect = False
        self._audio_handle_lock = asyncio.Lock()
        self.buffered_frames_size = 0

        self._finalize_lock = asyncio.Lock()
        self._finalize_request_lock = asyncio.Lock()
        self._finalize_event = asyncio.Event()
        self._finalize_pending = False
        self._finalize_completed = False
        self._pending_committed: deque[dict[str, Any]] = deque()
        # Retain fallback commits for delayed timestamp pairing, including
        # repeated text. Bound the history if enrichment messages are lost.
        self._timestamp_fallbacks: deque[str] = deque(maxlen=256)
        self._pending_timestamp_task: asyncio.Task | None = None
        self._finalize_metadata: dict[str, Any] | None = None

    @override
    def vendor(self) -> str:
        return "elevenlabs"

    @override
    def vendor_metadata(self) -> dict[str, Any]:
        if self.config is None:
            return {}
        params = self.config.params
        result = {
            "api_key": params.get("api_key"),
            "token": params.get("token"),
            "url": redact_url(params.get("ws_url", "")),
            "model": params.get("model_id", DEFAULT_MODEL_ID),
            "audio_format": params.get("audio_format"),
            "language_code": params.get("language_code"),
        }
        return {key: value for key, value in result.items() if value}

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        self.reconnect_manager = ReconnectManager(logger=ten_env)

        config_json, _ = await ten_env.get_property_to_json("")
        try:
            self.config = ElevenLabsASRConfig.model_validate_json(config_json)
            self.config.validate_config()
            ten_env.log_info(
                f"config: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            if self.config.dump and self.config.dump_path.strip():
                dump_file_path = os.path.join(
                    self.config.dump_path, DUMP_FILE_NAME
                )
                self.audio_dumper = Dumper(dump_file_path)
                await self.audio_dumper.start()
            elif self.config.dump:
                ten_env.log_warn(
                    "ElevenLabs ASR dump is enabled but dump_path is empty; "
                    "skipping audio dump"
                )
        except Exception as exc:
            self._init_failed = True
            ten_env.log_error(f"Invalid ElevenLabs ASR config: {exc}")
            self.config = ElevenLabsASRConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(exc),
                ),
            )

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        self._stopping = True
        self._cancel_timestamp_task()
        await self._cancel_reconnect_task()
        await super().on_stop(ten_env)

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        self._stopping = True
        self._fatal_error = True
        self._cancel_timestamp_task()
        await self._cancel_reconnect_task()
        try:
            await self.stop_connection()
            if self.audio_dumper:
                await self.audio_dumper.stop()
                self.audio_dumper = None
        finally:
            await super().on_deinit(ten_env)
            self.reconnect_manager = None
            self.config = None

    @override
    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        """Preserve finalize metadata before the base dispatches the request."""
        if data.get_name() == "asr_finalize":
            async with self._finalize_request_lock:
                metadata_json, error = data.get_property_to_json("metadata")
                if not error and metadata_json:
                    try:
                        parsed = copy.deepcopy(json.loads(metadata_json))
                        if isinstance(parsed, dict):
                            self._finalize_metadata = parsed
                    except (TypeError, ValueError):
                        ten_env.log_warn(
                            "ElevenLabs finalize metadata is not valid JSON"
                        )
                await super().on_data(ten_env, data)
            return
        await super().on_data(ten_env, data)

    @override
    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        """Wake a deferred reconnect when audio resumes after an idle close."""
        if self._deferred_reconnect and not self._stopping:
            self._deferred_reconnect = False
            self._schedule_reconnect()
        await super().on_audio_frame(ten_env, audio_frame)

    async def _handle_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        """Serialize normal and reconnect-triggered buffered frame delivery."""
        async with self._audio_handle_lock:
            await super()._handle_audio_frame(ten_env, audio_frame)

    async def _flush_buffered_audio(self) -> None:
        """Send frames buffered while an idle session was reconnecting.

        The base ASR consumer normally drains this queue when the next frame
        arrives.  A session can reconnect after the only post-idle frame,
        though, so explicitly kick the queue once the new socket is ready.
        """
        if not self.is_connected() or self.buffered_frames.empty():
            return
        async with self._audio_handle_lock:
            frames: list[AudioFrame] = []
            while True:
                try:
                    frames.append(self.buffered_frames.get_nowait())
                except asyncio.QueueEmpty:
                    break
            self.buffered_frames_size = 0
            for frame in frames:
                await super()._handle_audio_frame(self.ten_env, frame)

    @override
    async def start_connection(self) -> None:
        """Create a client and wait for the vendor session handshake."""
        if self._stopping:
            return
        if self._init_failed or self._fatal_error:
            await self.on_disconnected(
                code=ModuleErrorCode.FATAL_ERROR.value,
                message="ElevenLabs connection disabled after a fatal error",
            )
            return
        if self.config is None:
            return

        api_key = self.config.params.get("api_key", "")
        token = self.config.params.get("token", "")
        if not (
            isinstance(api_key, str)
            and api_key.strip()
            or isinstance(token, str)
            and token.strip()
        ):
            await self._report_connect_failure(
                "missing_credentials",
                "ElevenLabs API key or token is required",
                fatal=True,
            )
            return

        self._replacing = True
        self._connection_epoch += 1
        generation = self._connection_epoch
        old = self.recognition
        self.recognition = None
        if old is not None:
            await old.close(notify=False)

        client = ElevenLabsWSRecognition(
            audio_timeline=self.audio_timeline,
            ten_env=self.ten_env,
            params=self.config.params,
            callback=self,
            generation=generation,
        )
        self.recognition = client
        try:
            await client.start(
                timeout=float(self.config.params.get("connect_timeout", 10))
            )
            if self.recognition is not client or not client.is_connected():
                raise ConnectionError("ElevenLabs session is no longer active")
            self.ten_env.log_info(
                "vendor_status_changed: ElevenLabs session ready",
                category=LOG_CATEGORY_VENDOR,
            )
        except Exception as exc:
            if self.recognition is client:
                self.recognition = None
            code = self._vendor_code_from_exception(exc)
            fatal = code in self._FATAL_VENDOR_CODES
            await self._report_connect_failure(code, str(exc), fatal=fatal)
        finally:
            self._replacing = False
            # The base ASR consumer drains buffered frames before the next
            # live frame. Keeping that drain in one task preserves PCM dump
            # ordering and prevents concurrent writes to the async dumper.

    async def _report_connect_failure(
        self, code: str, message: str, *, fatal: bool
    ) -> None:
        if fatal:
            self._fatal_error = True
        generation_was_reported = (
            self._connection_epoch in self._reported_error_epochs
        )
        vendor_info = ModuleErrorVendorInfo(
            vendor=self.vendor(), code=str(code), message=message
        )
        if not generation_was_reported:
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=(
                        ModuleErrorCode.FATAL_ERROR.value
                        if fatal
                        else ModuleErrorCode.NON_FATAL_ERROR.value
                    ),
                    message=message,
                ),
                vendor_info,
            )
        await self.on_disconnected(
            code=(
                ModuleErrorCode.FATAL_ERROR.value
                if fatal
                else ModuleErrorCode.NON_FATAL_ERROR.value
            ),
            message=message,
            vendor_info=vendor_info,
        )
        if not fatal and not self._stopping:
            self._schedule_reconnect()

    @staticmethod
    def _vendor_code_from_exception(exc: Exception) -> str:
        if isinstance(exc, InvalidStatus):
            return str(exc.response.status_code)

        text = str(exc)
        if ":" in text:
            return text.split(":", 1)[0].strip()
        return exc.__class__.__name__

    @override
    async def on_open(
        self,
        generation: int | None = None,
        session_id: str | None = None,
        connect_delay_ms: int | None = None,
    ) -> None:
        if generation is not None and generation != self._connection_epoch:
            return
        # A reconnect creates a new vendor session. Any committed transcript
        # waiting for delayed timestamps belongs to the previous session and
        # must not be emitted with the new session's metadata.
        self._cancel_timestamp_task()
        self._pending_committed.clear()
        self._timestamp_fallbacks.clear()
        self._vendor_session_id = session_id
        self._session_audio_duration_ms = 0
        self._session_audio_sent = False
        self._local_segment_start_ms = 0
        self._deferred_reconnect = False
        if self.reconnect_manager:
            self.reconnect_manager.mark_connection_successful()
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.audio_timeline.get_total_user_audio_duration()
        )
        self.audio_timeline.reset()
        await self.on_connected()
        if connect_delay_ms is not None:
            await self.send_connect_delay_metrics(connect_delay_ms)

    @override
    async def on_result(
        self,
        message_data: dict[str, Any],
        generation: int | None = None,
    ) -> None:
        if generation is not None and generation != self._connection_epoch:
            return
        message_type = message_data.get("message_type", "")
        text = str(message_data.get("text", "") or "")
        self.ten_env.log_debug(
            f"vendor_result: {message_type} text_length={len(text)}",
            category=LOG_CATEGORY_TRANSCRIPTS,
        )

        if message_type == "partial_transcript":
            if text:
                await self._emit_result(message_data, final=False)
            return

        if message_type == "committed_transcript":
            if not text:
                if self._finalize_pending:
                    await self._complete_finalize()
                return
            pending = dict(message_data)
            pending["_connection_epoch"] = (
                generation if generation is not None else self._connection_epoch
            )
            self._pending_committed.append(pending)
            if self._timestamps_enabled():
                self._replace_timestamp_task()
            else:
                await self._emit_pending_committed()
            return

        if message_type == "committed_transcript_with_timestamps":
            if not text:
                if self._finalize_pending:
                    await self._complete_finalize()
                return
            if text in self._timestamp_fallbacks:
                self._timestamp_fallbacks.remove(text)
                return
            for candidate in self._pending_committed:
                if candidate.get("text") == text:
                    self._pending_committed.remove(candidate)
                    break
            else:
                # Every enrichment follows a committed_transcript. An
                # unmatched event has already been delivered or discarded.
                return
            if not self._pending_committed:
                self._cancel_timestamp_task()
            combined = dict(message_data)
            combined.setdefault("text", text)
            await self._emit_result(combined, final=True)
            return

    def _timestamps_enabled(self) -> bool:
        assert self.config is not None
        params = self.config.params
        return bool(
            params.get("include_timestamps")
            or params.get("include_language_detection")
        )

    def _replace_timestamp_task(self) -> None:
        if (
            self._pending_timestamp_task
            and not self._pending_timestamp_task.done()
        ):
            return
        self._pending_timestamp_task = asyncio.create_task(
            self._emit_pending_after_timestamp_window()
        )

    def _cancel_timestamp_task(self) -> None:
        task = self._pending_timestamp_task
        self._pending_timestamp_task = None
        if task and task is not asyncio.current_task() and not task.done():
            task.cancel()

    async def _emit_pending_after_timestamp_window(self) -> None:
        try:
            await asyncio.sleep(0.25)
            await self._emit_pending_committed()
        except asyncio.CancelledError:
            return

    async def _emit_pending_committed(self) -> None:
        while self._pending_committed:
            pending = self._pending_committed.popleft()
            if pending.get("_connection_epoch") != self._connection_epoch:
                continue
            if pending.get("text"):
                if self._timestamps_enabled():
                    self._timestamp_fallbacks.append(pending["text"])
                await self._emit_result(pending, final=True)

    def _result_metadata(
        self, timing_source: str | None = None, final: bool | None = None
    ) -> dict[str, Any]:
        metadata = (
            copy.deepcopy(self.metadata) if self.metadata is not None else {}
        )
        if not isinstance(metadata, dict):
            metadata = {}
        if "session_id" not in metadata and self.session_id is not None:
            metadata["session_id"] = self.session_id
        asr_info = metadata.get("asr_info")
        if not isinstance(asr_info, dict):
            asr_info = {}
        asr_info.update(
            {
                "vendor": self.vendor(),
                "model": self.config.params.get("model_id", DEFAULT_MODEL_ID),
                "vendor_session_id": self._vendor_session_id,
            }
        )
        if timing_source is not None:
            # ``asr_info`` is the existing extensible metadata object in the
            # TEN ASR contract; keep the estimate marker there instead of
            # adding a new top-level result field.
            asr_info["timing_source"] = timing_source
        if final is not None:
            # ElevenLabs' committed event is the provider's speech boundary.
            # Exposing that existing TEN turn-detector marker lets integrations
            # use a final-event mode when word timestamps are disabled.
            asr_info["speech_final"] = final
        metadata["asr_info"] = asr_info
        return metadata

    def _estimate_local_timing(self, final: bool) -> tuple[int, int, str]:
        """Estimate timing for a result without provider word timestamps.

        ElevenLabs does not expose word timing when timestamp enrichment is
        disabled.  The estimate uses successfully sent user audio in this
        provider session and can therefore include silence.  It is only a
        fallback for turn ordering; it is not caption-quality timing.
        """
        sent_audio_ms = self._session_audio_duration_ms
        if not self._session_audio_sent and sent_audio_ms <= 0:
            self.ten_env.log_warn(
                "ElevenLabs returned transcript timing without sent audio; "
                "leaving timing at zero"
            )
            return 0, 0, "unavailable"

        segment_start = min(self._local_segment_start_ms, sent_audio_ms)
        elapsed_ms = max(0, sent_audio_ms - segment_start)
        # A nonempty result with successfully sent audio must not look like an
        # old zero-length result to the turn detector.  One millisecond is the
        # smallest representable approximation when no new frame duration is
        # available yet.
        duration_ms = max(1, elapsed_ms)
        start_ms = (
            self.sent_user_audio_duration_ms_before_last_reset
            + self.audio_timeline.get_audio_duration_before_time(segment_start)
        )
        if final:
            self._local_segment_start_ms = sent_audio_ms
        return max(0, int(start_ms)), duration_ms, "local_estimate"

    async def _emit_result(
        self, message_data: dict[str, Any], *, final: bool
    ) -> None:
        text = str(message_data.get("text", "") or "")
        if not text:
            if final and self._finalize_pending:
                await self._complete_finalize()
            return

        words = self._parse_words(message_data.get("words", []))
        actual_start_ms = 0
        if words:
            start_ms = words[0].start_ms
            end_ms = max(word.start_ms + word.duration_ms for word in words)
            duration_ms = max(0, end_ms - start_ms)
            timing_source = "provider"
        else:
            actual_start_ms, duration_ms, timing_source = (
                self._estimate_local_timing(final)
            )
        if words:
            actual_start_ms = int(
                self.audio_timeline.get_audio_duration_before_time(start_ms)
                + self.sent_user_audio_duration_ms_before_last_reset
            )
            for word in words:
                word.start_ms = max(
                    0,
                    int(
                        self.audio_timeline.get_audio_duration_before_time(
                            word.start_ms
                        )
                        + self.sent_user_audio_duration_ms_before_last_reset
                    ),
                )
            if final:
                self._local_segment_start_ms = self._session_audio_duration_ms

        assert self.config is not None
        provider_language = message_data.get("language_code")
        if not isinstance(provider_language, str):
            provider_language = None
        requested_language = (
            self.config.params.get("language_code") or self.config.language
        )
        if (
            not provider_language
            or str(provider_language).strip().lower() in {"und", "unknown"}
        ) and requested_language:
            provider_language = requested_language
        language = self.config.normalize_language_code(provider_language)
        metadata = self._result_metadata(timing_source, final=final)
        result = ASRResult(
            text=text,
            final=final,
            start_ms=max(0, actual_start_ms),
            duration_ms=max(0, duration_ms),
            language=language,
            words=words,
            metadata=metadata,
        )
        await self.send_asr_result(result)
        if final and self._finalize_pending and not self._pending_committed:
            await self._complete_finalize()

    @staticmethod
    def _parse_words(raw_words: Any) -> list[ASRWord]:
        if not isinstance(raw_words, list):
            return []
        words: list[ASRWord] = []
        for raw in raw_words:
            if not isinstance(raw, dict) or raw.get("type") not in (
                None,
                "word",
            ):
                continue
            word = str(raw.get("text", "") or "")
            if not word:
                continue
            try:
                start = max(0, int(float(raw.get("start", 0)) * 1000))
                end = max(start, int(float(raw.get("end", 0)) * 1000))
            except (TypeError, ValueError):
                continue
            words.append(
                ASRWord(
                    word=word,
                    start_ms=start,
                    duration_ms=end - start,
                    stable=True,
                )
            )
        return words

    @override
    async def on_error(
        self,
        error_msg: str,
        error_code: Optional[str] = None,
        generation: int | None = None,
    ) -> None:
        if generation is not None and generation != self._connection_epoch:
            return
        code = str(error_code or "unknown")
        if generation is not None and generation in self._reported_error_epochs:
            return
        if generation is not None:
            self._reported_error_epochs.add(generation)
        fatal = code in self._FATAL_VENDOR_CODES
        if fatal:
            self._fatal_error = True
        self.ten_env.log_error(
            f"vendor_error: code: {code}, reason: {error_msg}",
            category=LOG_CATEGORY_VENDOR,
        )
        await self.send_asr_error(
            ModuleError(
                module=ModuleType.ASR,
                code=(
                    ModuleErrorCode.FATAL_ERROR.value
                    if fatal
                    else ModuleErrorCode.NON_FATAL_ERROR.value
                ),
                message=error_msg,
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(), code=code, message=error_msg
            ),
        )
        if fatal:
            await self.on_disconnected(
                code=ModuleErrorCode.FATAL_ERROR.value,
                message=error_msg,
                vendor_info=ModuleErrorVendorInfo(
                    vendor=self.vendor(), code=code, message=error_msg
                ),
            )

    @override
    async def on_close(
        self,
        vendor_code: int = 0,
        vendor_message: str = "closed",
        generation: int | None = None,
        _normal: bool = False,
    ) -> None:
        if generation is not None and generation != self._connection_epoch:
            return
        if self._stopping or self._replacing:
            return
        await self.on_disconnected(
            code=vendor_code,
            message=vendor_message,
            vendor_info=ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=str(vendor_code),
                message=vendor_message,
            ),
        )
        if self._finalize_pending:
            await self._complete_finalize()
        if not self._fatal_error:
            if self._session_audio_sent:
                self._schedule_reconnect()
            else:
                # ElevenLabs intentionally closes sessions that have been
                # idle. Keep the socket closed until audio resumes instead
                # of creating an endless reconnect loop with no input.
                self._deferred_reconnect = True
                self.ten_env.log_info(
                    "ElevenLabs session closed before audio; "
                    "deferring reconnect until the next audio frame",
                    category=LOG_CATEGORY_VENDOR,
                )

    @override
    async def on_event(
        self,
        message_data: dict[str, Any],
        generation: int | None = None,
    ) -> None:
        if generation is not None and generation != self._connection_epoch:
            return
        if message_data.get("message_type") == "warning":
            self.ten_env.log_warn(
                f"ElevenLabs warning: {message_data.get('warning', '')}",
                category=LOG_CATEGORY_VENDOR,
            )

    @override
    async def finalize(self, _session_id: str | None) -> None:
        async with self._finalize_lock:
            self._finalize_pending = True
            self._finalize_completed = False
            self._finalize_event.clear()
            self._cancel_timestamp_task()
            try:
                if not self.recognition or not self.is_connected():
                    await self._complete_finalize()
                    return
                if not await self.recognition.send_final():
                    await self._complete_finalize()
                    return
                timeout = float(
                    (self.config.params if self.config else {}).get(
                        "finalize_timeout", 5
                    )
                )
                try:
                    await asyncio.wait_for(
                        self._finalize_event.wait(), timeout=timeout
                    )
                except asyncio.TimeoutError:
                    self.ten_env.log_warn(
                        "ElevenLabs finalize timed out; completing handshake"
                    )
                    await self._complete_finalize()
            except Exception as exc:
                self.ten_env.log_error(f"ElevenLabs finalize failed: {exc}")
                await self._complete_finalize()

    async def _complete_finalize(self) -> None:
        if not self._finalize_pending or self._finalize_completed:
            return
        self._finalize_completed = True
        self._finalize_pending = False
        self._cancel_timestamp_task()
        try:
            await self._emit_pending_committed()
        except Exception as exc:
            self.ten_env.log_error(
                f"ElevenLabs pending final result failed: {exc}"
            )
        saved_metadata = self.metadata
        if self._finalize_metadata is not None:
            self.metadata = copy.deepcopy(self._finalize_metadata)
        try:
            await self.send_asr_finalize_end()
        except Exception as exc:
            self.ten_env.log_error(
                f"ElevenLabs finalize-end delivery failed: {exc}"
            )
        finally:
            self.metadata = saved_metadata
            self._finalize_metadata = None
            self._finalize_event.set()

    async def _handle_reconnect(self) -> None:
        if not self.reconnect_manager:
            return
        try:
            while not self._stopping and not self._fatal_error:
                if not self.reconnect_manager.can_retry():
                    self._fatal_error = True
                    await self.send_asr_error(
                        ModuleError(
                            module=ModuleType.ASR,
                            code=ModuleErrorCode.FATAL_ERROR.value,
                            message="ElevenLabs reconnect attempts exhausted",
                        ),
                        ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code="reconnect_exhausted",
                            message="ElevenLabs reconnect attempts exhausted",
                        ),
                    )
                    return
                await self.reconnect_manager.handle_reconnect(
                    self.start_connection
                )
                if self.is_connected():
                    await self._flush_buffered_audio()
                    return
        finally:
            if self._reconnect_task is asyncio.current_task():
                self._reconnect_task = None

    def _schedule_reconnect(self) -> None:
        if self._stopping or self._fatal_error:
            return
        if self._reconnect_task and not self._reconnect_task.done():
            return
        self._reconnect_task = asyncio.create_task(self._handle_reconnect())

    async def _cancel_reconnect_task(self) -> None:
        task = self._reconnect_task
        self._reconnect_task = None
        if task and task is not asyncio.current_task() and not task.done():
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

    @override
    async def stop_connection(self) -> None:
        self._replacing = True
        self._connection_epoch += 1
        client = self.recognition
        self.recognition = None
        try:
            if self._finalize_pending:
                await self._complete_finalize()
            if client:
                await client.close(notify=False)
        finally:
            self._replacing = False

    @override
    def is_connected(self) -> bool:
        return (
            not self._replacing
            and self.recognition is not None
            and self.recognition.is_connected()
        )

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=10 * 1024 * 1024)

    @override
    def input_audio_sample_rate(self) -> int:
        if self.config is None:
            return 16000
        return int(self.config.params.get("sample_rate", 16000))

    @override
    def input_audio_channels(self) -> int:
        return 1

    @override
    def input_audio_sample_width(self) -> int:
        return 2

    @override
    async def send_audio(
        self, frame: AudioFrame, _session_id: str | None
    ) -> bool:
        if not self.recognition or not self.is_connected():
            return False
        buf = None
        try:
            buf = frame.lock_buf()
            audio_data = bytes(buf)
            if self.audio_dumper:
                await self.audio_dumper.push_bytes(audio_data)
            sent = await self.recognition.send_audio_frame(audio_data)
            if sent:
                self._session_audio_sent = True
                self._session_audio_duration_ms += self._audio_duration_ms(
                    len(audio_data)
                )
            return sent
        except Exception as exc:
            self.ten_env.log_error(f"Error sending audio to ElevenLabs: {exc}")
            return False
        finally:
            if buf is not None:
                frame.unlock_buf(buf)

    def _audio_duration_ms(self, byte_count: int) -> int:
        return int(
            byte_count
            / self.input_audio_sample_rate()
            / self.input_audio_channels()
            / self.input_audio_sample_width()
            * 1000
        )
