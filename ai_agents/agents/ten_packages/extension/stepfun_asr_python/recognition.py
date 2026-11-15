from __future__ import annotations

import asyncio
import base64
import json
import uuid
from collections import deque
from contextlib import suppress
from dataclasses import dataclass, field
from typing import Any

import websockets
from websockets.exceptions import (
    ConnectionClosedError,
    ConnectionClosedOK,
    WebSocketException,
)

from ten_runtime import AsyncTenEnv

from .config import StepFunASRConfig


def _transcription_config(config: StepFunASRConfig) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "model": config.model,
        "language": config.language,
        "full_rerun_on_commit": config.full_rerun_on_commit,
        "enable_itn": config.enable_itn,
        "enable_timestamp_align": config.enable_timestamp_align,
    }
    if config.prompt is not None:
        payload["prompt"] = config.prompt
    return payload


@dataclass
class TranscriptSegment:
    start: int
    end: int | None = None
    id: str = field(default_factory=lambda: uuid.uuid4().hex)
    # Stable asr_result.id for partial/final pairs on this span.
    result_id: str | None = None
    finalize_id: str | None = None
    finalize_time: float | None = None
    metadata: dict[str, Any] | None = None
    session_id: str | None = None
    timed_out: bool = False
    result: dict[str, Any] | None = None
    published: bool = False
    generation: int = -1
    commit_generation: int = -1
    # Local session-boundary seals must still get a vendor commit under
    # server_vad so consecutive sessions are not one continuous utterance.
    needs_vendor_commit: bool = False

    def can_bind_finalize(self, session_id: str | None) -> bool:
        """Whether an empty/open finalize may claim this sealed span."""
        if self.published or self.finalize_id is not None:
            return False
        if self.session_id is None or session_id is None:
            return True
        return self.session_id == session_id

    def bind_finalize(
        self,
        finalize_id: str,
        finalize_time: float,
        *,
        metadata: dict[str, Any] | None = None,
        session_id: str | None = None,
        timed_out: bool = False,
        force_identity: bool = False,
    ) -> None:
        """Attach finalize identity once.

        Pending claims keep existing session/metadata (write-once). Sealing the
        open span for this finalize may force the finalize's identity.
        """
        if self.finalize_id is not None:
            return
        self.finalize_id = finalize_id
        self.finalize_time = finalize_time
        self.timed_out = timed_out
        if force_identity:
            # Empty finalize metadata must not wipe the originating audio context.
            if metadata:
                self.metadata = dict(metadata)
            if session_id is not None:
                self.session_id = session_id
            if self.session_id is not None and self.metadata is not None:
                if "session_id" not in self.metadata:
                    self.metadata["session_id"] = self.session_id
            return
        if self.metadata is None and metadata:
            self.metadata = dict(metadata)
        if self.session_id is None and session_id is not None:
            self.session_id = session_id


class StepFunASRRecognitionCallback:
    async def on_open(self) -> None:
        pass

    async def on_result(
        self,
        message: dict[str, Any],
        segment: TranscriptSegment,
    ) -> None:
        pass

    async def on_audio_sent(self, total_bytes: int) -> None:
        pass

    async def on_error(
        self,
        message: str,
        error_type: str = "",
        error_code: str = "",
    ) -> None:
        pass

    async def on_close(self) -> None:
        pass


class StepFunASRRecognition:
    """StepFun transport; unsent audio is buffered by the ASR base class."""

    _MAX_COMPLETED_ITEMS = 256

    def __init__(
        self,
        config: StepFunASRConfig,
        ten_env: AsyncTenEnv,
        callback: StepFunASRRecognitionCallback,
    ):
        self.config = config
        self.ten_env = ten_env
        self.callback = callback
        self.websocket: Any = None
        self._receive_task: asyncio.Task[None] | None = None
        self._pump_lock = asyncio.Lock()
        self._session_ready = asyncio.Event()
        self._session_confirmed = False
        self._session_error: str | None = None
        self._normal_close = False
        self._started = False
        self._closing = False
        self._generation = 0
        self._session_offset = 0
        self._tail = 0
        self._interrupted = False
        self._pending: deque[TranscriptSegment] = deque()
        self._current = TranscriptSegment(0)
        self._items: dict[str, TranscriptSegment] = {}
        self._item_starts: dict[str, int] = {}
        self._completed_items: deque[str] = deque()
        self._completed_item_set: set[str] = set()
        self._timed_out_finalizes: deque[str] = deque()
        self._timed_out_finalize_set: set[str] = set()
        self._awaiting_ack: deque[TranscriptSegment] = deque()

    @property
    def bytes_per_second(self) -> int:
        return (
            self.config.sample_rate
            * self.config.channels
            * self.config.bits
            // 8
        )

    @property
    def normal_close(self) -> bool:
        """Whether the latest connection ended with a normal close."""
        return self._normal_close

    def _mark_normal_close(self) -> None:
        self._normal_close = True

    async def start(self) -> None:
        await self.close()
        self._closing = False
        async with self._pump_lock:
            self._session_ready.clear()
            self._session_confirmed = False
            self._session_error = None
            self._normal_close = False
            if self._pending or self._tail > self._current.start:
                self._interrupted = True
            self._generation += 1
            self._session_offset = self._tail
            self._pending.clear()
            self._current = TranscriptSegment(self._tail)
            self._items.clear()
            self._item_starts.clear()
            self._completed_items.clear()
            self._completed_item_set.clear()
            self._awaiting_ack.clear()

        try:
            websocket = await websockets.connect(
                self.config.url,
                additional_headers={
                    "Authorization": f"Bearer {self.config.api_key}",
                },
                open_timeout=self.config.connection_timeout,
                ping_interval=20,
                ping_timeout=20,
                close_timeout=2,
                max_size=None,
            )
            if self._closing:
                with suppress(Exception):
                    await websocket.close()
                return
            self.websocket = websocket
            self._receive_task = asyncio.create_task(self._receive_loop())
            input_config: dict[str, Any] = {
                "format": {
                    "type": "pcm",
                    "codec": "pcm_s16le",
                    "rate": self.config.sample_rate,
                    "bits": self.config.bits,
                    "channel": self.config.channels,
                },
                "transcription": _transcription_config(self.config),
            }
            if self.config.server_vad:
                input_config["turn_detection"] = {
                    "type": "server_vad",
                    "silence_duration_ms": self.config.silence_duration_ms,
                    "threshold": self.config.vad_threshold,
                }
            await self._send_json(
                "session.update",
                session={"audio": {"input": input_config}},
            )
            await asyncio.wait_for(
                self._session_ready.wait(),
                self.config.connection_timeout,
            )
            if self._normal_close:
                self._started = False
                await self.close()
                await self.callback.on_close()
                return
            if not self._session_confirmed:
                raise ConnectionError(
                    self._session_error
                    or "StepFun session closed during handshake"
                )
            self._started = True
            await self.callback.on_open()
            if not self.is_connected():
                if self._normal_close:
                    self._started = False
                    await self.close()
                    await self.callback.on_close()
                    return
                raise ConnectionError(
                    "StepFun session closed while publishing ready"
                )
        except ConnectionClosedOK:
            self._mark_normal_close()
            self._started = False
            await self.close()
            await self.callback.on_close()
            return
        except BaseException:
            await self.close()
            raise

    async def _receive_loop(self) -> None:
        try:
            async for raw in self.websocket:
                if not isinstance(raw, bytes):
                    await self._handle_message(json.loads(raw))
        except ConnectionClosedOK:
            self._mark_normal_close()
        except (ConnectionClosedError, WebSocketException) as exc:
            if not self._closing:
                await self.callback.on_error(str(exc), "connection_closed")
        except Exception as exc:
            if not self._closing:
                await self.callback.on_error(str(exc), "invalid_response")
        else:
            # websockets closes a normal async iterator without raising.
            self._mark_normal_close()
        finally:
            was_started = self._started
            self._started = False
            self._session_ready.set()
            if was_started and not self._closing:
                await self.callback.on_close()

    def _seal_current(self, end: int) -> TranscriptSegment:
        segment = self._current
        segment.end = end
        self._pending.append(segment)
        has_unsealed_suffix = self._tail > end
        self._current = TranscriptSegment(
            end,
            metadata=(
                dict(segment.metadata)
                if has_unsealed_suffix and segment.metadata is not None
                else None
            ),
            session_id=segment.session_id if has_unsealed_suffix else None,
        )
        for item, mapped in list(self._items.items()):
            if mapped is segment and self._item_starts.get(item, -1) >= end:
                self._items[item] = self._current
        return segment

    def _stamp_open_segment(
        self,
        metadata: dict[str, Any] | None,
        session_id: str | None,
    ) -> None:
        """Bind input session metadata to the open audio span."""
        if (
            session_id is not None
            and self._current.session_id is not None
            and session_id != self._current.session_id
            and self._tail > self._current.start
        ):
            # Client session boundary arrived before the vendor sealed the
            # previous turn; keep each span's originating metadata and force a
            # vendor commit even when server_vad owns endpoints.
            sealed = self._seal_current(self._tail)
            sealed.needs_vendor_commit = True
        if self._current.metadata is None and metadata is not None:
            self._current.metadata = dict(metadata)
            self._current.session_id = session_id
        elif self._current.session_id is None and session_id is not None:
            self._current.session_id = session_id

    def _segment_for(self, message: dict[str, Any]) -> TranscriptSegment:
        item = str(message.get("item_id") or "")
        words = message.get("words") or []
        start = (
            self._session_offset
            + round(float(words[0]["start"]) * self.bytes_per_second)
            if words
            else self._item_starts.get(item)
        )
        if item and start is not None:
            self._item_starts.setdefault(item, start)
        segment = self._items.get(item)
        if segment is not None and segment.published:
            return segment
        if (
            segment is not None
            and segment.result is not None
            and start is not None
            and segment.end is not None
            and start >= segment.end
        ):
            segment = None
        if segment is None:
            candidates = list(self._pending)
            if segment is None:
                segment = next(
                    (
                        candidate
                        for candidate in candidates
                        if start is not None
                        and candidate.end is not None
                        and candidate.start <= start < candidate.end
                    ),
                    None,
                )
            if segment is None:
                segment = (
                    next(
                        (
                            candidate
                            for candidate in candidates
                            if not candidate.published
                        ),
                        self._current,
                    )
                    if str(message.get("type", "")).endswith(".completed")
                    else self._current
                )
            if item:
                self._items[item] = segment
        return segment

    async def _handle_message(self, message: dict[str, Any]) -> None:
        kind = str(message.get("type") or "")
        item = str(message.get("item_id") or "")
        if kind == "session.updated":
            self._session_confirmed = True
            self._session_ready.set()
        elif kind == "error":
            error = message.get("error") or {}
            self._session_error = str(
                error.get("message") or "StepFun ASR error"
            )
            # Deliver the structured error before waking the handshake waiter so
            # start()/close() cannot cancel this report mid-flight, and so the
            # fatal latch is owned by on_error rather than a plain ConnectionError.
            await self.callback.on_error(
                self._session_error,
                str(error.get("type") or ""),
                str(error.get("code") or ""),
            )
            if not self._started:
                self._session_ready.set()
        elif (
            kind == "input_audio_buffer.speech_stopped"
            and self.config.server_vad
        ):
            if item in self._completed_item_set:
                return
            end_ms = message.get("audio_end_ms", message.get("audio_start_ms"))
            if end_ms is not None:
                end = self._session_offset + int(
                    float(end_ms) * self.bytes_per_second / 1000
                )
                end = min(end, self._tail)
                boundary_segment = next(
                    (
                        candidate
                        for candidate in self._pending
                        if candidate.end == end and not candidate.published
                    ),
                    None,
                )
                if boundary_segment is not None:
                    if item:
                        self._items[item] = boundary_segment
                    return
                if end > self._current.start:
                    segment = self._seal_current(end)
                    segment.generation = self._generation
                    if item:
                        self._items[item] = segment
        elif kind == "input_audio_buffer.committed":
            if self._awaiting_ack:
                self._items[item] = self._awaiting_ack.popleft()
        elif kind.endswith((".delta", ".completed")):
            if item in self._completed_item_set:
                return
            if kind.endswith(".delta") and not any(
                message.get(field) for field in ("text", "stash", "words")
            ):
                return
            segment = self._segment_for(message)
            message = dict(message)
            message["audio_offset_ms"] = (
                self._session_offset * 1000 / self.bytes_per_second
            )
            if kind.endswith(".delta"):
                if segment.result is None:
                    await self.callback.on_result(message, segment)
                return
            if segment.published:
                self._remember_completed_item(item)
                return
            if segment.end is None:
                words = message.get("words") or []
                if words and words[-1].get("end") is not None:
                    end = self._session_offset + round(
                        float(words[-1]["end"]) * self.bytes_per_second
                    )
                    end -= end % (self.config.bits // 8 * self.config.channels)
                    if (
                        segment is self._current
                        and segment.start < end <= self._tail
                    ):
                        segment = self._seal_current(end)
                        segment.generation = self._generation
            if segment.end is None:
                await self.callback.on_error(
                    "StepFun final has no committed audio boundary",
                    "invalid_response",
                    "missing_commit_boundary",
                )
                return
            self._remember_completed_item(item)
            segment.result = message
            await self.callback.on_result(message, segment)
            segment.published = True
            if segment in self._pending:
                self._pending.remove(segment)
            self._retire_segment_mappings(segment)

    def is_complete(self) -> bool:
        """Whether reconnect has avoided abandoning unconfirmed audio."""
        return not self._interrupted

    def _remember_completed_item(self, item: str) -> None:
        if not item or item in self._completed_item_set:
            return
        self._completed_items.append(item)
        self._completed_item_set.add(item)
        while len(self._completed_items) > self._MAX_COMPLETED_ITEMS:
            retired = self._completed_items.popleft()
            self._completed_item_set.discard(retired)
            self._items.pop(retired, None)
            self._item_starts.pop(retired, None)

    def _retire_segment_mappings(self, segment: TranscriptSegment) -> None:
        """Drop completed segment transcript bookkeeping after publish."""
        for item, mapped in list(self._items.items()):
            if mapped is segment:
                self._remember_completed_item(item)
                self._items.pop(item, None)
                # Keep a bounded duplicate window; drop start map with item.
                self._item_starts.pop(item, None)
        segment.result = None

    def is_finalize_timed_out(self, finalize_id: str) -> bool:
        return finalize_id in self._timed_out_finalize_set

    def mark_finalize_timed_out(self, finalize_id: str) -> None:
        if finalize_id not in self._timed_out_finalize_set:
            self._timed_out_finalizes.append(finalize_id)
            self._timed_out_finalize_set.add(finalize_id)
            while len(self._timed_out_finalizes) > self._MAX_COMPLETED_ITEMS:
                retired = self._timed_out_finalizes.popleft()
                self._timed_out_finalize_set.discard(retired)
        for segment in self._pending:
            if segment.finalize_id == finalize_id:
                segment.timed_out = True

    async def send_audio(
        self,
        audio_data: bytes,
        *,
        metadata: dict[str, Any] | None = None,
        session_id: str | None = None,
    ) -> bool:
        """Send PCM directly without retaining a copy for final or reconnect."""
        if not audio_data:
            return True
        if len(audio_data) % (self.config.bits // 8 * self.config.channels):
            raise ValueError("StepFun audio must contain complete PCM samples")
        async with self._pump_lock:
            if not self.is_connected():
                return False
            self._stamp_open_segment(metadata, session_id)
            try:
                await self._pump()
                for offset in range(0, len(audio_data), 3200):
                    chunk = audio_data[offset : offset + 3200]
                    await self._send_json(
                        "input_audio_buffer.append",
                        audio=base64.b64encode(chunk).decode("ascii"),
                    )
                    self._tail += len(chunk)
                    await self.callback.on_audio_sent(self._tail)
                return True
            except ConnectionClosedOK:
                self._mark_normal_close()
                self._started = False
                await self.callback.on_close()
                return False
            except Exception as exc:
                self._started = False
                await self.callback.on_error(str(exc), "send_failed")
                await self.callback.on_close()
                return False

    async def commit(
        self,
        finalize_id: str,
        finalize_time: float,
        *,
        metadata: dict[str, Any] | None = None,
        session_id: str | None = None,
    ) -> None:
        async with self._pump_lock:
            end = self._tail
            if end > self._current.start:
                if not self._current.can_bind_finalize(session_id):
                    # Open span belongs to another session — leave it alone and
                    # acknowledge this finalize as empty.
                    await self.callback.on_result(
                        {
                            "type": "empty.completed",
                            "transcript": "",
                        },
                        TranscriptSegment(
                            end,
                            end,
                            finalize_id=finalize_id,
                            finalize_time=finalize_time,
                            metadata=(
                                dict(metadata) if metadata is not None else None
                            ),
                            session_id=session_id,
                            timed_out=finalize_id
                            in self._timed_out_finalize_set,
                        ),
                    )
                    return
                segment = self._seal_current(end)
                segment.bind_finalize(
                    finalize_id,
                    finalize_time,
                    metadata=metadata,
                    session_id=session_id,
                    timed_out=finalize_id in self._timed_out_finalize_set,
                    force_identity=True,
                )
                await self._pump_or_reconnect()
                return
            pending_segment = next(
                (
                    segment
                    for segment in reversed(self._pending)
                    if segment.can_bind_finalize(session_id)
                ),
                None,
            )
            if pending_segment is not None:
                pending_segment.bind_finalize(
                    finalize_id,
                    finalize_time,
                    metadata=metadata,
                    session_id=session_id,
                    timed_out=finalize_id in self._timed_out_finalize_set,
                )
                if self.is_connected():
                    await self._queue_commit(pending_segment)
                await self._pump_or_reconnect()
                return
            if end == self._current.start:
                await self.callback.on_result(
                    {
                        "type": "empty.completed",
                        "transcript": "",
                    },
                    TranscriptSegment(
                        end,
                        end,
                        finalize_id=finalize_id,
                        finalize_time=finalize_time,
                        metadata=(
                            dict(metadata) if metadata is not None else None
                        ),
                        session_id=session_id,
                        timed_out=finalize_id in self._timed_out_finalize_set,
                    ),
                )
                return
            await self._pump_or_reconnect()

    async def _pump_or_reconnect(self) -> bool:
        if not self.is_connected():
            return False
        try:
            await self._pump()
            return True
        except ConnectionClosedOK:
            # Completed normal close must not emit an ASR error.
            self._mark_normal_close()
            self._started = False
            await self.callback.on_close()
            return False
        except Exception as exc:
            self._started = False
            await self.callback.on_error(str(exc), "send_failed")
            await self.callback.on_close()
            return False

    async def _pump(self) -> None:
        for segment in list(self._pending):
            if segment.generation == self._generation:
                continue
            if segment.end is None:
                continue
            segment.generation = self._generation
            if (
                segment.finalize_id is not None
                or not self.config.server_vad
                or segment.needs_vendor_commit
            ):
                await self._queue_commit(segment)
                segment.needs_vendor_commit = False

    async def _queue_commit(self, segment: TranscriptSegment) -> None:
        if segment.commit_generation == self._generation:
            return
        segment.commit_generation = self._generation
        self._awaiting_ack.append(segment)
        await self._send_json("input_audio_buffer.commit")

    async def _send_json(self, kind: str, **fields: Any) -> None:
        if self.websocket is None:
            raise ConnectionError("StepFun WebSocket is not initialized")
        await self.websocket.send(
            json.dumps(
                {
                    "event_id": f"stepfun_{uuid.uuid4().hex}",
                    "type": kind,
                    **fields,
                },
                separators=(",", ":"),
            )
        )

    def is_connected(self) -> bool:
        if not self._started or self.websocket is None:
            return False
        state = getattr(getattr(self.websocket, "state", None), "name", "")
        return bool(getattr(self.websocket, "open", False) or state == "OPEN")

    async def close(self) -> None:
        self._closing = True
        self._started = False
        if (
            self._receive_task
            and self._receive_task is not asyncio.current_task()
        ):
            self._receive_task.cancel()
            with suppress(asyncio.CancelledError):
                await self._receive_task
        self._receive_task = None
        if self.websocket is not None:
            try:
                await self.websocket.close()
            finally:
                self.websocket = None

    async def shutdown(self) -> None:
        await self.close()
