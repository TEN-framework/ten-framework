import asyncio
import contextvars
import copy
import json
import os
import time
from functools import partial
from typing import Any, Optional

from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    AsyncASRBaseExtension,
)
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
)
from ten_ai_base.struct import ASRResult, ASRWord
from ten_runtime import AsyncTenEnv, AudioFrame, Data
from typing_extensions import override
from websockets.exceptions import ConnectionClosedOK

from .config import FishAudioASRConfig, redact_configured_url
from .websocket import FishAudioASRClient

MODULE_NAME_ASR = "fish_asr"
SUPPORTED_SAMPLE_RATES = {8000, 12000, 16000, 22050, 24000, 32000, 44100, 48000}
LANGUAGE_LOCALES = {
    "en": "en-US",
    "zh": "zh-CN",
    "es": "es-ES",
    "fr": "fr-FR",
    "de": "de-DE",
    "ja": "ja-JP",
    "ko": "ko-KR",
    "pt": "pt-BR",
    "it": "it-IT",
    "ru": "ru-RU",
}


class FishAudioASRExtension(AsyncASRBaseExtension):
    """Stream PCM16 frames and publish Fish committed turns as TEN results."""

    def __init__(self, name: str):
        super().__init__(name)
        self.config: FishAudioASRConfig | None = None
        self.client: FishAudioASRClient | None = None
        self.audio_dumper: Dumper | None = None
        self._dump_lock = asyncio.Lock()
        self._lifecycle_lock = asyncio.Lock()
        self._audio_lock = asyncio.Lock()
        self._finalize_lock = asyncio.Lock()
        self._finalize_context: contextvars.ContextVar[
            dict[str, Any] | None
        ] = contextvars.ContextVar("fish_finalize_context", default=None)
        self._generation = 0
        self._opening = False
        self._connection_stopping = False
        self._vendor_error_reported = False
        self._connect_started_ms = 0
        self._last_draft: dict[int, dict[str, Any]] = {}
        self._turn_start_ms: dict[int, int] = {}
        self._user_turns: set[int] = set()
        self._last_audio_ms = 0
        self._session_audio_bytes = 0
        self._total_audio_bytes = 0
        self._session_origin_ms = 0
        self._finalize_waiter: asyncio.Future[None] | None = None
        self._session_fatal = False
        self._reconnect_task: asyncio.Task[None] | None = None
        self._stream_session_id: str | None = None
        self._stream_metadata: dict[str, Any] | None = None
        self._buffered_session_id: str | None = None
        self.auto_connect = True

    @override
    def vendor(self) -> str:
        return "fish_audio"

    @override
    def vendor_metadata(self) -> dict[str, Any]:
        if self.config is None:
            return {}
        return {
            "api_key": self.config.api_key,
            "url": redact_configured_url(self.config.url),
            "model": self.config.model,
        }

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        config_json, error = await ten_env.get_property_to_json("")
        try:
            if error:
                raise ValueError(str(error))
            self.config = FishAudioASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)
            self.config.validate_config(SUPPORTED_SAMPLE_RATES)
            ten_env.log_info(
                f"config: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            if self.config.dump:
                self.audio_dumper = Dumper(
                    os.path.join(self.config.dump_path, "fish_asr_in.pcm")
                )
                await self.audio_dumper.start()
        except Exception as error:
            ten_env.log_error(f"invalid property: {error}")
            self.auto_connect = False
            self._session_fatal = True
            self.config = None
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(error),
                )
            )

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_stop(ten_env)
        finally:
            await self._stop_audio_dumper()

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        try:
            self.stopped = True
            await self.stop_connection()
            await super().on_deinit(ten_env)
        finally:
            await self._stop_audio_dumper()

    async def _stop_audio_dumper(self) -> None:
        async with self._dump_lock:
            if self.audio_dumper is not None:
                await self.audio_dumper.stop()
                self.audio_dumper = None

    @override
    async def start_connection(self) -> None:
        async with self._lifecycle_lock:
            if self.is_connected():
                return
            if self.stopped or self._session_fatal or self.config is None:
                await self.on_disconnected(message="connection disabled")
                return
            await self._close_client()
            self._opening = True
            self._connect_started_ms = int(time.time() * 1000)
            self._vendor_error_reported = False
            generation = self._generation
            client = FishAudioASRClient(
                url=self.config.url,
                api_key=self.config.api_key,
                model=self.config.model,
                config_message=self.config.build_config_message(),
                on_message=partial(self._handle_message, generation=generation),
                on_close=partial(self._handle_close, generation=generation),
            )
            self.client = client
            try:
                await client.connect()
            except asyncio.CancelledError:
                await self._close_client()
                raise
            except Exception as error:
                response = getattr(error, "response", None)
                status = getattr(response, "status_code", None)
                fatal = self._session_fatal or (
                    status is not None
                    and 400 <= status < 500
                    and status not in {408, 429}
                )
                self._session_fatal = fatal
                severity = (
                    ModuleErrorCode.FATAL_ERROR.value
                    if fatal
                    else ModuleErrorCode.NON_FATAL_ERROR.value
                )
                if not self._vendor_error_reported and not self.stopped:
                    await self.send_asr_error(
                        ModuleError(
                            module=MODULE_NAME_ASR,
                            code=severity,
                            message=f"Fish Audio connection failed: {error}",
                        ),
                        ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(status or "connection_error"),
                            message=str(error),
                        ),
                    )
                await self.on_disconnected(code=severity, message=str(error))
                await self._close_client()
                self._schedule_reconnect()
            finally:
                self._opening = False

    @override
    async def stop_connection(self) -> None:
        self._connection_stopping = True
        try:
            reconnect_task = self._reconnect_task
            if reconnect_task and reconnect_task is not asyncio.current_task():
                reconnect_task.cancel()
                await asyncio.gather(reconnect_task, return_exceptions=True)
            self._reconnect_task = None
            async with self._lifecycle_lock:
                await self._close_client()
        finally:
            self._connection_stopping = False

    async def _close_client(self) -> None:
        async with self._audio_lock:
            client = self.client
            self.client = None
            self._generation += 1
            self._stream_session_id = None
            self._stream_metadata = None
            self._finish_finalize_waiter()
        if client:
            await client.close()

    @override
    def is_connected(self) -> bool:
        return bool(
            not self._opening
            and not self._connection_stopping
            and not self._session_fatal
            and not self.stopped
            and self.client
            and self.client.connected
        )

    @override
    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else 16000

    @override
    def input_audio_channels(self) -> int:
        return 1

    @override
    def input_audio_sample_width(self) -> int:
        return 2

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=10 * 1024 * 1024)

    @override
    async def send_audio(
        self, frame: AudioFrame, _session_id: Optional[str]
    ) -> bool:
        async with self._audio_lock:
            if not self.is_connected():
                return False
            return await self._send_audio_locked(frame, _session_id)

    async def _handle_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        if not audio_frame.get_buf():
            return
        # Serialize the base consumer with the handshake's explicit drain.
        async with self._audio_lock:
            if self.is_connected():
                if await self._flush_buffered_frames_locked():
                    session_id = self._frame_session_id(audio_frame)
                    if (
                        self._stream_metadata is not None
                        and session_id != self._stream_session_id
                    ):
                        # A Fish stream has one result owner. Do not put a
                        # frame from a new TEN session on the old stream.
                        self._drop_audio_frame_locked(audio_frame)
                        return
                    if await self._send_audio_locked(audio_frame, session_id):
                        return
            await self._buffer_frame_locked(ten_env, audio_frame)

    def _frame_metadata(self, frame: AudioFrame) -> dict[str, Any]:
        getter = getattr(frame, "get_property_to_json", None)
        if getter is None:
            return {}
        try:
            raw, error = getter("metadata")
        except Exception as error:
            self.ten_env.log_warn(f"failed to read audio metadata: {error}")
            return {}
        if error or not raw:
            return {}
        try:
            metadata = json.loads(raw)
            if isinstance(metadata, dict):
                return copy.deepcopy(metadata)
        except (TypeError, json.JSONDecodeError) as error:
            self.ten_env.log_warn(f"invalid audio metadata: {error}")
        return {}

    def _frame_session_id(self, frame: AudioFrame) -> str | None:
        metadata = self._frame_metadata(frame)
        return self._metadata_session_id(metadata)

    def _metadata_session_id(self, metadata: dict[str, Any]) -> str | None:
        session_id = metadata.get("session_id", self.session_id)
        return session_id if isinstance(session_id, str) else None

    async def _buffer_frame_locked(
        self, ten_env: AsyncTenEnv, frame: AudioFrame
    ) -> None:
        frame_metadata = self._frame_metadata(frame)
        session_id = self._metadata_session_id(frame_metadata)
        if (
            not self.buffered_frames.empty()
            and session_id != self._buffered_session_id
        ):
            # A Keep queue cannot safely contain audio for two vendor result
            # owners. The newer session supersedes stale disconnected audio.
            self._discard_buffered_frames_locked()
        self._buffered_session_id = session_id
        if frame_metadata:
            self.metadata = copy.deepcopy(frame_metadata)
            if session_id is not None:
                self.session_id = session_id
        await super()._handle_audio_frame(ten_env, frame)

    def _drop_audio_frame_locked(self, frame: AudioFrame) -> None:
        frame_size = len(frame.get_buf())
        self.audio_timeline.add_dropped_audio(
            self._audio_duration_ms(frame_size)
        )
        self.dropped_audio_frames += 1

    def _discard_buffered_frames_locked(self) -> None:
        while not self.buffered_frames.empty():
            frame = self.buffered_frames.get_nowait()
            self._drop_audio_frame_locked(frame)
        self.buffered_frames_size = 0
        self._buffered_session_id = None

    def _audio_duration_ms(self, byte_count: int) -> int:
        return int(
            byte_count
            * 1000
            / (
                self.input_audio_sample_rate()
                * self.input_audio_channels()
                * self.input_audio_sample_width()
            )
        )

    async def _send_audio_locked(
        self, frame: AudioFrame, session_id: str | None = None
    ) -> bool:
        if not self.client or not self.client.connected:
            return False
        buf = frame.get_buf()
        if not buf:
            return False
        data = bytes(buf)
        frame_metadata = self._frame_metadata(frame)
        if "session_id" in frame_metadata:
            frame_session_id = self._metadata_session_id(frame_metadata)
        elif session_id is not None:
            frame_session_id = session_id
        else:
            frame_session_id = self._metadata_session_id(frame_metadata)
        if self._stream_metadata is None:
            stream_metadata = frame_metadata or copy.deepcopy(
                self.metadata or {}
            )
            if frame_session_id is not None:
                stream_metadata.setdefault("session_id", frame_session_id)
            self._stream_metadata = stream_metadata
            self._stream_session_id = self._metadata_session_id(stream_metadata)
        elif frame_session_id != self._stream_session_id:
            return False
        elif frame_metadata:
            merged_metadata = copy.deepcopy(self._stream_metadata)
            merged_metadata.update(frame_metadata)
            self._stream_metadata = merged_metadata
        self.metadata = copy.deepcopy(self._stream_metadata)
        if self._stream_session_id is not None:
            self.session_id = self._stream_session_id
        try:
            await self.client.send_audio(data)
        except Exception as error:
            self.client.connected = False
            await self._handle_close(-1, str(error))
            return False
        sample_rate = self.input_audio_sample_rate()
        before_ms = int(self._total_audio_bytes * 1000 / (sample_rate * 2))
        self._total_audio_bytes += len(data)
        self._session_audio_bytes += len(data)
        after_ms = int(self._total_audio_bytes * 1000 / (sample_rate * 2))
        self._last_audio_ms = int(
            self._session_audio_bytes * 1000 / (sample_rate * 2)
        )
        self.audio_timeline.add_user_audio(after_ms - before_ms)
        self.sent_buffer_length += len(data)
        if self.first_audio_time is None:
            self.first_audio_time = asyncio.get_running_loop().time()
        async with self._dump_lock:
            if self.audio_dumper is not None:
                await self.audio_dumper.push_bytes(data)
        return True

    async def _flush_buffered_frames_locked(self) -> bool:
        frames = []
        while not self.buffered_frames.empty():
            frames.append(self.buffered_frames.get_nowait())
        self.buffered_frames_size = 0
        if not frames:
            self._buffered_session_id = None
            return True

        owner_session_id = self._frame_session_id(frames[0])
        self._buffered_session_id = owner_session_id
        for index, frame in enumerate(frames):
            if self._frame_session_id(frame) != owner_session_id:
                self._drop_audio_frame_locked(frame)
                continue
            if not await self._send_audio_locked(frame, owner_session_id):
                # Preserve FIFO order when a drain is interrupted by a send
                # failure; the next connection will retry unsent frames for
                # this owner. Frames from another owner are discarded.
                pending = []
                for pending_frame in frames[index:]:
                    if (
                        self._frame_session_id(pending_frame)
                        == owner_session_id
                    ):
                        pending.append(pending_frame)
                    else:
                        self._drop_audio_frame_locked(pending_frame)
                for pending_frame in pending:
                    self.buffered_frames.put_nowait(pending_frame)
                    self.buffered_frames_size += len(pending_frame.get_buf())
                return False
        self._buffered_session_id = None
        return True

    @override
    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        if data.get_name() != "asr_finalize":
            await super().on_data(ten_env, data)
            return
        # Snapshot before waiting for another finalize or a later audio frame.
        metadata = copy.deepcopy(self.metadata or {})
        raw, error = data.get_property_to_json("")
        if not error and raw:
            try:
                payload = json.loads(raw)
                if isinstance(payload, dict) and isinstance(
                    payload.get("metadata"), dict
                ):
                    metadata = copy.deepcopy(payload["metadata"])
            except (TypeError, ValueError) as error:
                self.ten_env.log_warn(f"invalid finalize metadata: {error}")
        async with self._finalize_lock:
            token = self._finalize_context.set(metadata)
            try:
                await super().on_data(ten_env, data)
            finally:
                self._finalize_context.reset(token)

    @override
    async def finalize(self, _session_id: Optional[str]) -> None:
        metadata = self._finalize_context.get()
        if metadata is None:
            async with self._finalize_lock:
                await self._finalize_impl(copy.deepcopy(self.metadata or {}))
            return
        await self._finalize_impl(metadata)

    async def _finalize_impl(self, metadata: dict[str, Any]) -> None:
        finalize_id = self.finalize_id
        waiter: asyncio.Future[None] | None = None
        try:
            # Wait for any handshake already in progress, then send all saved
            # audio before the vendor finalize. Do not hold locks while waiting
            # for its response: the receive loop must remain able to emit it.
            async with self._lifecycle_lock:
                async with self._audio_lock:
                    if not self.is_connected():
                        # A finalize acknowledgement closes the caller's
                        # turn. Never replay its Keep-buffered audio after a
                        # later reconnect.
                        self._discard_buffered_frames_locked()
                        return
                    if not await self._flush_buffered_frames_locked():
                        self._discard_buffered_frames_locked()
                        return
                    waiter = asyncio.get_running_loop().create_future()
                    self._finalize_waiter = waiter
                    await self.client.finalize()
            await asyncio.wait_for(waiter, timeout=10.0)
        except asyncio.TimeoutError:
            self.ten_env.log_warn("Fish Audio finalize timed out")
        except Exception as error:
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message=f"Fish Audio finalize failed: {error}",
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(),
                    code="finalize_error",
                    message=str(error),
                ),
            )
        finally:
            if waiter is not None and not waiter.done():
                waiter.cancel()
            if self._finalize_waiter is waiter:
                self._finalize_waiter = None
            self.last_finalize_time = None
            # The base helper uses mutable audio metadata. Emit the saved
            # request context directly so newer frames cannot change the echo.
            end = Data.create("asr_finalize_end")
            end.set_property_from_json(
                None,
                json.dumps({"finalize_id": finalize_id, "metadata": metadata}),
            )
            await self.ten_env.send_data(end)
            self.ten_env.log_info(
                f"send asr_finalize_end, finalize_id: {finalize_id}",
                category=LOG_CATEGORY_KEY_POINT,
            )

    def _finish_finalize_waiter(self) -> None:
        if self._finalize_waiter and not self._finalize_waiter.done():
            self._finalize_waiter.set_result(None)

    async def _handle_close(
        self,
        code: int,
        message: str,
        *,
        generation: int | None = None,
        close_exception: BaseException | None = None,
    ) -> None:
        if (
            self.stopped
            or self._connection_stopping
            or (generation is not None and generation != self._generation)
        ):
            return
        self._finish_finalize_waiter()
        self.ten_env.log_warn(
            f"Fish Audio WebSocket closed: {code} {message}",
            category=LOG_CATEGORY_VENDOR,
        )
        normal_close = isinstance(close_exception, ConnectionClosedOK) or (
            close_exception is None and code in {0, 1000, 1001}
        )
        severity = (
            ModuleErrorCode.FATAL_ERROR.value
            if self._session_fatal
            else ModuleErrorCode.NON_FATAL_ERROR.value
        )
        if not normal_close and not self._vendor_error_reported:
            self._vendor_error_reported = True
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR, code=severity, message=message
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(), code=str(code), message=message
                ),
            )
        await self.on_disconnected(
            code=0 if normal_close else severity,
            message=message,
            vendor_info=(
                None
                if normal_close
                else ModuleErrorVendorInfo(
                    vendor=self.vendor(), code=str(code), message=message
                )
            ),
        )
        # Every vendor close is recoverable with backoff, including normal
        # session completion and newly introduced gateway codes.
        reconnectable = True
        if reconnectable and not self._session_fatal and not self.stopped:
            self._schedule_reconnect()

    def _schedule_reconnect(self) -> None:
        if (
            self.stopped
            or self._connection_stopping
            or self._session_fatal
            or self.config is None
        ):
            return
        if self._reconnect_task is None or self._reconnect_task.done():
            self._reconnect_task = asyncio.create_task(
                self._reconnect_after_disconnect()
            )

    async def _reconnect_after_disconnect(self) -> None:
        delay = 0.5
        try:
            while not self.stopped and not self._session_fatal:
                await asyncio.sleep(delay)
                if self.stopped or self.is_connected():
                    return
                await self._ensure_connection()
                if self.is_connected():
                    return
                delay = min(delay * 2, 30.0)
        finally:
            if self._reconnect_task is asyncio.current_task():
                self._reconnect_task = None

    async def _handle_message(
        self, event: dict[str, Any], *, generation: int | None = None
    ) -> None:
        if self.stopped or (
            generation is not None and generation != self._generation
        ):
            return
        event_type = event.get("type")
        if event_type == "session_started":
            self._last_draft.clear()
            self._turn_start_ms.clear()
            self._user_turns.clear()
            self._last_audio_ms = 0
            self._session_audio_bytes = 0
            self._session_origin_ms = (
                self.audio_timeline.get_total_user_audio_duration()
            )
            delay = int(time.time() * 1000) - self._connect_started_ms
            await self.send_connect_delay_metrics(delay)
            if self.stopped or (
                generation is not None and generation != self._generation
            ):
                return
            await self.on_connected()
            async with self._audio_lock:
                self._stream_session_id = None
                self._stream_metadata = None
                await self._flush_buffered_frames_locked()
                self._opening = False
            return
        if event_type == "speech_start":
            turn_index = int(event.get("turn_index", 0))
            self._turn_start_ms[turn_index] = int(event.get("audio_ms", 0))
            return
        if event_type == "user_turn_taken":
            turn_index = int(event.get("turn_index", 0))
            self._user_turns.add(turn_index)
            draft = self._last_draft.get(turn_index)
            if draft:
                await self._send_draft(draft, generation=generation)
            return
        if event_type == "draft":
            turn_index = int(event.get("turn_index", 0))
            self._last_draft[turn_index] = copy.deepcopy(event)
            if (
                self.config is not None and self.config.turn_mode != "auto"
            ) or turn_index in self._user_turns:
                await self._send_draft(event, generation=generation)
            return
        if event_type == "eager_end_of_turn":
            turn_index = int(event.get("turn_index", 0))
            if (
                self.config is not None
                and self.config.turn_mode == "auto"
                and turn_index not in self._user_turns
            ):
                return
            text = str(event.get("text", ""))
            if text:
                await self._send_result(
                    text=text,
                    final=False,
                    turn_index=turn_index,
                    audio_end_ms=int(
                        event.get("audio_ms", self._last_audio_ms)
                    ),
                    language_code=event.get("language_code"),
                    metadata={
                        "turn_index": turn_index,
                        "eager_end_of_turn": True,
                    },
                    generation=generation,
                )
            return
        if event_type == "turn_resumed":
            # The preceding eager result was only a prediction.  A later
            # draft or the committed event carries the authoritative text.
            return
        if event_type == "error":
            reconnect_error = event.get("error_type") in {
                "max_duration_exceeded",
                "idle_timeout",
                "internal",
                "engine_error",
            }
            if not event.get("recoverable", False) and not reconnect_error:
                self._session_fatal = True
            self._vendor_error_reported = (
                not event.get("recoverable", False)
                or event.get("error_type") == "server_restarting"
            )
            await self._handle_vendor_error(
                event, reconnect_error, generation=generation
            )
            return
        if event_type == "committed":
            turn_index = int(event.get("turn_index", 0))
            text = str(event.get("text", ""))
            # Fish documents missing `user_turn` as compatible with true.
            user_turn = bool(event.get("user_turn", True))
            if "audio_start_ms" in event:
                self._turn_start_ms[turn_index] = int(event["audio_start_ms"])
            if user_turn and text:
                await self._send_result(
                    text=text,
                    final=True,
                    turn_index=turn_index,
                    audio_end_ms=int(
                        event.get("audio_end_ms", self._last_audio_ms)
                    ),
                    language_code=event.get("language_code"),
                    words=event.get("words"),
                    metadata={
                        "turn_index": turn_index,
                        "user_turn": user_turn,
                        "turn_kind": event.get("turn_kind", "user"),
                        "reason": event.get("reason", ""),
                        "degraded": bool(event.get("degraded", False)),
                    },
                    generation=generation,
                )
            if generation is not None and generation != self._generation:
                return
            self._last_draft.pop(turn_index, None)
            self._user_turns.discard(turn_index)
            self._turn_start_ms.pop(turn_index, None)
            if event.get("reason") == "finalize":
                self._finish_finalize_waiter()
            return
        if event_type == "session_done":
            self._finish_finalize_waiter()

    async def _send_draft(
        self, event: dict[str, Any], *, generation: int | None = None
    ) -> None:
        text = str(event.get("text", ""))
        if not text:
            return
        turn_index = int(event.get("turn_index", 0))
        await self._send_result(
            text=text,
            final=False,
            turn_index=turn_index,
            audio_end_ms=int(event.get("audio_ms", self._last_audio_ms)),
            language_code=event.get("language_code"),
            metadata={
                "turn_index": turn_index,
                "settled_text": event.get("settled_text", ""),
            },
            generation=generation,
        )

    async def _handle_vendor_error(
        self,
        event: dict[str, Any],
        reconnect_error: bool = False,
        *,
        generation: int | None = None,
    ) -> None:
        if generation is not None and generation != self._generation:
            return
        code = str(event.get("error_type", event.get("code", "unknown")))
        message = str(event.get("message", "Fish Audio realtime ASR error"))
        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=(
                    ModuleErrorCode.NON_FATAL_ERROR.value
                    if event.get("recoverable", False) or reconnect_error
                    else ModuleErrorCode.FATAL_ERROR.value
                ),
                message=message,
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(), code=code, message=message
            ),
        )

    async def _send_result(
        self,
        *,
        text: str,
        final: bool,
        turn_index: int,
        audio_end_ms: int,
        language_code: str | None,
        words: list[dict[str, Any]] | None = None,
        metadata: dict[str, Any] | None = None,
        generation: int | None = None,
    ) -> None:
        if generation is not None and generation != self._generation:
            return
        async with self._audio_lock:
            if generation is not None and generation != self._generation:
                return
            start_ms = max(0, self._turn_start_ms.get(turn_index, 0))
            word_results = None
            if words:
                word_results = []
                for word in words:
                    word_start = max(0, int(word.get("start_ms", start_ms)))
                    word_end = int(word.get("end_ms", word_start))
                    word_results.append(
                        ASRWord(
                            word=str(word.get("text", "")),
                            start_ms=self._session_origin_ms + word_start,
                            duration_ms=max(0, word_end - word_start),
                            stable=final,
                        )
                    )
            # Results can arrive after later frames have been sent. The
            # stream owner snapshot is stable for the lifetime of this
            # vendor session, unlike the base class's latest-frame metadata.
            previous_metadata = self.metadata
            source_metadata = (
                self._stream_metadata
                if self._stream_metadata is not None
                else self.metadata
            )
            result_metadata = copy.deepcopy(source_metadata or {})
            asr_info = result_metadata.get("asr_info")
            if not isinstance(asr_info, dict):
                asr_info = {}
            asr_info.update(copy.deepcopy(metadata or {}))
            asr_info["vendor"] = self.vendor()
            result_metadata["asr_info"] = asr_info
            # The base shallow-merges self.metadata over result metadata.
            # Use the merged copy for this send, under the same lock used by
            # audio metadata updates, then restore the original snapshot.
            self.metadata = result_metadata
            try:
                await self.send_asr_result(
                    ASRResult(
                        text=text,
                        final=final,
                        start_ms=self._session_origin_ms + start_ms,
                        duration_ms=max(0, audio_end_ms - start_ms),
                        language=self._language_locale(language_code),
                        words=word_results,
                        metadata=result_metadata,
                    )
                )
            finally:
                self.metadata = previous_metadata

    @staticmethod
    def _language_locale(language_code: str | None) -> str:
        if not language_code:
            return ""
        normalized = language_code.replace("_", "-")
        return LANGUAGE_LOCALES.get(normalized.lower(), normalized)


# Short alias used by a few TEN extensions and integration harnesses.
FishASRExtension = FishAudioASRExtension
