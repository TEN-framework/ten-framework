#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Request pacer: request boundaries, silence padding and gating.

Pure: no TEN or SpatialReal imports. The pacer talks to the session through
the ``PacerSink`` protocol and takes its clock and sleep from the caller so
the tests drive time by hand.

Design D5. One avatar request per TTS request. The first audio after idle
opens a request; the first of ``tts_audio_end`` (reasons 1, 3, unknown, and
2 unless a flush has been processed), ``finalize``, the padding bound or the
stall backstop closes it with ``send_audio(b"", end=True)``. Closing is
idempotent.

Padding keeps egress's playout buffer from running dry inside a request:
the virtual playhead is ``ahead_ms = sent_ms - (now - first_send)`` and a
tick sends enough 100 ms silence chunks to return it to ``pad_floor_ms``
(zero by default) only after it dips below. Positive lead is left alone so
the next speech chunk can follow without inserted silence.
``pad_max_ms`` of contiguous padding with no new real audio closes the
request; ``turn_stall_ms`` after the real audio drained is the outer backstop.

A new request whose predecessor is still less than ``gate_ms`` past drained
waits before its first chunk so the predecessor's tail is not cut by the
new server-side session. Design D6: ``flush`` cancels that wait and the
frame that was waiting (``push_audio`` returns ``PushResult.CANCELLED``
without sending), closes the open request locally without ``end=True``,
interrupts the sink and waives the gate for the next request.

The sink reports whether a send went out. A send that did not (the session
dropped between the sender's readiness check and the send, possibly after a
long gate wait) is not counted: ``push_audio`` returns
``PushResult.NOT_SENT`` and forgets the request it may just have opened, so
the caller can hold the chunk for the replacement session.
"""

import asyncio
import math
import time
from enum import Enum
from typing import Awaitable, Callable, Optional, Protocol

from .audio import duration_ms, silence

# Refill only after the virtual playhead drains. A positive reserve inserts
# silence between speech chunks even when the next chunk arrives in time.
PAD_FLOOR_MS = 0
PAD_TICK_MS = 100
PAD_MAX_MS = 3000
TURN_STALL_MS = 15000
GATE_MS = 2000

_EPS_MS = 1e-3


class PushResult(Enum):
    """What became of a ``push_audio`` call."""

    SENT = "sent"
    # A flush landed while the chunk waited at the gate: it belonged to the
    # cancelled turn and was not sent.
    CANCELLED = "cancelled"
    # The sink could not send (session gone): the chunk is still owed.
    NOT_SENT = "not_sent"


class PacerSink(Protocol):
    """What the pacer needs from the session: audio out and interrupt.

    ``send_audio`` returns False when nothing went out because the session
    is not connected or the send failed; by then the session has told the
    extension it is down (``on_session_reset``) or is about to.
    """

    async def send_audio(self, pcm: bytes, end: bool = False) -> bool: ...

    async def interrupt(self) -> bool: ...


class Pacer:
    """Owns request boundaries and the virtual playhead for one session.

    All methods run on one asyncio loop; ``push_audio`` is called by a
    single sender, ``tick``/``run`` by the ticker, ``flush`` and the end
    signals by the extension's handlers. State is re-read after every
    ``await`` so an end or flush that lands mid-send stops the padding.
    """

    def __init__(
        self,
        sink: PacerSink,
        sample_rate: int,
        *,
        clock: Callable[[], float] = time.monotonic,
        sleep: Callable[[float], Awaitable[None]] = asyncio.sleep,
        pad_floor_ms: int = PAD_FLOOR_MS,
        pad_tick_ms: int = PAD_TICK_MS,
        pad_max_ms: int = PAD_MAX_MS,
        turn_stall_ms: int = TURN_STALL_MS,
        gate_ms: int = GATE_MS,
    ) -> None:
        if sample_rate <= 0:
            raise ValueError("sample_rate must be positive")
        if pad_tick_ms <= 0:
            raise ValueError("pad_tick_ms must be positive")
        self._sink = sink
        self._rate = sample_rate
        self._clock = clock
        self._sleep = sleep
        self._pad_floor_ms = pad_floor_ms
        self._pad_tick_ms = pad_tick_ms
        self._pad_max_ms = pad_max_ms
        self._turn_stall_ms = turn_stall_ms
        self._gate_ms = gate_ms
        # Raises ValueError for a tick that is not whole samples at this rate.
        self._pad_chunk = silence(pad_tick_ms, sample_rate)

        self._req_open = False
        self._first_send: Optional[float] = None
        self._sent_ms = 0.0
        self._real_sent_ms = 0.0
        self._padded_ms_contiguous = 0.0
        self._last_real_audio_at: Optional[float] = None
        self._flush_seen = False
        self._gate_waiver = False
        self._gate_abort = asyncio.Event()
        # Bumped by every flush; a gate wait that started under an older
        # value was cancelled by it.
        self._cancel_gen = 0

    # -- observable state ---------------------------------------------------

    @property
    def request_open(self) -> bool:
        return self._req_open

    @property
    def ahead_ms(self) -> float:
        """Lead of the current or last request's playhead over wall time.

        Negative once the sent audio has drained; ``-inf`` when nothing has
        been sent since construction or the last session reset.
        """
        if self._first_send is None:
            return -math.inf
        return self._sent_ms - (self._clock() - self._first_send) * 1000.0

    # -- audio in -----------------------------------------------------------

    async def push_audio(self, pcm: bytes) -> PushResult:
        """Send one chunk at the session rate, opening a request if idle.

        An empty chunk is a no-op reported as sent. A chunk that a flush
        cancels while it waits at the gate is not sent. A chunk the sink
        could not send is ``NOT_SENT``: nothing is counted, and a request
        opened for it is forgotten so the next chunk opens a fresh one.
        """
        if not pcm:
            return PushResult.SENT
        opened = False
        if not self._req_open:
            if not await self._wait_for_gate():
                return PushResult.CANCELLED
            self._open()
            opened = True
        if not await self._sink.send_audio(pcm, end=False):
            if opened:
                self._forget_request()
            return PushResult.NOT_SENT
        now = self._clock()
        ms = duration_ms(pcm, self._rate)
        self._sent_ms += ms
        self._real_sent_ms += ms
        self._padded_ms_contiguous = 0.0
        self._last_real_audio_at = now
        return PushResult.SENT

    def _open(self) -> None:
        now = self._clock()
        self._req_open = True
        self._first_send = now
        self._sent_ms = 0.0
        self._real_sent_ms = 0.0
        self._padded_ms_contiguous = 0.0
        self._last_real_audio_at = now
        self._gate_waiver = False

    async def _wait_for_gate(self) -> bool:
        """Block while the previous request may still be playing out.

        The wait is a race between the injected ``sleep`` and the abort
        event ``flush`` and ``on_session_reset`` set; whichever finishes
        first, the loop re-reads the waiver and the playhead, so an early
        wake never sends too soon. Returns False when a flush landed during
        the wait: the chunk belongs to the cancelled turn. The waiver the
        flush set stays for the next request's first chunk.
        """
        gen = self._cancel_gen
        self._gate_abort.clear()
        while not self._gate_waiver:
            remaining_ms = self.ahead_ms + self._gate_ms
            if remaining_ms <= 0:
                return True
            sleeper = asyncio.ensure_future(self._sleep(remaining_ms / 1000.0))
            aborter = asyncio.ensure_future(self._gate_abort.wait())
            try:
                await asyncio.wait(
                    {sleeper, aborter}, return_when=asyncio.FIRST_COMPLETED
                )
            finally:
                sleeper.cancel()
                aborter.cancel()
            if gen != self._cancel_gen:
                return False
        return True

    # -- request end --------------------------------------------------------

    async def end_request(self, reason: str) -> bool:
        """Close the open request with ``end=True``; a no-op when idle.

        Returns True when this call closed the request, so the caller can
        log the end signal once with its reason. ``reason`` is for that log
        line only.
        """
        del reason  # the caller logs it; the pacer keeps no history
        if not self._req_open:
            return False
        self._req_open = False
        self._padded_ms_contiguous = 0.0
        # An end signal closes the window in which a late reason 2 could
        # belong to a pre-flush request.
        self._flush_seen = False
        await self._sink.send_audio(b"", end=True)
        return True

    async def on_tts_audio_end(self, reason: int) -> bool:
        """Apply the D5 rules for data ``tts_audio_end``.

        Reasons 1 (REQUEST_END), 3 (ERROR) and anything unknown close the
        request. Reason 2 (INTERRUPTED) closes it only when no flush has been
        processed since the last end signal: after a flush it may belong to
        the pre-flush request and arrive after the new turn's first frame.
        """
        if reason == 2 and self._flush_seen:
            return False
        return await self.end_request(f"tts_audio_end reason={reason}")

    async def flush(self) -> None:
        """D6: drop the open request locally, interrupt, waive the gate.

        A chunk waiting at the gate is cancelled rather than released.
        """
        self._flush_seen = True
        self._gate_waiver = True
        self._cancel_gen += 1
        self._gate_abort.set()
        if self._req_open:
            self._req_open = False
            self._padded_ms_contiguous = 0.0
        await self._sink.interrupt()

    # -- ticker -------------------------------------------------------------

    async def tick(self) -> None:
        """One padding/stall step; ``run`` calls it every ``pad_tick_ms``."""
        if not self._req_open:
            return
        now = self._clock()
        if self._stalled(now):
            await self.end_request("turn stall")
            return
        # A microsecond of tolerance: float drift in (now - first_send)
        # otherwise makes a playhead sitting exactly on the floor look one
        # whole chunk short.
        deficit_ms = self._pad_floor_ms - self.ahead_ms - _EPS_MS
        if deficit_ms <= 0:
            return
        chunks = math.ceil(deficit_ms / self._pad_tick_ms)
        for _ in range(chunks):
            if not self._req_open:
                return
            if not await self._sink.send_audio(self._pad_chunk, end=False):
                # Session gone; the reset that follows forgets the request.
                return
            self._sent_ms += self._pad_tick_ms
            self._padded_ms_contiguous += self._pad_tick_ms
            if self._padded_ms_contiguous >= self._pad_max_ms:
                await self.end_request("pad max")
                return

    def _stalled(self, now: float) -> bool:
        """True when real audio ran out ``turn_stall_ms`` ago and the request
        is still open; padding normally closes it long before this."""
        if self._first_send is None or self._last_real_audio_at is None:
            return False
        real_drained_at = self._first_send + self._real_sent_ms / 1000.0
        idle_since = max(real_drained_at, self._last_real_audio_at)
        return (now - idle_since) * 1000.0 >= self._turn_stall_ms

    async def run(self) -> None:
        """Tick every ``pad_tick_ms`` until cancelled."""
        while True:
            await self._sleep(self._pad_tick_ms / 1000.0)
            await self.tick()

    # -- session ------------------------------------------------------------

    def on_session_reset(self) -> None:
        """After a reconnect: the server has no request, forget the playhead.

        A sender waiting at the gate for the old request's tail is woken:
        with no playhead left there is nothing to wait for.
        """
        self._forget_request()
        self._gate_abort.set()

    def _forget_request(self) -> None:
        """No request and no playhead: the server-side state is gone."""
        self._req_open = False
        self._first_send = None
        self._sent_ms = 0.0
        self._real_sent_ms = 0.0
        self._padded_ms_contiguous = 0.0
        self._last_real_audio_at = None
        self._flush_seen = False
        self._gate_waiver = False
