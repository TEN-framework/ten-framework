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

Design D4: frames that arrive while the session is down are kept for at most
``INPUT_QUEUE_MAX_MS`` of playout; the oldest frames are dropped beyond
that, markers are never dropped.
"""

import asyncio
import traceback
from collections import deque
from dataclasses import dataclass
from typing import Callable, Optional, Protocol, Union

from .audio import Resampler, duration_ms
from .pacer import Pacer, PushResult

INPUT_QUEUE_MAX_MS = 10_000


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

    The bound is playout time of the queued frames (D4). When it is exceeded
    the oldest frames are dropped, never a marker; the caller is told once
    per overflow episode so the log is not flooded.
    """

    def __init__(self, max_ms: int = INPUT_QUEUE_MAX_MS) -> None:
        self._max_ms = max_ms
        self._items: deque[QueueItem] = deque()
        self._frames = 0
        self._queued_ms = 0.0
        self._ready = asyncio.Event()
        self._overflowing = False

    def __len__(self) -> int:
        return len(self._items)

    @property
    def queued_ms(self) -> float:
        return self._queued_ms

    @property
    def turns(self) -> set[Turn]:
        """Identified turns still queued, including end-only requests."""
        return {item.turn for item in self._items if item.turn is not None}

    def put_audio(
        self, pcm: bytes, rate: int, turn: Optional[Turn] = None
    ) -> bool:
        """Queue a frame; True when this put started dropping old frames."""
        self._items.append(Frame(pcm, rate, turn))
        self._frames += 1
        self._queued_ms += duration_ms(pcm, rate)
        first_drop = False
        while self._queued_ms > self._max_ms and self._frames > 1:
            self._drop_oldest_frame()
            if not self._overflowing:
                self._overflowing = True
                first_drop = True
        self._ready.set()
        return first_drop

    def put_end(self, end: End) -> None:
        self._items.append(end)
        self._ready.set()

    def _drop_oldest_frame(self) -> None:
        for index, item in enumerate(self._items):
            if isinstance(item, Frame):
                del self._items[index]
                self._frames -= 1
                self._queued_ms -= duration_ms(item.pcm, item.rate)
                return

    async def get(self) -> QueueItem:
        while not self._items:
            self._ready.clear()
            await self._ready.wait()
        item = self._items.popleft()
        if isinstance(item, Frame):
            self._frames -= 1
            self._queued_ms -= duration_ms(item.pcm, item.rate)
        if self._frames == 0:
            self._queued_ms = 0.0
            self._overflowing = False
        return item

    def clear(self) -> int:
        """Drop everything; returns how many items were dropped."""
        count = len(self._items)
        self._items.clear()
        self._frames = 0
        self._queued_ms = 0.0
        self._overflowing = False
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
    ) -> None:
        self._pacer = pacer
        self._rate = sample_rate
        self._log = log
        self._on_request_state = on_request_state
        self._queue = InputQueue(max_queue_ms)
        self._max_queue_ms = max_queue_ms
        self._connected = asyncio.Event()
        self._resampler: Optional[Resampler] = None
        self._flush_gen = 0
        self._active_turn: Optional[Turn] = None
        self._inflight_turn: Optional[Turn] = None
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
        if turn is not None and turn in self._retired_turns:
            self._log.log_debug("audio ignored: retired turn")
            return
        if self._queue.put_audio(pcm, rate, turn):
            self._log.log_warn(
                f"input queue over {self._max_queue_ms} ms while the session "
                "is not up: dropping the oldest audio"
            )

    def put_end(self, end: End) -> None:
        self._queue.put_end(end)

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
        self._queue.clear()
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
            try:
                if isinstance(item, End):
                    await self._apply_end(item)
                else:
                    self._inflight_turn = item.turn
                    await self._send_frame(item)
            except Exception:
                # CancelledError derives from BaseException, so cancelling
                # the sender task still propagates past this handler.
                self._log.log_error(f"sender: {traceback.format_exc()}")
            finally:
                self._inflight_turn = None

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
            if gen != self._flush_gen:
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
