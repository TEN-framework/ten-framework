import asyncio
import copy
import contextvars
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
        self._swap_lock = asyncio.Lock()
        self._client_lock = asyncio.Lock()
        self._result_lock = asyncio.Lock()
        self._output_metadata = contextvars.ContextVar(
            f"elevenlabs_metadata_{id(self)}", default=None
        )
        self._vendor_session_id: str | None = None

        # Local timing is used only when ElevenLabs omits word timestamps.
        # These values are reset at every provider session boundary.
        self._session_audio_duration_ms = 0
        self._session_audio_sent = False
        self._local_segment_start_ms = 0
        self._deferred_reconnect = False
        self._audio_handle_lock = asyncio.Lock()
        self._audio_consumer_task: asyncio.Task | None = None
        self.buffered_frames_size = 0

        self._finalize_lock = asyncio.Lock()
        self._finalize_request_lock = asyncio.Lock()
        self._finalize_event = asyncio.Event()
        self._finalize_pending = False
        self._finalize_completed = False
        self._pending_committed: deque[dict[str, Any]] = deque()
        # Bounded tombstones retain the sent-audio upper bound at each commit.
        # See the documented conservative repeated-text pairing policy.
        self._timestamp_fallbacks: deque[dict[str, Any]] = deque(maxlen=256)
        self._pending_timestamp_task: asyncio.Task | None = None
        self._finalize_metadata: dict[str, Any] | None = None

    @property
    def metadata(self) -> dict[str, Any] | None:
        """Keep delayed output context independent of incoming audio metadata."""
        context = getattr(self, "_output_metadata", None)
        if context is not None and context.get() is not None:
            return context.get()
        return getattr(self, "_stream_metadata", None)

    @metadata.setter
    def metadata(self, value: dict[str, Any] | None) -> None:
        self._stream_metadata = value

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
        await self._cancel_reconnect_task()
        await super().on_stop(ten_env)
        task = self._audio_consumer_task
        if task and task is not asyncio.current_task() and not task.done():
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        self._stopping = True
        self._fatal_error = True
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

    async def _audio_frame_consumer(self) -> None:
        """Retain the base queue boundary and expose a drain barrier."""
        self._audio_consumer_task = asyncio.current_task()
        while not self.stopped and not self._stopping:
            frame = await self.audio_frames_queue.get()
            try:
                await self._handle_audio_frame(self.ten_env, frame)
            except Exception as exc:
                self.ten_env.log_error(
                    f"Error consuming ElevenLabs audio: {exc}"
                )
            finally:
                self.audio_frames_queue.task_done()

    def _retain_frame(self, frame: AudioFrame) -> None:
        limit = self.buffer_strategy().byte_limit
        size = len(frame.get_buf())
        while self.buffered_frames_size + size > limit:
            try:
                dropped = self.buffered_frames.get_nowait()
            except asyncio.QueueEmpty:
                break
            self.buffered_frames_size -= len(dropped.get_buf())
            self.dropped_audio_frames += 1
        if size <= limit:
            self.buffered_frames.put_nowait(frame)
            self.buffered_frames_size += size

    async def _deliver_frame(self, frame: AudioFrame) -> bool:
        if not await self.send_audio(frame, None):
            return False
        self.sent_buffer_length += len(frame.get_buf())
        if self.first_audio_time is None:
            self.first_audio_time = asyncio.get_running_loop().time()
        return True

    async def _handle_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        del ten_env
        if not audio_frame.get_buf() or self._stopping:
            return
        async with self._audio_handle_lock:
            await self._flush_buffered_audio_locked()
            if not self.is_connected() or not await self._deliver_frame(
                audio_frame
            ):
                self._retain_frame(audio_frame)

    async def _flush_buffered_audio_locked(self) -> None:
        while self.is_connected() and not self.buffered_frames.empty():
            frame = self.buffered_frames.get_nowait()
            self.buffered_frames_size -= len(frame.get_buf())
            if not await self._deliver_frame(frame):
                # Restore a failed head before newer frames, without resending
                # a frame that was already accepted by the transport.
                remaining = []
                while not self.buffered_frames.empty():
                    remaining.append(self.buffered_frames.get_nowait())
                self.buffered_frames.put_nowait(frame)
                for queued in remaining:
                    self.buffered_frames.put_nowait(queued)
                self.buffered_frames_size += len(frame.get_buf())
                return

    async def _flush_buffered_audio(self) -> None:
        async with self._audio_handle_lock:
            await self._flush_buffered_audio_locked()

    @override
    async def start_connection(self) -> None:
        """Serialize all replacements, including explicit trigger_connect."""
        async with self._swap_lock:
            if self._stopping or self._init_failed or self._fatal_error:
                await self.on_disconnected(
                    code=(
                        ModuleErrorCode.FATAL_ERROR.value
                        if self._fatal_error or self._init_failed
                        else 0
                    ),
                    message="ElevenLabs connection disabled",
                )
                return
            if self.is_connected():
                await self.on_connected()
                return
            if self.config is None:
                return
            self._replacing = True
            client = None
            generation = self._connection_epoch
            try:
                # Handshakes hold the swap lock, never the client lock.
                async with self._client_lock:
                    old = self.recognition
                    self.recognition = None
                    if old is not None:
                        await old.close(notify=False)
                async with self._result_lock:
                    self._cancel_timestamp_task()
                    await self._emit_pending_committed()
                self._connection_epoch += 1
                generation = self._connection_epoch
                client = ElevenLabsWSRecognition(
                    audio_timeline=self.audio_timeline,
                    ten_env=self.ten_env,
                    params=self.config.params,
                    callback=self,
                    generation=generation,
                )
                self.recognition = client
                await client.start(
                    timeout=float(self.config.params.get("connect_timeout", 10))
                )
                if self._stopping or not client.is_connected():
                    raise ConnectionError(
                        "ElevenLabs session is no longer active"
                    )
            except asyncio.CancelledError:
                if client is not None:
                    await client.close(notify=False)
                if self.recognition is client:
                    self.recognition = None
                await self.on_disconnected(message="connection cancelled")
                raise
            except Exception as exc:
                if client is not None:
                    await client.close(notify=False)
                if generation == self._connection_epoch:
                    self.recognition = None
                    code = self._vendor_code_from_exception(exc)
                    await self._report_connect_failure(
                        code, str(exc), fatal=code in self._FATAL_VENDOR_CODES
                    )
            finally:
                self._replacing = False
            # Drain after initial, explicit and automatic connects, even when
            # no later live frame arrives.
            if self.is_connected():
                await self.audio_frames_queue.join()
                await self._flush_buffered_audio()

    async def _report_connect_failure(
        self, code: str, message: str, *, fatal: bool
    ) -> None:
        if fatal:
            self._fatal_error = True
        vendor_info = ModuleErrorVendorInfo(
            vendor=self.vendor(), code=str(code), message=message
        )
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
        async with self._result_lock:
            self._cancel_timestamp_task()
            await self._emit_pending_committed()
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
        async with self._result_lock:
            await self._on_result_locked(message_data, generation)

    async def _on_result_locked(
        self, message_data: dict[str, Any], generation: int | None
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
            pending["_result"] = self._prepare_result(pending, final=True)
            pending["_audio_end_ms"] = (
                self.audio_timeline.total_user_audio_duration
                + self.audio_timeline.total_silence_audio_duration
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
            words = self._parse_words(message_data.get("words", []))
            word_end = max(
                (word.start_ms + word.duration_ms for word in words), default=0
            )
            for fallback in list(self._timestamp_fallbacks):
                if fallback["text"] != text:
                    continue
                if not words or word_end <= fallback["_audio_end_ms"]:
                    self._timestamp_fallbacks.remove(fallback)
                    return
                # Beyond audio sent at the earlier commit: its enrichment
                # was missing, so this later event must not be consumed by it.
                self._timestamp_fallbacks.remove(fallback)
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
            result = self._prepare_result(
                combined, final=True, advance_segment=False
            )
            result.metadata = candidate["_result"].metadata
            result.metadata["asr_info"]["timing_source"] = "provider"
            if not result.words:
                result = candidate["_result"]
            await self._send_result(result)
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
            async with self._result_lock:
                await self._emit_pending_committed()
        except asyncio.CancelledError:
            return

    async def _emit_pending_committed(self) -> None:
        while self._pending_committed:
            pending = self._pending_committed.popleft()
            if pending.get("text"):
                if self._timestamps_enabled():
                    self._timestamp_fallbacks.append(pending)
                await self._send_result(pending["_result"])

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
        await self._send_result(self._prepare_result(message_data, final=final))

    def _prepare_result(
        self,
        message_data: dict[str, Any],
        *,
        final: bool,
        advance_segment: bool = True,
    ) -> ASRResult:
        text = str(message_data.get("text", "") or "")
        words = self._parse_words(message_data.get("words", []))
        actual_start_ms = 0
        if words:
            start_ms = words[0].start_ms
            end_ms = max(word.start_ms + word.duration_ms for word in words)
            duration_ms = max(0, end_ms - start_ms)
            timing_source = "provider"
        else:
            actual_start_ms, duration_ms, timing_source = (
                self._estimate_local_timing(final and advance_segment)
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
            if final and advance_segment:
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
        return ASRResult(
            text=text,
            final=final,
            start_ms=max(0, actual_start_ms),
            duration_ms=max(0, duration_ms),
            language=language,
            words=words,
            metadata=metadata,
        )

    async def _send_result(self, result: ASRResult) -> None:
        token = self._output_metadata.set(copy.deepcopy(result.metadata))
        try:
            delivery = asyncio.create_task(self.send_asr_result(result))
            try:
                await asyncio.shield(delivery)
            except asyncio.CancelledError:
                # Once removed from the pending queue, a committed final must
                # finish delivery even when a reconnect task is being stopped.
                await delivery
                raise
        finally:
            self._output_metadata.reset(token)
        if (
            result.final
            and self._finalize_pending
            and not self._pending_committed
        ):
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
        fatal = code in self._FATAL_VENDOR_CODES
        if fatal:
            # Invalidate immediately, then wait for active sends before close.
            self._fatal_error = True
            async with self._client_lock:
                if (
                    generation is not None
                    and generation != self._connection_epoch
                ):
                    return
                client = self.recognition
                self.recognition = None
                if client is not None:
                    await client.close(notify=False)
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
        if self._stopping or self._replacing or self._fatal_error:
            return
        await self.on_disconnected(
            code=0 if _normal else ModuleErrorCode.NON_FATAL_ERROR.value,
            message=vendor_message,
            vendor_info=ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=str(vendor_code),
                message=vendor_message,
            ),
        )
        if self._finalize_pending:
            await self._finish_finalize()
        if not self._fatal_error:
            if (
                self._session_audio_sent
                or not self.buffered_frames.empty()
                or not self.audio_frames_queue.empty()
            ):
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
        async with self._swap_lock, self._finalize_lock:
            await self.audio_frames_queue.join()
            await self._flush_buffered_audio()
            self._finalize_pending = True
            self._finalize_completed = False
            self._finalize_event.clear()
            try:
                if not self.recognition or not self.is_connected():
                    await self._finish_finalize()
                    return
                async with self._client_lock:
                    client = self.recognition
                    sent = bool(
                        self.is_connected()
                        and client
                        and await client.send_final()
                    )
                if not sent:
                    await self._finish_finalize()
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
                    await self._finish_finalize()
            except Exception as exc:
                self.ten_env.log_error(f"ElevenLabs finalize failed: {exc}")
                await self._finish_finalize()

    async def _finish_finalize(self) -> None:
        async with self._result_lock:
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
        token = self._output_metadata.set(
            copy.deepcopy(self._finalize_metadata or self.metadata or {})
        )
        try:
            await self.send_asr_finalize_end()
        except Exception as exc:
            self.ten_env.log_error(
                f"ElevenLabs finalize-end delivery failed: {exc}"
            )
        finally:
            self._output_metadata.reset(token)
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
        async with self._swap_lock:
            self._replacing = True
            try:
                async with self._client_lock:
                    client = self.recognition
                    self.recognition = None
                    if client:
                        await client.close(notify=False)
                async with self._result_lock:
                    self._cancel_timestamp_task()
                    await self._emit_pending_committed()
                    if self._finalize_pending:
                        await self._complete_finalize()
                self._connection_epoch += 1
            finally:
                self._replacing = False

    @override
    def is_connected(self) -> bool:
        return (
            not self._replacing
            and not self._fatal_error
            and not self._stopping
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
        async with self._client_lock:
            client = self.recognition
            if client is None or not self.is_connected():
                return False
            buf = None
            try:
                # The frame context and client share one operation lock.
                # A suspended dump cannot resume against a replaced client.
                metadata_json, error = frame.get_property_to_json("metadata")
                if not error and metadata_json:
                    metadata = json.loads(metadata_json)
                    if isinstance(metadata, dict):
                        self.metadata = metadata
                        self.session_id = metadata.get(
                            "session_id", self.session_id
                        )
                buf = frame.lock_buf()
                audio_data = bytes(buf)
                if self.audio_dumper:
                    await self.audio_dumper.push_bytes(audio_data)
                sent = await client.send_audio_frame(audio_data)
                if sent:
                    self._session_audio_sent = True
                    self._session_audio_duration_ms += self._audio_duration_ms(
                        len(audio_data)
                    )
                return sent
            except Exception as exc:
                self.ten_env.log_error(
                    f"Error sending audio to ElevenLabs: {exc}"
                )
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
