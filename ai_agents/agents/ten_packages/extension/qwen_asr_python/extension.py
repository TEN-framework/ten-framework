import asyncio
import copy
import contextvars
import json
import os
from typing import Any

from typing_extensions import override
from websockets.exceptions import ConnectionClosedOK

from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
    AsyncASRBaseExtension,
)
from ten_ai_base.const import (
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_VENDOR,
    PROPERTY_KEY_METADATA,
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
from ten_runtime import AsyncTenEnv, AudioFrame

from .client import QwenASRClient, QwenProtocolError
from .config import MODEL_NAME, QwenASRConfig


MODULE_NAME_ASR = ModuleType.ASR
BUFFER_LIMIT = 10 * 1024 * 1024


class QwenASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: QwenASRConfig | None = None
        self.client: QwenASRClient | None = None
        self.audio_dumper: Dumper | None = None
        self.connected = False
        self._init_failed = False
        self._stop_requested = False
        self._fatal_error = False
        self._send_lock = asyncio.Lock()
        self._swap_lock = asyncio.Lock()
        self._finalize_lock = asyncio.Lock()
        self._finalize_dispatch_active: contextvars.ContextVar[bool] = (
            contextvars.ContextVar(
                "qwen_finalize_dispatch_active", default=False
            )
        )
        self._connection_swap_depth = 0
        self._reconnect_task: asyncio.Task[None] | None = None
        self._reconnect_attempt = 0
        self._generation = 0
        self._connection_start = 0.0
        self._task_audio_start_ms = 0
        self._task_contexts: dict[str, tuple[dict[str, Any], str | None]] = {}
        self._active_task_id: str | None = None
        self._active_session_id: str | None = None
        self._task_starting = False
        self._task_start_task: asyncio.Task[None] | None = None
        self._task_recovery_task: asyncio.Task[None] | None = None
        self._buffer_flush_head: AudioFrame | None = None
        self._dump_lock = asyncio.Lock()

    @override
    def vendor(self) -> str:
        return "qwen"

    @override
    def vendor_metadata(self) -> dict[str, Any]:
        config = self.config
        if config is None:
            return {}
        return {
            "vendor": self.vendor(),
            "model": MODEL_NAME,
            "url": config.redacted_url,
            "sample_rate": config.sample_rate,
            "api_key": config.api_key,
        }

    @override
    async def on_data(self, ten_env: AsyncTenEnv, data: Any) -> None:
        if data.get_name() == "asr_finalize":
            async with self._finalize_lock:
                raw, error = data.get_property_to_json(None)
                if not error:
                    try:
                        finalize_payload = json.loads(raw)
                        finalize_metadata = (
                            finalize_payload.get("metadata") or {}
                        )
                        if isinstance(finalize_metadata, dict):
                            self.metadata = copy.deepcopy(finalize_metadata)
                            if finalize_metadata.get("session_id") is not None:
                                self.session_id = finalize_metadata[
                                    "session_id"
                                ]
                    except (TypeError, json.JSONDecodeError):
                        pass
                token = self._finalize_dispatch_active.set(True)
                try:
                    await super().on_data(ten_env, data)
                finally:
                    self._finalize_dispatch_active.reset(token)
            return
        await super().on_data(ten_env, data)

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        config_json, error = await ten_env.get_property_to_json("")
        try:
            if error:
                raise ValueError(str(error))
            self.config = QwenASRConfig.model_validate_json(config_json)
            self.config.apply_defaults()
            self.config.validate_config()
            ten_env.log_info(
                f"config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            if self.config.dump:
                dump_path = os.path.join(self.config.dump_path, "qwen_asr.pcm")
                self.audio_dumper = Dumper(dump_path)
                await self.audio_dumper.start()
        except Exception as exc:
            self._init_failed = True
            self.auto_connect = False
            self.config = None
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(exc),
                )
            )

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        if self.audio_dumper:
            await self.audio_dumper.stop()
            self.audio_dumper = None

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        self._stop_requested = True
        if self._task_recovery_task and not self._task_recovery_task.done():
            self._task_recovery_task.cancel()
            try:
                await self._task_recovery_task
            except asyncio.CancelledError:
                pass
            self._task_recovery_task = None
        if self._task_start_task and not self._task_start_task.done():
            self._task_start_task.cancel()
            try:
                await self._task_start_task
            except asyncio.CancelledError:
                pass
            self._task_start_task = None
        if self._reconnect_task and not self._reconnect_task.done():
            self._reconnect_task.cancel()
            try:
                await self._reconnect_task
            except asyncio.CancelledError:
                pass
            self._reconnect_task = None
        await super().on_stop(ten_env)

    @override
    async def start_connection(self) -> None:
        if self._init_failed or self._stop_requested or self._fatal_error:
            return
        assert self.config is not None
        flushed_audio: list[bytes] = []
        ready_to_report_connected = False
        generation = 0
        if self._task_start_task and not self._task_start_task.done():
            self._task_start_task.cancel()
            try:
                await self._task_start_task
            except asyncio.CancelledError:
                pass
            self._task_start_task = None
            self._task_starting = False
        async with self._swap_lock:
            self._connection_swap_depth += 1
            try:
                self._connection_start = asyncio.get_event_loop().time()
                generation = self._generation + 1
                self._generation = generation
                await self._stop_connection_locked()
                client = QwenASRClient(
                    url=self.config.url,
                    api_key=self.config.api_key,
                    run_task_factory=self.config.build_run_task,
                    finish_task_factory=self.config.build_finish_task,
                    event_callback=lambda event: self._handle_event(
                        event, generation
                    ),
                    close_callback=lambda reason, exc: self._handle_close(
                        reason, generation, exc
                    ),
                    timeout_s=self.config.connect_timeout_s,
                )
                self.client = client
                self._task_contexts.clear()
                try:
                    await client.connect()
                    if self._stop_requested or generation != self._generation:
                        await client.close()
                        return
                    self.connected = True
                    self._reconnect_attempt = 0
                    self._start_task_timeline()
                except asyncio.CancelledError:
                    await client.close()
                    raise
                except Exception as exc:
                    if generation != self._generation:
                        return
                    await self._abort_connection_attempt(generation, exc)
                    return
                try:
                    flushed_audio = await self._flush_buffered_frames_locked()
                except Exception as exc:
                    await self._abort_connection_attempt(generation, exc)
                    return
                ready_to_report_connected = (
                    generation == self._generation
                    and self.connected
                    and self.client is client
                    and client.connected
                )
                if ready_to_report_connected:
                    await self._dump_audio(flushed_audio)
            finally:
                self._connection_swap_depth -= 1
        if ready_to_report_connected:
            await self.on_connected()
            await self.send_connect_delay_metrics(
                int(
                    (asyncio.get_event_loop().time() - self._connection_start)
                    * 1000
                )
            )

    @override
    async def stop_connection(self) -> None:
        async with self._swap_lock:
            self._connection_swap_depth += 1
            try:
                await self._stop_connection_locked()
            finally:
                self._connection_swap_depth -= 1

    async def _stop_connection_locked(self) -> None:
        client = self.client
        self.client = None
        self.connected = False
        if client:
            await client.close()

    @override
    def is_connected(self) -> bool:
        return (
            self._connection_swap_depth == 0
            and self.connected
            and not self._task_starting
            and self.client is not None
            and self.client.connected
            and self.client.task_ready
        )

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=BUFFER_LIMIT)

    @override
    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else 16000

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        buf = None
        try:
            buf = frame.lock_buf()
            audio_data = bytes(buf)
            frame_metadata = self._frame_metadata(frame)
            frame_session_id = frame_metadata.get("session_id", session_id)
            async with self._swap_lock:
                client = self.client
                task_start_task = self._task_start_task
                if (
                    client is None
                    or not self.is_connected()
                    or self._task_starting
                    or (
                        task_start_task is not None
                        and not task_start_task.done()
                    )
                ):
                    return False
                if not await self._prepare_task_for_session_locked(
                    client, frame_session_id
                ):
                    return False
                async with self._send_lock:
                    if (
                        client is not self.client
                        or not self.connected
                        or not client.connected
                        or not client.task_ready
                    ):
                        return False
                    if not await self._send_audio_locked(
                        audio_data, frame_metadata, frame_session_id
                    ):
                        return False
            await self._dump_audio([audio_data])
            return True
        except Exception as exc:
            fatal = self._is_fatal_exception(exc)
            self._fatal_error = self._fatal_error or fatal
            if not self.stopped:
                await self._recover_after_task_failure(exc, fatal)
            return False
        finally:
            if buf is not None:
                frame.unlock_buf(buf)

    @override
    async def finalize(self, session_id: str | None) -> None:
        if not self._finalize_dispatch_active.get():
            async with self._finalize_lock:
                await self._finalize_impl(session_id)
            return
        await self._finalize_impl(session_id)

    async def _finalize_impl(self, session_id: str | None) -> None:
        finalize_id = self.finalize_id
        finalize_metadata = copy.deepcopy(self.metadata or {})
        try:
            async with self._swap_lock:
                client = self.client
                if self.is_connected() and client is not None:
                    try:
                        await asyncio.wait_for(
                            client.finish_task(),
                            timeout=(
                                (self.config.finalize_timeout_ms / 1000)
                                if self.config
                                else 5
                            ),
                        )
                        self._clear_active_task(client)
                        self._schedule_task_start(client)
                    except asyncio.CancelledError:
                        client.task_ready = False
                        raise
                    except Exception as exc:
                        client.task_ready = False
                        fatal = self._is_fatal_exception(exc)
                        self._schedule_task_recovery(client, exc, fatal)
        finally:
            self.finalize_id = finalize_id
            self.metadata = finalize_metadata
            if session_id is not None:
                self.session_id = session_id
                self.metadata.setdefault("session_id", session_id)
            await self.send_asr_finalize_end()

    async def _handle_event(
        self, event: dict[str, Any], generation: int
    ) -> None:
        if generation != self._generation or self._stop_requested:
            return
        event_task_id = (event.get("header") or {}).get("task_id")
        client_task_id = getattr(self.client, "task_id", None)
        if (
            event_task_id is not None
            and isinstance(client_task_id, str)
            and event_task_id != client_task_id
        ):
            return
        event_name = (event.get("header") or {}).get("event")
        if event_name == "task-failed":
            await self._handle_task_failed(event)
        elif event_name == "result-generated":
            await self._handle_result(event)

    async def _handle_task_failed(self, event: dict[str, Any]) -> None:
        client = self.client
        code, message = QwenASRClient.parse_task_failed(event)
        fatal = self._is_fatal_vendor_code(code)
        if client is not None:
            client.task_ready = False
        if fatal:
            self._fatal_error = True
            self.connected = False
        if client is not None and client.waiting_for_task_event is True:
            return
        # Let the reader return before recovery closes and joins it.
        self._schedule_task_recovery(
            client, QwenProtocolError(message, code), fatal
        )

    async def _handle_result(self, event: dict[str, Any]) -> None:
        payload = event.get("payload") or {}
        if payload.get("heartbeat"):
            return
        output = payload.get("output") or {}
        sentence = output.get("sentence") or {}
        text = str(sentence.get("text") or payload.get("text") or "").strip()
        if not text:
            return
        final = bool(sentence.get("sentence_end", False))
        begin_time = int(sentence.get("begin_time") or 0)
        end_time = sentence.get("end_time")
        start_ms = self._timeline_time(begin_time)
        duration = max(
            0, self._timeline_time(int(end_time or begin_time)) - start_ms
        )
        words: list[ASRWord] = []
        for word in sentence.get("words") or []:
            word_start = int(word.get("begin_time") or 0)
            word_end = int(word.get("end_time") or word_start)
            word_start_ms = self._timeline_time(word_start)
            words.append(
                ASRWord(
                    word=str(word.get("text") or "")
                    + str(word.get("punctuation") or ""),
                    start_ms=word_start_ms,
                    duration_ms=max(
                        0, self._timeline_time(word_end) - word_start_ms
                    ),
                    stable=final or bool(word.get("fixed", False)),
                )
            )
        event_task_id = (event.get("header") or {}).get("task_id")
        context = self._task_contexts.get(event_task_id or "")
        if context is None and event_task_id is None and self._active_task_id:
            context = self._task_contexts.get(self._active_task_id)
        context_metadata, context_session_id = context or (
            copy.deepcopy(self.metadata) if self.metadata else {},
            self.session_id,
        )
        metadata = copy.deepcopy(context_metadata)
        if context_session_id is not None:
            metadata["session_id"] = context_session_id
        metadata["asr_info"] = {
            "vendor": self.vendor(),
            "model": MODEL_NAME,
            "request_id": payload.get("request_id"),
            "sentence_id": sentence.get("sentence_id"),
            "locked": False,
            "usage": payload.get("usage") or {},
        }
        previous_metadata = self.metadata
        previous_session_id = self.session_id
        self.metadata = copy.deepcopy(metadata)
        self.metadata.pop("asr_info", None)
        self.session_id = context_session_id
        try:
            await self.send_asr_result(
                ASRResult(
                    text=text,
                    final=final,
                    start_ms=start_ms,
                    duration_ms=duration,
                    language=self.config.language if self.config else "und",
                    words=words,
                    metadata=metadata,
                )
            )
        finally:
            self.metadata = previous_metadata
            self.session_id = previous_session_id
        self.ten_env.log_debug(
            f"qwen result: final={final} text={text!r}",
            category=LOG_CATEGORY_VENDOR,
        )

    def _timeline_time(self, vendor_time_ms: int) -> int:
        return self.audio_timeline.get_audio_duration_before_time(
            self._task_audio_start_ms + vendor_time_ms
        )

    def _start_task_timeline(self) -> None:
        self._task_audio_start_ms = (
            self.audio_timeline.total_user_audio_duration
            + self.audio_timeline.total_silence_audio_duration
        )

    def _clear_active_task(self, client: QwenASRClient) -> None:
        task_id = getattr(client, "task_id", None)
        if isinstance(task_id, str):
            self._task_contexts.pop(task_id, None)
        if self._active_task_id == task_id:
            self._active_task_id = None
            self._active_session_id = None

    async def _dump_audio(self, audio_chunks: list[bytes]) -> None:
        if not self.audio_dumper or not audio_chunks:
            return
        async with self._dump_lock:
            for audio_data in audio_chunks:
                try:
                    await self.audio_dumper.push_bytes(audio_data)
                except Exception as exc:
                    self.ten_env.log_error(
                        f"qwen audio dump failed: {redact_url(str(exc))}",
                        category=LOG_CATEGORY_VENDOR,
                    )

    def _schedule_task_recovery(
        self, client: QwenASRClient, exc: BaseException, fatal: bool
    ) -> None:
        self.connected = False
        self._fatal_error = self._fatal_error or fatal
        if self._task_recovery_task and not self._task_recovery_task.done():
            return
        self._task_recovery_task = asyncio.create_task(
            self._recover_client(client, exc, fatal)
        )

    def _schedule_task_start(self, client: QwenASRClient) -> None:
        if self._stop_requested or self._fatal_error:
            return
        if self._task_start_task and not self._task_start_task.done():
            return
        self._task_start_task = asyncio.create_task(
            self._start_next_task(client)
        )

    async def _start_next_task(self, client: QwenASRClient) -> None:
        flushed_audio: list[bytes] = []
        try:
            async with self._swap_lock:
                if self.client is not client or not self.connected:
                    return
                self._connection_swap_depth += 1
                self._task_starting = True
                try:
                    await client.start_task()
                    self._start_task_timeline()
                    flushed_audio = await self._flush_buffered_frames_locked()
                finally:
                    self._task_starting = False
                    self._connection_swap_depth -= 1
            await self._dump_audio(flushed_audio)
        except Exception as exc:
            fatal = self._is_fatal_exception(exc)
            self._fatal_error = self._fatal_error or fatal
            self._schedule_task_recovery(client, exc, fatal)
        finally:
            if self._task_start_task is asyncio.current_task():
                self._task_start_task = None

    async def _recover_after_task_failure(
        self,
        exc: BaseException,
        fatal: bool,
        generation: int | None = None,
    ) -> None:
        client = self.client
        if client is None:
            if not self.stopped:
                await self._send_vendor_error(exc, fatal=fatal)
            return
        if generation is not None and generation != self._generation:
            return
        self._schedule_task_recovery(client, exc, fatal)
        if self._task_recovery_task is not asyncio.current_task():
            await self._task_recovery_task

    async def _recover_client(
        self, client: QwenASRClient, exc: BaseException, fatal: bool
    ) -> None:
        recovery_generation = self._generation
        try:
            async with self._swap_lock:
                self._connection_swap_depth += 1
                try:
                    if self.client is not client:
                        return
                    self.client = None
                    self.connected = False
                    self._clear_active_task(client)
                    await client.close()
                finally:
                    self._connection_swap_depth -= 1
            if self._stop_requested or self._generation != recovery_generation:
                return
            if self.client is not None and self.client is not client:
                return
            await self._send_vendor_error(exc, fatal=fatal)
            if self._stop_requested or self._generation != recovery_generation:
                return
            if self.client is not None and self.client is not client:
                return
            await self.on_disconnected(
                code=(
                    ModuleErrorCode.FATAL_ERROR.value
                    if fatal
                    else ModuleErrorCode.NON_FATAL_ERROR.value
                ),
                message=redact_url(str(exc)),
            )
            if (
                not fatal
                and self._generation == recovery_generation
                and not self.is_connected()
            ):
                self._schedule_reconnect()
        finally:
            if self._task_recovery_task is asyncio.current_task():
                self._task_recovery_task = None

    async def _handle_close(
        self,
        reason: str,
        generation: int,
        exc: BaseException | None = None,
    ) -> None:
        if generation != self._generation or self._stop_requested:
            return
        async with self._swap_lock:
            self._connection_swap_depth += 1
            try:
                if generation != self._generation or self._stop_requested:
                    return
                client = self.client
                self.connected = False
                self.client = None
                if client is not None:
                    await client.close()
            finally:
                self._connection_swap_depth -= 1
        clean_close = isinstance(exc, ConnectionClosedOK)
        fatal = self._is_fatal_exception(exc) if exc is not None else False
        vendor_info = ModuleErrorVendorInfo(
            vendor=self.vendor(),
            code=(
                self._vendor_error_code(exc)
                if self._status_code(exc) is not None
                else "websocket_closed"
            ),
            message=redact_url(reason),
        )
        await self.on_disconnected(
            code=(
                0
                if clean_close
                else (
                    ModuleErrorCode.FATAL_ERROR.value
                    if fatal
                    else ModuleErrorCode.NON_FATAL_ERROR.value
                )
            ),
            message=redact_url(reason),
            vendor_info=vendor_info,
        )
        # Normal idle closes (1000/1001) must not enter ASR error stats.
        if not clean_close:
            vendor_exception = (
                exc
                if self._status_code(exc) is not None
                else QwenProtocolError(reason, "websocket_closed")
            )
            await self._send_vendor_error(
                vendor_exception,
                fatal=fatal,
            )
        if fatal:
            self._fatal_error = True
        else:
            self._schedule_reconnect()

    async def _send_vendor_error(self, exc: Exception, fatal: bool) -> None:
        if self._stop_requested:
            return
        message = redact_url(str(exc))
        vendor_code = self._vendor_error_code(exc)
        self.ten_env.log_error(
            f"qwen vendor error: code={vendor_code} message={message}",
            category=LOG_CATEGORY_VENDOR,
        )
        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=(
                    ModuleErrorCode.FATAL_ERROR.value
                    if fatal
                    else ModuleErrorCode.NON_FATAL_ERROR.value
                ),
                message=message,
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(), code=vendor_code, message=message
            ),
        )

    @staticmethod
    def _is_fatal_exception(exc: BaseException) -> bool:
        status = QwenASRExtension._status_code(exc)
        if status in {400, 401, 403, 404, 405, 406, 422}:
            return True
        code = getattr(exc, "code", None)
        return isinstance(code, str) and QwenASRExtension._is_fatal_vendor_code(
            code
        )

    @staticmethod
    def _status_code(exc: BaseException | None) -> int | None:
        if exc is None:
            return None
        status = getattr(exc, "status_code", None)
        response = getattr(exc, "response", None)
        if status is None and response is not None:
            status = getattr(response, "status_code", None)
        if isinstance(status, int) and 100 <= status <= 599:
            return status
        return None

    @staticmethod
    def _vendor_error_code(
        exc: BaseException | None, default: str = "unknown"
    ) -> str:
        status = QwenASRExtension._status_code(exc)
        if status is not None:
            return str(status)
        return str(getattr(exc, "code", default))

    @staticmethod
    def _frame_metadata(frame: AudioFrame) -> dict[str, Any]:
        raw, error = frame.get_property_to_json(PROPERTY_KEY_METADATA)
        if error or not raw:
            return {}
        try:
            metadata = json.loads(raw)
        except (TypeError, json.JSONDecodeError):
            return {}
        return metadata if isinstance(metadata, dict) else {}

    async def _send_audio_locked(
        self,
        audio_data: bytes,
        frame_metadata: dict[str, Any],
        session_id: str | None,
    ) -> bool:
        client = self.client
        if not self.connected or client is None or not client.connected:
            return False
        task_id = getattr(client, "task_id", None)
        if not isinstance(task_id, str):
            task_id = None
        task_metadata = copy.deepcopy(frame_metadata)
        if session_id is not None:
            task_metadata.setdefault("session_id", session_id)
        if task_id is not None:
            self._task_contexts[task_id] = (task_metadata, session_id)
        self._active_task_id = task_id
        self._active_session_id = session_id
        await client.send_audio(audio_data)
        self.audio_timeline.add_user_audio(
            int(len(audio_data) / (self.input_audio_sample_rate() / 1000 * 2))
        )
        if frame_metadata:
            self.metadata = copy.deepcopy(frame_metadata)
        self.session_id = session_id
        return True

    async def _prepare_task_for_session_locked(
        self, client: QwenASRClient | None, session_id: str | None
    ) -> bool:
        """Swap vendor tasks while holding the connection swap lock only."""
        if (
            client is None
            or self.client is not client
            or not self.connected
            or not client.connected
        ):
            return False
        if (
            client.task_ready
            and self._active_session_id is not None
            and self._active_session_id != session_id
        ):
            await client.finish_task()
            self._clear_active_task(client)
        if not client.task_ready:
            self._task_starting = True
            try:
                await client.start_task()
                self._start_task_timeline()
            finally:
                self._task_starting = False
        return client.task_ready

    async def _flush_buffered_frames_locked(self) -> list[bytes]:
        flushed_audio: list[bytes] = []
        pending = self._buffer_flush_head
        while True:
            if pending is None:
                try:
                    pending = self.buffered_frames.get_nowait()
                except asyncio.QueueEmpty:
                    break
            buffered_frame = pending
            pending = None
            frame_metadata = self._frame_metadata(buffered_frame)
            frame_session_id = frame_metadata.get("session_id", self.session_id)
            audio_data = bytes(buffered_frame.get_buf())
            frame_size = len(audio_data)
            try:
                if not await self._prepare_task_for_session_locked(
                    self.client, frame_session_id
                ):
                    sent = False
                else:
                    async with self._send_lock:
                        sent = await self._send_audio_locked(
                            audio_data,
                            frame_metadata,
                            frame_session_id,
                        )
            except Exception:
                self._buffer_flush_head = buffered_frame
                raise
            if not sent:
                self._buffer_flush_head = buffered_frame
                break
            flushed_audio.append(audio_data)
            self.buffered_frames_size -= frame_size
            if self.first_audio_time is None:
                self.first_audio_time = asyncio.get_event_loop().time()
        if pending is not None:
            self._buffer_flush_head = pending
        self.buffered_frames_size = max(0, self.buffered_frames_size)
        return flushed_audio

    async def _abort_connection_attempt(
        self, generation: int, exc: BaseException
    ) -> None:
        if generation != self._generation or self._stop_requested:
            return
        self.connected = False
        await self._stop_connection_locked()
        fatal = self._is_fatal_exception(exc)
        self._fatal_error = self._fatal_error or fatal
        await self._send_vendor_error(exc, fatal=fatal)
        await self.on_disconnected(
            code=(
                ModuleErrorCode.FATAL_ERROR.value
                if fatal
                else ModuleErrorCode.NON_FATAL_ERROR.value
            ),
            message=redact_url(str(exc)),
        )
        if not fatal and not self._stop_requested:
            self._schedule_reconnect()

    @staticmethod
    def _is_fatal_vendor_code(code: str) -> bool:
        normalized = code.lower().replace("-", "").replace("_", "")
        if normalized.isdigit():
            return int(normalized) in {400, 401, 403, 404, 405, 406, 422}
        return normalized in {
            "unauthorized",
            "forbidden",
            "invalidapikey",
            "authentication",
            "permissiondenied",
            "invalidparameter",
            "invaliduri",
            "invalidurl",
            "unsupported",
        }

    def _schedule_reconnect(self) -> None:
        if self._stop_requested or self._fatal_error:
            return
        if self._reconnect_task and not self._reconnect_task.done():
            return
        self._reconnect_task = asyncio.create_task(self._reconnect_loop())

    async def _reconnect_loop(self) -> None:
        assert self.config is not None
        attempt = 0
        try:
            # Non-fatal reconnect has no ceiling; keep backing off until the
            # session reconnects, stop is requested, or a fatal latch is set.
            while True:
                if self._stop_requested or self._fatal_error:
                    return
                attempt += 1
                self._reconnect_attempt = attempt
                await asyncio.sleep(min(2 ** (attempt - 1), 8))
                if self._stop_requested or self._fatal_error:
                    return
                try:
                    await self.start_connection()
                except Exception:
                    continue
                if self.is_connected():
                    return
        finally:
            if self._reconnect_task is asyncio.current_task():
                self._reconnect_task = None
