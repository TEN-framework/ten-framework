#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Audio sender: the ordered queue between the TEN hooks and the pacer.

Pure: no TEN or SpatialReal imports. The extension's hooks only append to
the queue; one sender task drains it in order, resamples each frame to the
session rate and pushes it through the ``Pacer``.

The queue carries two kinds of item. A ``Frame`` is PCM at the rate it
arrived with. An ``End`` is the end-of-request signal (``tts_audio_end`` or
``finalize``) as an ordered marker: it is applied only after every frame
queued before it has been sent, so ``end=True`` can never overtake its own
audio while the sender is behind, waiting for the session or waiting at the
previous request's gate. ``flush`` (design D6) acts at once instead: it
clears the queue, markers included, because everything queued belongs to
the cancelled turn.

Frames and ordered end markers are preserved within explicit duration, byte
and item budgets. Admission beyond a budget raises ``QueueCapacityError``;
the extension cancels the buffered turn instead of playing truncated speech.
"""

import asyncio
import traceback
from collections import deque
from dataclasses import dataclass
from typing import Callable, Optional, Protocol, Union

from .audio import Resampler, duration_ms
from .pacer import Pacer, PushResult

INPUT_QUEUE_MAX_MS = 120_000
INPUT_QUEUE_MAX_BYTES = 16 * 1024 * 1024
INPUT_QUEUE_MAX_ITEMS = 4096


class QueueCapacityError(RuntimeError):
    """An atomic admission failed; its caller must cancel buffered speech."""

    def __init__(self, duration: float, byte_count: int, item_count: int):
        self.turn: Optional["Turn"] = None
        super().__init__(
            "input queue capacity exceeded: "
            f"duration_ms={duration:.3f} bytes={byte_count} items={item_count}"
        )


class SenderLog(Protocol):
    """The four log levels the sender uses; ``AsyncTenEnv`` satisfies it."""

    def log_debug(self, msg: str) -> None: ...

    def log_info(self, msg: str) -> None: ...

    def log_warn(self, msg: str) -> None: ...

    def log_error(self, msg: str) -> None: ...


@dataclass(frozen=True)
class Turn:
    """The identity carried by TEN TTS PCM and end metadata.

    PCM has no standard request_id. A turn may contain multiple TTS
    requests, so normal request completion does not retire the turn.
    """

    session_id: str
    turn_id: int

    @classmethod
    def from_metadata(cls, metadata) -> Optional["Turn"]:
        """Accept the actual metadata fields, including turn zero."""
        if not isinstance(metadata, dict):
            return None
        session_id = metadata.get("session_id", "")
        turn_id = metadata.get("turn_id")
        if (
            not isinstance(session_id, str)
            or not isinstance(turn_id, int)
            or isinstance(turn_id, bool)
        ):
            return None
        return cls(session_id, turn_id)


@dataclass(frozen=True)
class Frame:
    """One audio frame as it arrived: PCM int16 mono at ``rate``."""

    pcm: bytes
    rate: int
    turn: Optional[Turn] = None


@dataclass(frozen=True)
class End:
    """An end-of-request marker, applied after every frame queued before it.

    ``label`` is for the log line only. ``tts_reason`` is the
    ``tts_audio_end`` reason, so the pacer can apply the D5 reason rules;
    ``None`` is an unconditional end (``finalize``).
    """

    label: str
    tts_reason: Optional[int] = None
    turn: Optional[Turn] = None
    request_id: str = ""


QueueItem = Union[Frame, End]


class InputQueue:
    """Frames and end markers waiting for the sender, in arrival order.

    No admission removes older PCM or moves an end marker. The caller must
    handle a capacity error as cancellation, never as a normally ended tail.
    """

    def __init__(
        self,
        max_ms: int = INPUT_QUEUE_MAX_MS,
        max_bytes: int = INPUT_QUEUE_MAX_BYTES,
        max_items: int = INPUT_QUEUE_MAX_ITEMS,
    ) -> None:
        self._max_ms = max_ms
        self._max_bytes = max_bytes
        self._max_items = max_items
        self._items: deque[QueueItem] = deque()
        self._frames = 0
        self._queued_ms = 0.0
        self._queued_bytes = 0
        self._ready = asyncio.Event()

    def __len__(self) -> int:
        return len(self._items)

    @property
    def queued_ms(self) -> float:
        return self._queued_ms

    def check_capacity(
        self, duration: float, byte_count: int, item_count: int
    ) -> None:
        """Check additional retained data before mutating the FIFO."""
        duration += self._queued_ms
        byte_count += self._queued_bytes
        item_count += len(self._items)
        if (
            duration > self._max_ms
            or byte_count > self._max_bytes
            or item_count > self._max_items
        ):
            raise QueueCapacityError(duration, byte_count, item_count)

    @property
    def turns(self) -> set[Turn]:
        """Identified turns still queued, including end-only requests."""
        return {item.turn for item in self._items if item.turn is not None}

    @property
    def has_unidentified(self) -> bool:
        """Whether cancellation would include items without turn metadata."""
        return any(item.turn is None for item in self._items)

    def put_audio(
        self, pcm: bytes, rate: int, turn: Optional[Turn] = None
    ) -> bool:
        """Queue a frame without discarding any earlier item."""
        self.check_capacity(duration_ms(pcm, rate), len(pcm), 1)
        self._items.append(Frame(pcm, rate, turn))
        self._frames += 1
        self._queued_ms += duration_ms(pcm, rate)
        self._queued_bytes += len(pcm)
        self._ready.set()
        return False

    def put_end(self, end: End) -> None:
        self.check_capacity(0.0, 0, 1)
        self._items.append(end)
        self._ready.set()

    async def get(self) -> QueueItem:
        while not self._items:
            self._ready.clear()
            await self._ready.wait()
        item = self._items.popleft()
        if isinstance(item, Frame):
            self._frames -= 1
            self._queued_ms -= duration_ms(item.pcm, item.rate)
            self._queued_bytes -= len(item.pcm)
        if self._frames == 0:
            self._queued_ms = 0.0
        return item

    def clear(self) -> int:
        """Drop everything; returns how many items were dropped."""
        count = len(self._items)
        self._items.clear()
        self._frames = 0
        self._queued_ms = 0.0
        self._queued_bytes = 0
        self._ready.clear()
        return count


class AudioSender:
    """Drains the input queue into the pacer, in order.

    All methods run on one asyncio loop. The hooks call ``put_audio``,
    ``put_end`` and ``flush``; the session callbacks call ``session_up`` and
    ``session_down``; ``run`` is the sender task. ``on_request_state`` is
    told the pacer's request state after every operation that may change
    it, so the session's idle timer knows when a request is open.
    """

    def __init__(
        self,
        pacer: Pacer,
        sample_rate: int,
        log: SenderLog,
        *,
        on_request_state: Callable[[bool], None] = lambda open_: None,
        max_queue_ms: int = INPUT_QUEUE_MAX_MS,
        max_queue_bytes: int = INPUT_QUEUE_MAX_BYTES,
        max_queue_items: int = INPUT_QUEUE_MAX_ITEMS,
    ) -> None:
        self._pacer = pacer
        self._rate = sample_rate
        self._log = log
        self._on_request_state = on_request_state
        self._queue = InputQueue(max_queue_ms, max_queue_bytes, max_queue_items)
        self._connected = asyncio.Event()
        self._resampler: Optional[Resampler] = None
        self._flush_gen = 0
        self._active_turn: Optional[Turn] = None
        self._inflight_turn: Optional[Turn] = None
        self._inflight_item: Optional[QueueItem] = None
        self._legacy_overflowed = False
        self._retired_turns: set[Turn] = set()
        self._latest_turn_ids: dict[str, int] = {}
        self._ended_requests: set[tuple[Optional[Turn], str]] = set()
        self._flushing = 0
        self._flush_done = asyncio.Event()
        self._flush_done.set()

    # -- observable state ---------------------------------------------------

    @property
    def request_open(self) -> bool:
        return self._pacer.request_open

    @property
    def queued(self) -> int:
        """Items still waiting for the sender."""
        return len(self._queue)

    # -- hooks --------------------------------------------------------------

    def put_audio(
        self, pcm: bytes, rate: int, turn: Optional[Turn] = None
    ) -> None:
        if not pcm:
            return
        if turn is None and self._legacy_overflowed:
            return
        if turn is not None and turn in self._retired_turns:
            self._log.log_debug("audio ignored: retired turn")
            return
        self._check_admission(duration_ms(pcm, rate), len(pcm), turn)
        self._queue.put_audio(pcm, rate, turn)

    def put_end(self, end: End) -> None:
        if end.turn is None and self._legacy_overflowed:
            # Without shared metadata, only this ordered request boundary
            # proves that later unidentified PCM can belong to a new request.
            key = (end.turn, end.request_id)
            if end.request_id and key in self._ended_requests:
                return
            if end.request_id:
                self._ended_requests.add(key)
            self._legacy_overflowed = False
            return
        if end.turn is not None and end.turn in self._retired_turns:
            return
        try:
            self._check_admission(0.0, 0, end.turn)
        except QueueCapacityError:
            if end.turn is None:
                # The boundary that hit the item budget still terminates
                # the unidentified failed request; never send it as EOF.
                self._legacy_overflowed = False
                if end.request_id:
                    self._ended_requests.add((end.turn, end.request_id))
            raise
        self._queue.put_end(end)

    def _check_admission(
        self, duration: float, byte_count: int, turn: Optional[Turn]
    ) -> None:
        held = self._inflight_item
        if isinstance(held, Frame):
            duration += duration_ms(held.pcm, held.rate)
            byte_count += len(held.pcm)
        try:
            self._queue.check_capacity(
                duration, byte_count, 1 + int(held is not None)
            )
        except QueueCapacityError as error:
            # This must precede the extension's awaited cancellation: late
            # failed-turn PCM/end cannot become a normal shortened request.
            if (
                turn is None
                or self._queue.has_unidentified
                or (held is not None and held.turn is None)
                or (self._active_turn is None and self._pacer.request_open)
            ):
                self._legacy_overflowed = True
            if turn is not None:
                self._retired_turns.add(turn)
            error.turn = turn
            raise

    async def flush(self) -> None:
        """D6: drop everything queued, cancel the turn, interrupt the avatar."""
        self._flush_gen += 1
        self._flushing += 1
        self._flush_done.clear()
        self._retired_turns.update(self._queue.turns)
        for turn in (self._active_turn, self._inflight_turn):
            if turn is not None:
                self._retired_turns.add(turn)
        self._active_turn = None
        dropped = self._queue.clear()
        if dropped:
            self._log.log_debug(f"flush: dropped {dropped} queued items")
        self._reset_resampler()
        try:
            await self._pacer.flush()
        finally:
            self._note_request_state()
            self._flushing -= 1
            if not self._flushing:
                self._flush_done.set()

    def clear(self) -> None:
        """Drop the queue without touching the pacer (used at stop)."""
        self._flush_gen += 1
        self._queue.clear()
        self._legacy_overflowed = False
        self._active_turn = None
        self._inflight_turn = None
        self._inflight_item = None
        self._reset_resampler()
        self._retired_turns.clear()
        self._latest_turn_ids.clear()
        self._ended_requests.clear()

    # -- session ------------------------------------------------------------

    def session_up(self) -> None:
        self._pacer.on_session_reset()
        self._reset_resampler()
        self._note_request_state()
        self._connected.set()

    def session_down(self) -> None:
        self._connected.clear()
        self._pacer.on_session_reset()
        self._reset_resampler()
        self._note_request_state()

    # -- sender task --------------------------------------------------------

    async def run(self) -> None:
        """Drain the queue until cancelled.

        A frame waits for the session before it is sent, so audio queued
        during a reconnect goes out (bounded by the queue) instead of being
        lost; a flush that lands during that wait, or while the frame waits
        at the pacer's gate, drops it. A frame the session could not take
        (it dropped during the gate wait) is held for the next session. A
        marker never waits: every frame queued before it has already been
        sent or dropped by the time it is dequeued.
        """
        while True:
            item = await self._queue.get()
            self._inflight_item = item
            self._inflight_turn = item.turn
            try:
                if isinstance(item, End):
                    await self._apply_end(item)
                else:
                    await self._send_frame(item)
            except Exception:
                # CancelledError derives from BaseException, so cancelling
                # the sender task still propagates past this handler.
                self._log.log_error(f"sender: {traceback.format_exc()}")
            finally:
                self._inflight_turn = None
                self._inflight_item = None

    async def _send_frame(self, frame: Frame) -> None:
        gen = self._flush_gen
        # New audio must follow the actual interrupt send. Otherwise an
        # awaited old interrupt could reach the sink after the new audio.
        await self._flush_done.wait()
        if gen != self._flush_gen:
            return
        if frame.turn is not None:
            latest = self._latest_turn_ids.get(frame.turn.session_id)
            if frame.turn in self._retired_turns or (
                latest is not None and frame.turn.turn_id < latest
            ):
                self._log.log_debug("audio ignored: retired turn")
                return
            # TEN's controller increments turn_id within a session. A
            # cancelled turn's first PCM can arrive only after its successor
            # starts, so it need not appear in the retired set yet. Update
            # on dequeue, not admission: already-queued earlier audio is valid.
            self._latest_turn_ids[frame.turn.session_id] = frame.turn.turn_id
            if (
                self._active_turn is not None
                and frame.turn != self._active_turn
            ):
                # The next identified turn is an actual boundary, even if
                # its predecessor's end was delayed or never delivered.
                self._retired_turns.add(self._active_turn)
                await self._pacer.end_request("turn changed")
                if gen != self._flush_gen:
                    return
                self._reset_resampler()
            self._active_turn = frame.turn
        converted: Optional[bytes] = None
        while True:
            await self._connected.wait()
            if gen != self._flush_gen or (
                frame.turn is not None and frame.turn in self._retired_turns
            ):
                # A flush landed while this frame waited for the session: it
                # belongs to the cancelled turn.
                return
            if converted is None:
                converted = self._resample(frame.pcm, frame.rate)
                if not converted:
                    return
            result = await self._pacer.push_audio(converted)
            self._note_request_state()
            if result is PushResult.SENT:
                return
            if result is PushResult.CANCELLED:
                self._log.log_debug(
                    "flush: dropped the frame waiting at the gate"
                )
                return
            if self._connected.is_set():
                # Refused while the session is still up: a send failure,
                # not an outage. There is no reconnect to wait for.
                self._log.log_warn(
                    "send failed while the session is up: dropping the frame"
                )
                return
            # The session went down between the readiness check and the
            # send (a gate wait can last seconds); the frame waits for the
            # next session.
            self._log.log_debug("send failed: holding the frame for reconnect")

    async def _apply_end(self, end: End) -> None:
        gen = self._flush_gen
        await self._flush_done.wait()
        if gen != self._flush_gen:
            return
        if end.turn is not None and (
            end.turn in self._retired_turns
            or (self._active_turn is not None and end.turn != self._active_turn)
        ):
            self._log.log_debug(f"end ignored: stale turn; {end.label}")
            return
        key = (end.turn, end.request_id)
        if end.request_id and key in self._ended_requests:
            self._log.log_debug(f"end ignored: duplicate request; {end.label}")
            return
        if end.request_id:
            self._ended_requests.add(key)
        # A matching identified end belongs to this turn, including reason
        # 2 after a prior flush. Keep the legacy reason guard only when no
        # common turn identity is available.
        if end.tts_reason is None or (
            end.turn is not None and end.turn == self._active_turn
        ):
            closed = await self._pacer.end_request(end.label)
        else:
            closed = await self._pacer.on_tts_audio_end(end.tts_reason)
        self._note_request_state()
        if closed:
            self._reset_resampler()
            self._log.log_info(f"request closed: {end.label}")
        else:
            self._log.log_debug(
                f"end ignored: {end.label} "
                f"request_open={self._pacer.request_open}"
            )

    # -- helpers ------------------------------------------------------------

    def _resample(self, pcm: bytes, rate: int) -> bytes:
        if self._resampler is None or self._resampler.src_rate != rate:
            if self._resampler is not None:
                self._log.log_info(
                    f"input sample rate changed "
                    f"{self._resampler.src_rate} -> {rate}"
                )
            self._resampler = Resampler(rate, self._rate)
        return self._resampler.convert(pcm)

    def _reset_resampler(self) -> None:
        if self._resampler is not None:
            self._resampler.reset()

    def _note_request_state(self) -> None:
        self._on_request_state(self._pacer.request_open)
