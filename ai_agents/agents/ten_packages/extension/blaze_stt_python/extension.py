#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import array
import asyncio
import contextvars
import copy
import json
import os
import time
from functools import partial
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
    LOG_CATEGORY_TRANSCRIPTS,
    LOG_CATEGORY_VENDOR,
)
from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
)
from ten_runtime import AsyncTenEnv, AudioFrame, Data

from .config import BlazeSTTConfig
from .const import (
    AUDIO_BUFFER_BYTE_LIMIT,
    DUMP_FILE_NAME,
    FINAL_COVERAGE_TOLERANCE_MS,
    FINALIZE_SILENCE_STEP_MS,
    MODULE_NAME_ASR,
    RECONNECT_BASE_DELAY_SECONDS,
    RECONNECT_MAX_DELAY_SECONDS,
    VOICED_MEAN_ABS_THRESHOLD,
    WS_CLOSE_POLICY_VIOLATION,
)
from .recognition import BlazeSTTAuthError, BlazeSTTRecognition

DATA_IN_ASR_FINALIZE = "asr_finalize"
DATA_OUT_ASR_FINALIZE_END = "asr_finalize_end"


def _now_ms() -> int:
    return int(time.time() * 1000)


def _is_voiced(pcm: bytes) -> bool:
    """True when a PCM16 frame carries more than background-level energy."""
    samples = array.array("h")
    samples.frombytes(pcm[: len(pcm) - len(pcm) % 2])
    if not samples:
        return False
    mean_abs = sum(abs(sample) for sample in samples) / len(samples)
    return mean_abs >= VOICED_MEAN_ABS_THRESHOLD


class BlazeSTTExtension(AsyncASRBaseExtension):
    """Blaze realtime STT over one long-lived WebSocket.

    Connection ownership:
      - Every connection attempt goes through public start_connection(), so
        the base class reports CONNECTING; the handshake reports CONNECTED
        and every failure or close reports DISCONNECTED.
      - Each socket gets a generation number. Callbacks from a replaced
        socket are ignored.
      - Recovery runs on one background task with capped exponential
        backoff, never inside the socket's receive loop.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.config: BlazeSTTConfig | None = None
        self.client: BlazeSTTRecognition | None = None
        self.audio_dumper: Dumper | None = None

        # Lock order: lifecycle -> audio. Neither is held while waiting
        # for a vendor result.
        self._lifecycle_lock = asyncio.Lock()
        self._audio_lock = asyncio.Lock()
        self._finalize_lock = asyncio.Lock()
        self._finalize_context: contextvars.ContextVar[
            dict[str, Any] | None
        ] = contextvars.ContextVar("blaze_finalize_context", default=None)

        self._generation = 0
        self._opening = False
        self._connection_stopping = False
        self._session_fatal = False
        self._reconnect_task: asyncio.Task[None] | None = None
        self._connect_started_ms = 0

        # Audio timeline: Blaze timestamps restart with every socket.
        self.sent_user_audio_duration_ms_before_last_reset = 0
        self._total_user_bytes = 0
        # Per-socket stream position in ms (user audio plus injected
        # silence), the same clock as Blaze's start_ms / end_ms.
        self._socket_bytes = 0
        # Finalization watermark: the end of the latest final, and the end
        # of the latest frame that carried speech energy. Speech after the
        # watermark is still waiting for its final.
        self._last_final_end_ms = 0
        self._last_voiced_end_ms: int | None = None
        # Latest voiced end among frames sent after a final arrived. These
        # are new speech, so the end-time tolerance never covers them.
        self._voiced_end_after_final_ms: int | None = None
        self._final_seen = False
        self._partial_since_final = False
        self._final_event = asyncio.Event()

        # Session ownership. The keep-buffer only ever holds one session's
        # audio, and each socket records which session's metadata owns
        # each span of its stream clock, so a late result is labelled with
        # the session whose audio produced it.
        self._buffered_session_id: str | None = None
        self._stream_owners: list[tuple[int, dict[str, Any]]] = []

    @override
    def vendor(self) -> str:
        return "blaze"

    @override
    def vendor_metadata(self) -> dict[str, Any]:
        if self.config is None:
            return {}
        params = self.config.params or {}
        fields = {
            "key": self.config.api_key,
            "url": params.get("base_url"),
            "model": params.get("model"),
            "language": params.get("language"),
        }
        return {k: v for k, v in fields.items() if v}

    # Lifecycle

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        config_json, _ = await ten_env.get_property_to_json("")
        try:
            self.config = BlazeSTTConfig.model_validate_json(config_json)
            self.config.apply_defaults()
            self.config.validate_config()
            ten_env.log_info(
                f"config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            if self.config.dump:
                self.audio_dumper = Dumper(
                    os.path.join(self.config.dump_path, DUMP_FILE_NAME)
                )
                await self.audio_dumper.start()
        except Exception as e:  # pylint: disable=broad-exception-caught
            ten_env.log_error(f"Invalid Blaze STT config: {e}")
            # Latch the failure: report it once and never auto-connect
            # with a fallback config.
            self._session_fatal = True
            self.auto_connect = False
            self.config = None
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        try:
            self.stopped = True
            await self.stop_connection()
            await super().on_deinit(ten_env)
        finally:
            if self.audio_dumper:
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
            self._connect_started_ms = _now_ms()
            generation = self._generation
            client = BlazeSTTRecognition(
                config=self.config,
                ten_env=self.ten_env,
                on_message=partial(self._handle_message, generation=generation),
                on_close=partial(self._handle_close, generation=generation),
            )
            self.client = client
            try:
                await client.connect()
            except asyncio.CancelledError:
                self._opening = False
                await self._close_client()
                raise
            except Exception as e:  # pylint: disable=broad-exception-caught
                self._opening = False
                await self._report_connect_failure(e)
                await self._close_client()
                self._schedule_reconnect()
                return

            await self._on_handshake_ready()

    async def _on_handshake_ready(self) -> None:
        self.ten_env.log_info(
            "vendor_status_changed: ready", category=LOG_CATEGORY_VENDOR
        )
        await self.send_connect_delay_metrics(
            _now_ms() - self._connect_started_ms
        )
        # Blaze timestamps restart at zero on every socket.
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.audio_timeline.get_total_user_audio_duration()
        )
        self.audio_timeline.reset()
        self._socket_bytes = 0
        self._last_final_end_ms = 0
        self._last_voiced_end_ms = None
        self._voiced_end_after_final_ms = None
        self._final_seen = False
        self._partial_since_final = False
        self._stream_owners = []
        await self.on_connected()
        async with self._audio_lock:
            # Frames that arrive during the drain are buffered because
            # is_connected() is False while opening; the drain picks them
            # up, so audio kept during an outage is sent even if no new
            # frame arrives after recovery.
            await self._flush_buffered_frames_locked()
            self._opening = False

    async def _report_connect_failure(self, error: Exception) -> None:
        fatal = isinstance(error, BlazeSTTAuthError)
        if fatal:
            self._session_fatal = True
        severity = (
            ModuleErrorCode.FATAL_ERROR.value
            if fatal
            else ModuleErrorCode.NON_FATAL_ERROR.value
        )
        vendor_info = ModuleErrorVendorInfo(
            vendor=self.vendor(),
            code=(
                str(error.status_code)
                if isinstance(error, BlazeSTTAuthError)
                else type(error).__name__
            ),
            message=str(error),
        )
        self.ten_env.log_error(
            f"vendor_error: Blaze STT connect failed: {error}",
            category=LOG_CATEGORY_VENDOR,
        )
        if not self.stopped and not self._connection_stopping:
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR, code=severity, message=str(error)
                ),
                vendor_info,
            )
        await self.on_disconnected(
            code=severity, message=str(error), vendor_info=vendor_info
        )

    @override
    async def stop_connection(self) -> None:
        self._connection_stopping = True
        try:
            # Abort a handshake in progress rather than waiting for its
            # deadline while it holds the lifecycle lock.
            opening_client = self.client
            if opening_client is not None and self._opening:
                await opening_client.close()
            reconnect_task = self._reconnect_task
            self._reconnect_task = None
            if (
                reconnect_task is not None
                and reconnect_task is not asyncio.current_task()
            ):
                reconnect_task.cancel()
                await asyncio.gather(reconnect_task, return_exceptions=True)
            async with self._lifecycle_lock:
                await self._close_client()
        finally:
            self._connection_stopping = False

    async def _close_client(self) -> None:
        async with self._audio_lock:
            client = self.client
            self.client = None
            # Invalidate callbacks from the socket being closed.
            self._generation += 1
            self._final_event.set()
        if client is not None:
            await client.close()

    @override
    def is_connected(self) -> bool:
        return bool(
            not self._opening
            and not self._connection_stopping
            and not self._session_fatal
            and not self.stopped
            and self.client is not None
            and self.client.connected
        )

    # Reconnect

    async def _handle_close(
        self,
        code: int,
        message: str,
        close_exception: BaseException | None = None,
        *,
        generation: int,
    ) -> None:
        if (
            self.stopped
            or self._connection_stopping
            or generation != self._generation
        ):
            return
        # Handle each socket's close once (a failed send and the receive
        # loop can both report it).
        self._generation += 1
        # The socket is unusable now even if its receive loop has not
        # noticed yet: stop sending to it so audio is buffered instead and
        # recovery does not mistake it for a live connection.
        if self.client is not None:
            self.client.connected = False
        self._final_event.set()
        self.ten_env.log_info(
            f"vendor_status_changed: closed {code} {message}",
            category=LOG_CATEGORY_VENDOR,
        )
        if code == WS_CLOSE_POLICY_VIOLATION:
            self._session_fatal = True
        normal_close = isinstance(close_exception, ConnectionClosedOK) or (
            close_exception is None and code in (1000, 1001)
        )
        severity = (
            ModuleErrorCode.FATAL_ERROR.value
            if self._session_fatal
            else ModuleErrorCode.NON_FATAL_ERROR.value
        )
        vendor_info = None
        if not normal_close:
            vendor_info = ModuleErrorVendorInfo(
                vendor=self.vendor(), code=str(code), message=message
            )
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR, code=severity, message=message
                ),
                vendor_info,
            )
        await self.on_disconnected(
            code=0 if normal_close else severity,
            message=message,
            vendor_info=vendor_info,
        )
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
        delay = RECONNECT_BASE_DELAY_SECONDS
        attempt = 0
        try:
            while not self.stopped and not self._session_fatal:
                await asyncio.sleep(delay)
                if self.stopped or self.is_connected():
                    return
                attempt += 1
                self.ten_env.log_info(
                    f"vendor_status: reconnect attempt {attempt}",
                    category=LOG_CATEGORY_VENDOR,
                )
                # Public path: the base class reports CONNECTING.
                await self._ensure_connection()
                if self.is_connected():
                    return
                delay = min(delay * 2, RECONNECT_MAX_DELAY_SECONDS)
        finally:
            if self._reconnect_task is asyncio.current_task():
                self._reconnect_task = None

    # Audio

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=AUDIO_BUFFER_BYTE_LIMIT)

    @override
    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else 16000

    @override
    async def send_audio(
        self, frame: AudioFrame, _session_id: str | None
    ) -> bool:
        async with self._audio_lock:
            if not self.is_connected():
                # Returning False lets the base class buffer the frame.
                return False
            return await self._send_audio_locked(frame)

    @override
    async def _handle_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        """Send or keep one frame, serialized with the handshake drain.

        Replaces the base flow so kept audio is drained in order with
        per-frame metadata, and the keep-buffer never mixes sessions.
        """
        del ten_env  # logging goes through self.ten_env
        if not audio_frame.get_buf():
            return
        async with self._audio_lock:
            if (
                self.is_connected()
                and await self._flush_buffered_frames_locked()
            ):
                metadata = self._frame_metadata(audio_frame)
                if metadata:
                    self.metadata = metadata
                    self.session_id = metadata.get(
                        "session_id", self.session_id
                    )
                if await self._send_audio_locked(audio_frame):
                    self.sent_buffer_length += len(audio_frame.get_buf())
                    if self.first_audio_time is None:
                        self.first_audio_time = (
                            asyncio.get_running_loop().time()
                        )
                    return
            self._buffer_frame_locked(audio_frame)

    def _buffer_frame_locked(self, frame: AudioFrame) -> None:
        """Keep a frame during an outage, bounded and single-session."""
        metadata = self._frame_metadata(frame)
        session_id = metadata.get("session_id", self.session_id)
        if (
            not self.buffered_frames.empty()
            and session_id != self._buffered_session_id
        ):
            # One Blaze stream serves the drained audio, so kept audio
            # from an older session must not be replayed in front of a
            # newer session's audio. The newer session supersedes it.
            self.ten_env.log_info(
                "dropping kept audio of a previous session",
                category=LOG_CATEGORY_KEY_POINT,
            )
            self._discard_buffered_frames_locked()
        self._buffered_session_id = session_id
        frame_size = len(frame.get_buf())
        while (
            not self.buffered_frames.empty()
            and self.buffered_frames_size + frame_size > AUDIO_BUFFER_BYTE_LIMIT
        ):
            dropped = self.buffered_frames.get_nowait()
            self.buffered_frames_size -= len(dropped.get_buf())
            self.dropped_audio_frames += 1
        self.buffered_frames.put_nowait(frame)
        self.buffered_frames_size += frame_size

    async def _send_audio_locked(self, frame: AudioFrame) -> bool:
        client = self.client
        if client is None or not client.connected:
            return False
        owner = self._frame_metadata(frame) or copy.deepcopy(
            self.metadata or {}
        )
        buf = frame.lock_buf()
        try:
            audio_data = bytes(buf)
        finally:
            frame.unlock_buf(buf)
        if not audio_data:
            return True
        try:
            await client.send_audio(audio_data)
        except Exception as e:  # pylint: disable=broad-exception-caught
            self.ten_env.log_error(
                f"vendor_error: Blaze STT audio send failed: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            await self._handle_close(
                -1, f"audio send failed: {e}", e, generation=self._generation
            )
            return False
        # Count audio only after Blaze has it.
        self._record_stream_owner(owner)
        self._add_user_audio(len(audio_data))
        stream_end_ms = self._advance_socket(len(audio_data))
        if _is_voiced(audio_data):
            self._last_voiced_end_ms = stream_end_ms
            if self._final_seen:
                self._voiced_end_after_final_ms = max(
                    self._voiced_end_after_final_ms or 0, stream_end_ms
                )
        if self.audio_dumper:
            await self.audio_dumper.push_bytes(audio_data)
        return True

    def _advance_socket(self, byte_count: int) -> int:
        """Advance this socket's stream clock; return its new end in ms."""
        self._socket_bytes += byte_count
        return self._socket_bytes * 1000 // (self.input_audio_sample_rate() * 2)

    def _record_stream_owner(self, owner: dict[str, Any]) -> None:
        """Note which metadata owns the stream from its current position."""
        if self._stream_owners and self._stream_owners[-1][1] == owner:
            return
        position = self._advance_socket(0)
        # Spans that end before the latest final can no longer be the
        # start of a result; keep the one that covers it.
        while (
            len(self._stream_owners) > 1
            and self._stream_owners[1][0] <= self._last_final_end_ms
        ):
            self._stream_owners.pop(0)
        self._stream_owners.append((position, copy.deepcopy(owner)))

    def _owner_at(self, vendor_ms: int) -> dict[str, Any] | None:
        owner = None
        for start_ms, metadata in self._stream_owners:
            if start_ms > vendor_ms:
                break
            owner = metadata
        return owner

    def _has_pending_speech(self) -> bool:
        """Speech was sent that no final has covered yet.

        Compared on the vendor stream clock, so a late final for an
        earlier utterance does not cover speech sent after it. The
        tolerance for Blaze's end-time rounding applies only to speech
        sent before the latest final arrived; anything voiced after it is
        new speech that still needs its own final.
        """
        if self._partial_since_final:
            return True
        if self._last_voiced_end_ms is None:
            return False
        if not self._final_seen:
            return True
        if (
            self._voiced_end_after_final_ms is not None
            and self._voiced_end_after_final_ms > self._last_final_end_ms
        ):
            return True
        return (
            self._last_voiced_end_ms
            > self._last_final_end_ms + FINAL_COVERAGE_TOLERANCE_MS
        )

    def _add_user_audio(self, byte_count: int) -> None:
        bytes_per_second = self.input_audio_sample_rate() * 2
        before_ms = self._total_user_bytes * 1000 // bytes_per_second
        self._total_user_bytes += byte_count
        after_ms = self._total_user_bytes * 1000 // bytes_per_second
        self.audio_timeline.add_user_audio(after_ms - before_ms)

    async def _flush_buffered_frames_locked(self) -> bool:
        """Send frames kept during an outage, each with its own metadata."""
        while not self.buffered_frames.empty():
            frame = self.buffered_frames.get_nowait()
            self.buffered_frames_size = max(
                0, self.buffered_frames_size - len(frame.get_buf())
            )
            metadata = self._frame_metadata(frame)
            if metadata:
                self.metadata = metadata
                self.session_id = metadata.get("session_id", self.session_id)
            if not await self._send_audio_locked(frame):
                # Keep the frame, in order, ahead of everything still queued.
                pending = [frame]
                while not self.buffered_frames.empty():
                    pending.append(self.buffered_frames.get_nowait())
                for kept in pending:
                    self.buffered_frames.put_nowait(kept)
                self.buffered_frames_size = sum(
                    len(kept.get_buf()) for kept in pending
                )
                return False
            self.sent_buffer_length += len(frame.get_buf())
            if self.first_audio_time is None:
                self.first_audio_time = asyncio.get_running_loop().time()
        self.buffered_frames_size = 0
        self._buffered_session_id = None
        return True

    def _frame_metadata(self, frame: AudioFrame) -> dict[str, Any]:
        try:
            raw, error = frame.get_property_to_json("metadata")
        except Exception:  # pylint: disable=broad-exception-caught
            return {}
        if error or not raw:
            return {}
        try:
            metadata = json.loads(raw)
        except (TypeError, ValueError):
            return {}
        return copy.deepcopy(metadata) if isinstance(metadata, dict) else {}

    def _discard_buffered_frames_locked(self) -> None:
        while not self.buffered_frames.empty():
            self.buffered_frames.get_nowait()
            self.dropped_audio_frames += 1
        self.buffered_frames_size = 0
        self._buffered_session_id = None

    # Finalize

    @override
    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        if data.get_name() != DATA_IN_ASR_FINALIZE:
            await super().on_data(ten_env, data)
            return
        # Snapshot this request's metadata before waiting behind another
        # finalize; later audio frames must not change the echo.
        metadata = copy.deepcopy(self.metadata or {})
        raw, error = data.get_property_to_json("")
        if not error and raw:
            try:
                payload = json.loads(raw)
                if isinstance(payload, dict) and isinstance(
                    payload.get("metadata"), dict
                ):
                    metadata = copy.deepcopy(payload["metadata"])
            except (TypeError, ValueError) as e:
                ten_env.log_warn(f"invalid finalize metadata: {e}")
        async with self._finalize_lock:
            token = self._finalize_context.set(metadata)
            try:
                # The base class stores finalize_id, then calls finalize().
                await super().on_data(ten_env, data)
            finally:
                self._finalize_context.reset(token)

    @override
    async def finalize(self, _session_id: str | None) -> None:
        metadata = self._finalize_context.get()
        if metadata is None:
            async with self._finalize_lock:
                await self._finalize_impl(copy.deepcopy(self.metadata or {}))
            return
        await self._finalize_impl(metadata)

    async def _finalize_impl(self, metadata: dict[str, Any]) -> None:
        assert self.config is not None or self._session_fatal
        finalize_id = self.finalize_id
        started_ms = _now_ms()
        wait_for_final = False
        try:
            # Only the audio lock: a handshake in progress (which holds the
            # lifecycle lock) must not delay asr_finalize_end. While opening,
            # is_connected() is False and finalize ends immediately.
            async with self._audio_lock:
                if not self.is_connected():
                    # This turn is closed now; never replay its kept
                    # audio after a later reconnect.
                    self._discard_buffered_frames_locked()
                    return
                if not await self._flush_buffered_frames_locked():
                    self._discard_buffered_frames_locked()
                    return
                if not self._has_pending_speech():
                    # Every utterance sent so far already has its final.
                    return
                assert self.config is not None
                self._final_event.clear()
                # Blaze has no flush message: trailing silence lets
                # its endpointing close the utterance.
                await self._send_silence_locked(self.config.finalize_silence_ms)
                wait_for_final = True
            if wait_for_final:
                assert self.config is not None
                if not await self._wait_for_final_keeping_stream_alive(
                    self.config.finalize_timeout_ms / 1000
                ):
                    self.ten_env.log_warn(
                        "asr_finalize: no final result before timeout",
                        category=LOG_CATEGORY_KEY_POINT,
                    )
        except Exception as e:  # pylint: disable=broad-exception-caught
            self.ten_env.log_warn(
                f"asr_finalize: failed: {e}", category=LOG_CATEGORY_KEY_POINT
            )
        finally:
            self.last_finalize_time = None
            await self._send_finalize_end(finalize_id, metadata, started_ms)

    async def _wait_for_final_keeping_stream_alive(
        self, timeout_seconds: float
    ) -> bool:
        """Wait for the final; True if it arrived before the timeout.

        Blaze's endpointing only advances while audio arrives. If the input
        stream goes idle, keep sending real-time silence so the final is not
        held back; while user audio still flows, add nothing.
        """
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout_seconds
        step_seconds = FINALIZE_SILENCE_STEP_MS / 1000
        last_socket_bytes = self._socket_bytes
        while True:
            remaining = deadline - loop.time()
            if remaining <= 0:
                return False
            try:
                await asyncio.wait_for(
                    self._final_event.wait(), min(step_seconds, remaining)
                )
                return self._final_event.is_set() and self.is_connected()
            except asyncio.TimeoutError:
                pass
            async with self._audio_lock:
                if not self.is_connected():
                    return False
                if self._socket_bytes == last_socket_bytes:
                    await self._send_silence_locked(FINALIZE_SILENCE_STEP_MS)
                last_socket_bytes = self._socket_bytes

    async def _send_silence_locked(self, duration_ms: int) -> None:
        client = self.client
        if client is None or not client.connected:
            raise ConnectionError("Blaze STT WebSocket is not ready")
        byte_count = int(
            self.input_audio_sample_rate() * 2 * duration_ms / 1000
        )
        await client.send_audio(b"\x00" * byte_count)
        # Silence is not user audio and is not dumped.
        self.audio_timeline.add_silence_audio(duration_ms)
        self._advance_socket(byte_count)

    async def _send_finalize_end(
        self, finalize_id: str | None, metadata: dict[str, Any], started_ms: int
    ) -> None:
        # Echo this request's own id and metadata, not the base class's
        # mutable latest-frame values.
        end = Data.create(DATA_OUT_ASR_FINALIZE_END)
        end.set_property_from_json(
            None, json.dumps({"finalize_id": finalize_id, "metadata": metadata})
        )
        await self.ten_env.send_data(end)
        self.ten_env.log_info(
            f"send asr_finalize_end, finalize_id: {finalize_id}, "
            f"latency_ms: {_now_ms() - started_ms}",
            category=LOG_CATEGORY_KEY_POINT,
        )

    # Vendor messages

    async def _handle_message(
        self, event: dict[str, Any], *, generation: int
    ) -> None:
        if self.stopped or generation != self._generation:
            return
        event_type = event.get("type", "")
        if event_type == "partial":
            await self._on_partial(event)
        elif event_type == "final":
            self.ten_env.log_debug(
                f"vendor_result: final: {event}",
                category=LOG_CATEGORY_TRANSCRIPTS,
            )
            await self._on_final(event)
        elif event_type == "error":
            await self._on_vendor_error(
                str(event.get("text") or "Unknown Blaze STT error"),
                str(event.get("code") or "unknown"),
                fatal=False,
            )
        elif event_type == "failed-authentication":
            self._session_fatal = True
            await self._on_vendor_error(
                str(event.get("message") or "authentication failed"),
                "401",
                fatal=True,
            )

    async def _on_partial(self, event: dict[str, Any]) -> None:
        text = str(event.get("text") or "").strip()
        if not text:
            return
        self._partial_since_final = True
        await self._emit_asr_result(
            text,
            final=False,
            start_ms=self._actual_ms(self._last_final_end_ms),
            duration_ms=0,
            owner=self._owner_at(self._last_final_end_ms),
        )

    async def _on_final(self, event: dict[str, Any]) -> None:
        text = str(event.get("text") or "").strip()
        start = int(event.get("start_ms") or self._last_final_end_ms)
        if event.get("end_ms") is not None:
            end = int(event["end_ms"])
        else:
            # Without an end, the final covers everything sent so far.
            end = max(start, self._advance_socket(0))
        self._last_final_end_ms = max(end, self._last_final_end_ms)
        self._final_seen = True
        self._partial_since_final = False
        if (
            self._voiced_end_after_final_ms is not None
            and self._last_final_end_ms >= self._voiced_end_after_final_ms
        ):
            self._voiced_end_after_final_ms = None
        if text:
            actual_start = self._actual_ms(start)
            await self._emit_asr_result(
                text,
                final=True,
                start_ms=actual_start,
                duration_ms=self._actual_ms(end) - actual_start,
                owner=self._owner_at(start),
            )
        # Release a waiting finalize after the result is out, but only once
        # the final covers all speech sent: a late final for an earlier
        # utterance must not acknowledge newer speech.
        if not self._has_pending_speech():
            self._final_event.set()

    async def _on_vendor_error(
        self, message: str, code: str, *, fatal: bool
    ) -> None:
        self.ten_env.log_error(
            f"vendor_error: code: {code}, reason: {message}",
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
                vendor=self.vendor(), code=code, message=message
            ),
        )

    # Results

    def _actual_ms(self, vendor_ms: int) -> int:
        """Map a vendor stream timestamp to the session's user-audio time."""
        return int(
            self.audio_timeline.get_audio_duration_before_time(vendor_ms)
            + self.sent_user_audio_duration_ms_before_last_reset
        )

    def _build_metadata(
        self, owner: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        source = owner if owner is not None else self.metadata
        base_metadata = copy.deepcopy(source) if source is not None else {}
        session_id = base_metadata.pop("session_id", None)
        metadata: dict[str, Any] = {}
        if session_id is not None:
            metadata["session_id"] = session_id
        metadata["asr_info"] = {
            **base_metadata,
            "vendor": self.vendor(),
            "locked": False,
        }
        return metadata

    async def _emit_asr_result(
        self,
        text: str,
        final: bool,
        start_ms: int,
        duration_ms: int,
        owner: dict[str, Any] | None = None,
    ) -> None:
        assert self.config is not None
        result = ASRResult(
            text=text,
            final=final,
            start_ms=max(0, start_ms),
            duration_ms=max(0, duration_ms),
            language=self.config.asr_result_language,
            words=[],
            metadata=self._build_metadata(owner),
        )
        if owner is None:
            await self.send_asr_result(result)
            return
        # send_asr_result() merges self.metadata (the latest frame's) into
        # the result, which would relabel a late result with a newer
        # session. Make the owner current for that merge.
        latest = self.metadata
        swapped = copy.deepcopy(owner)
        self.metadata = swapped
        try:
            await self.send_asr_result(result)
        finally:
            if self.metadata is swapped:
                self.metadata = latest
