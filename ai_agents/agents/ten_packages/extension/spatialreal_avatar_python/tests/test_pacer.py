#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Unit tests for the request pacer (design D5 and D6).

Time never passes for real: the pacer gets the ``FakeClock`` and a fake
``sleep`` that records what was asked and advances the clock, and the
tests call ``tick()`` by hand instead of running the ticker loop.
"""

import asyncio
import math

import pytest

from spatialreal_avatar_python.audio import duration_ms
from spatialreal_avatar_python.pacer import Pacer, PushResult

pytestmark = pytest.mark.asyncio

RATE = 24000


class FakeSink:
    """Records every call the pacer makes, in order.

    ``connected`` is what ``send_audio`` returns, as the session client
    answers False once the session is gone.
    """

    def __init__(self):
        self.calls: list[tuple] = []
        self.connected = True

    async def send_audio(self, pcm: bytes, end: bool = False) -> bool:
        self.calls.append(("audio", pcm, end))
        return self.connected

    async def interrupt(self) -> bool:
        self.calls.append(("interrupt",))
        return self.connected

    @property
    def ends(self) -> int:
        return sum(1 for c in self.calls if c[0] == "audio" and c[2])

    @property
    def interrupts(self) -> int:
        return sum(1 for c in self.calls if c[0] == "interrupt")

    def audio_ms(self, *, silent: bool | None = None) -> float:
        """Milliseconds of audio sent; ``silent`` filters zeros / non-zeros."""
        total = 0.0
        for call in self.calls:
            if call[0] != "audio" or call[2]:
                continue
            is_silent = not call[1].strip(b"\x00")
            if silent is None or is_silent == silent:
                total += duration_ms(call[1], RATE)
        return total

    def silence_chunks(self) -> list[bytes]:
        return [
            c[1]
            for c in self.calls
            if c[0] == "audio" and not c[2] and not c[1].strip(b"\x00")
        ]


class PausingPaddingSink(FakeSink):
    """Let another pacer operation run during the first padding send."""

    def __init__(self):
        super().__init__()
        self.padding_started = asyncio.Event()
        self.release_padding = asyncio.Event()

    async def send_audio(self, pcm: bytes, end: bool = False) -> bool:
        result = await super().send_audio(pcm, end=end)
        if pcm and not pcm.strip(b"\x00"):
            if not self.padding_started.is_set():
                self.padding_started.set()
                await self.release_padding.wait()
        return result


class FakeSleep:
    """Records each requested sleep and advances the clock by it.

    It yields to the event loop once, as a real sleep would, so a loop
    built on it (``Pacer.run``) can be cancelled from the test.
    """

    def __init__(self, clock):
        self.clock = clock
        self.calls: list[float] = []

    async def __call__(self, seconds: float) -> None:
        self.calls.append(seconds)
        self.clock.advance(seconds * 1000)
        await asyncio.sleep(0)


class HangingSleep:
    """Records the request and never returns until cancelled."""

    def __init__(self):
        self.calls: list[float] = []

    async def __call__(self, seconds: float) -> None:
        self.calls.append(seconds)
        await asyncio.Event().wait()


def _speech(ms: int, rate: int = RATE) -> bytes:
    """Non-silent int16 PCM of ``ms`` (every sample is 0x0101)."""
    return b"\x01\x01" * (rate * ms // 1000)


def _pacer(clock, sink, sleep=None, **kw) -> Pacer:
    return Pacer(
        sink,
        RATE,
        clock=clock.now,
        sleep=sleep or FakeSleep(clock),
        **kw,
    )


async def test_first_audio_opens_request_and_sends(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    assert not pacer.request_open
    assert pacer.ahead_ms == -math.inf

    pcm = _speech(200)
    assert await pacer.push_audio(pcm) is PushResult.SENT

    assert pacer.request_open
    assert sink.calls == [("audio", pcm, False)]
    assert pacer.ahead_ms == 200.0

    # More audio within the request is sent as it arrives: no gate, no pad.
    await pacer.push_audio(_speech(100))
    assert len(sink.calls) == 2
    assert pacer.ahead_ms == 300.0


async def test_empty_audio_does_not_open_a_request(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.push_audio(b"")
    assert not pacer.request_open
    assert sink.calls == []


async def test_ahead_goes_negative_once_drained(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.push_audio(_speech(200))
    clock.advance(500)
    assert pacer.ahead_ms == pytest.approx(-300.0)


@pytest.mark.parametrize(
    "first_ms,tick_ms,next_chunk_at_ms",
    [(139, 62, 66), (139, 37, 65), (325, 28, 77)],
)
async def test_tick_preserves_speech_when_next_chunk_arrives_before_drain(
    clock, first_ms, tick_ms, next_chunk_at_ms
):
    """A ticker must not splice silence into speech that has not run out."""
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    first = _speech(first_ms)
    second = _speech(200)
    await pacer.push_audio(first)

    clock.advance(tick_ms)
    assert pacer.ahead_ms > 0
    await pacer.tick()
    assert sink.calls == [("audio", first, False)]

    clock.advance(next_chunk_at_ms - tick_ms)
    assert pacer.ahead_ms > 0
    await pacer.push_audio(second)
    await pacer.end_request("tts_audio_end")
    assert sink.calls == [
        ("audio", first, False),
        ("audio", second, False),
        ("audio", b"", True),
    ]


async def test_padding_rechecks_lead_after_real_audio_arrives_during_send(
    clock,
):
    sink = PausingPaddingSink()
    pacer = _pacer(clock, sink)
    first, second, third = _speech(20), _speech(1000), _speech(100)
    await pacer.push_audio(first)
    clock.advance(270)
    assert pacer.ahead_ms == pytest.approx(-250)

    ticker = asyncio.create_task(pacer.tick())
    try:
        await asyncio.wait_for(sink.padding_started.wait(), timeout=1)
        await pacer.push_audio(second)
        assert pacer.ahead_ms > 0
    finally:
        sink.release_padding.set()
        await asyncio.wait_for(ticker, timeout=1)

    await pacer.push_audio(third)
    await pacer.end_request("tts_audio_end")
    assert sink.calls == [
        ("audio", first, False),
        ("audio", bytes(RATE // 10 * 2), False),
        ("audio", second, False),
        ("audio", third, False),
        ("audio", b"", True),
    ]


async def test_end_during_padding_send_stops_padding_after_that_send(clock):
    sink = PausingPaddingSink()
    pacer = _pacer(clock, sink)
    first = _speech(20)
    await pacer.push_audio(first)
    clock.advance(270)

    ticker = asyncio.create_task(pacer.tick())
    try:
        await asyncio.wait_for(sink.padding_started.wait(), timeout=1)
        assert await pacer.end_request("tts_audio_end") is True
        assert await pacer.end_request("finalize") is False
    finally:
        sink.release_padding.set()
        await asyncio.wait_for(ticker, timeout=1)

    assert not pacer.request_open
    assert sink.calls == [
        ("audio", first, False),
        ("audio", bytes(RATE // 10 * 2), False),
        ("audio", b"", True),
    ]


async def test_padding_when_ahead_below_floor(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.push_audio(_speech(200))
    clock.advance(400)
    assert pacer.ahead_ms == pytest.approx(-200.0)

    await pacer.tick()

    # Catch up the actual deficit, without creating a reserve inside speech.
    assert sink.audio_ms(silent=True) == 200.0
    assert len(sink.silence_chunks()) == 2
    assert pacer.ahead_ms == pytest.approx(0.0, abs=1e-6)
    assert pacer.request_open
    assert sink.ends == 0


async def test_no_padding_when_ahead_above_floor(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.push_audio(_speech(2000))
    clock.advance(400)
    await pacer.tick()
    assert sink.audio_ms(silent=True) == 0.0
    assert len(sink.calls) == 1


async def test_no_padding_without_open_request(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.tick()
    assert sink.calls == []


async def test_pad_max_closes_request(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.push_audio(_speech(200))

    ticks = 0
    while pacer.request_open and ticks < 100:
        clock.advance(100)
        await pacer.tick()
        ticks += 1

    assert not pacer.request_open
    assert sink.ends == 1
    assert sink.audio_ms(silent=True) == 3000.0
    assert sink.calls[-1] == ("audio", b"", True)

    # Closed means closed: later ticks neither pad nor end again.
    before = len(sink.calls)
    clock.advance(100)
    await pacer.tick()
    assert len(sink.calls) == before


async def test_padding_resumes_after_real_audio(clock):
    """Real audio resets the contiguous-padding counter (D5 "Bounds")."""
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.push_audio(_speech(200))
    for _ in range(25):  # 200 ms of speech, then 2.3 s of bounded padding
        clock.advance(100)
        await pacer.tick()
    assert pacer.request_open
    assert sink.audio_ms(silent=True) == 2300.0

    await pacer.push_audio(_speech(100))
    for _ in range(25):
        clock.advance(100)
        await pacer.tick()
    assert pacer.request_open
    assert sink.ends == 0


async def test_turn_stall_closes_request(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.push_audio(_speech(200))

    clock.advance(15_200)
    await pacer.tick()

    assert not pacer.request_open
    assert sink.ends == 1
    # The stall backstop closes before any padding is attempted.
    assert sink.audio_ms(silent=True) == 0.0


@pytest.mark.parametrize("reason", [1, 3, 99])
async def test_tts_audio_end_reason_1_and_3_and_unknown_close(clock, reason):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.push_audio(_speech(200))

    assert await pacer.on_tts_audio_end(reason) is True

    assert not pacer.request_open
    assert sink.ends == 1
    assert sink.calls[-1] == ("audio", b"", True)


async def test_reason_2_closes_without_flush_and_is_ignored_after_flush(
    clock,
):
    sink = FakeSink()
    pacer = _pacer(clock, sink)

    # No flush since open: INTERRUPTED comes from a request_id preemption
    # and closes the request.
    await pacer.push_audio(_speech(200))
    assert await pacer.on_tts_audio_end(2) is True
    assert not pacer.request_open
    assert sink.ends == 1

    # A flush, then the new turn's first frame, then a late reason 2 that
    # belongs to the pre-flush request: the new request stays open.
    await pacer.push_audio(_speech(200))
    await pacer.flush()
    assert not pacer.request_open
    assert sink.ends == 1  # flush closes locally, no end=True
    await pacer.push_audio(_speech(200))
    assert pacer.request_open
    assert await pacer.on_tts_audio_end(2) is False
    assert pacer.request_open
    assert sink.ends == 1

    # The real end of the new request closes it and re-arms reason 2.
    assert await pacer.on_tts_audio_end(1) is True
    assert sink.ends == 2
    await pacer.push_audio(_speech(200))
    assert await pacer.on_tts_audio_end(2) is True
    assert sink.ends == 3


async def test_end_request_idempotent(clock):
    """Review Focus 2: finalize then tts_audio_end sends one end=True."""
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.push_audio(_speech(200))

    assert await pacer.end_request("finalize") is True
    assert await pacer.on_tts_audio_end(1) is False
    assert await pacer.end_request("finalize") is False

    assert sink.ends == 1
    assert not pacer.request_open


async def test_end_request_without_open_request_is_noop(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    assert await pacer.end_request("finalize") is False
    assert await pacer.on_tts_audio_end(1) is False
    assert sink.calls == []


async def test_gate_waits_until_drained(clock):
    sink = FakeSink()
    sleep = FakeSleep(clock)
    pacer = _pacer(clock, sink, sleep)

    await pacer.push_audio(_speech(2000))
    await pacer.end_request("tts_audio_end")
    assert pacer.ahead_ms == pytest.approx(2000.0)

    pcm_b = _speech(300)
    await pacer.push_audio(pcm_b)

    # Request A is 2000 ms ahead; B may start once A is 2000 ms drained.
    assert len(sleep.calls) == 1
    assert sleep.calls[0] == pytest.approx(4.0)
    assert sink.calls[-1] == ("audio", pcm_b, False)
    assert pacer.request_open
    # B's playhead starts fresh.
    assert pacer.ahead_ms == pytest.approx(300.0)


async def test_gate_rechecks_after_partial_sleep(clock):
    """A sleep that wakes early just loops; it never sends ahead of time."""

    class ShortSleep(FakeSleep):
        async def __call__(self, seconds: float) -> None:
            await super().__call__(min(seconds, 1.0))

    sink = FakeSink()
    sleep = ShortSleep(clock)
    pacer = _pacer(clock, sink, sleep)
    await pacer.push_audio(_speech(2000))
    await pacer.end_request("tts_audio_end")

    await pacer.push_audio(_speech(100))
    assert len(sleep.calls) == 4
    assert sink.calls[-1][2] is False
    assert len(sink.calls) == 3


async def test_no_gate_when_predecessor_drained(clock):
    sink = FakeSink()
    sleep = FakeSleep(clock)
    pacer = _pacer(clock, sink, sleep)
    await pacer.push_audio(_speech(500))
    await pacer.end_request("tts_audio_end")
    clock.advance(2600)  # 500 ms played, then 2100 ms > GATE_MS idle
    assert pacer.ahead_ms < -2000

    await pacer.push_audio(_speech(100))
    assert sleep.calls == []
    assert pacer.request_open


async def test_flush_cancels_the_chunk_waiting_at_the_gate(clock):
    """Flush during the gate wait: the wait ends, the sink is interrupted,
    and the chunk that was waiting is dropped, not sent (it belongs to the
    turn the flush cancelled). The next turn's first chunk skips the gate."""
    sink = FakeSink()
    sleep = HangingSleep()
    pacer = _pacer(clock, sink, sleep)

    await pacer.push_audio(_speech(2000))
    await pacer.end_request("tts_audio_end")

    pcm_b = _speech(300)
    sender = asyncio.create_task(pacer.push_audio(pcm_b))
    for _ in range(10):
        await asyncio.sleep(0)
        if sleep.calls:
            break
    assert sleep.calls == [pytest.approx(4.0)]
    assert not sender.done()

    await pacer.flush()
    assert await asyncio.wait_for(sender, timeout=1.0) is PushResult.CANCELLED

    assert sink.interrupts == 1
    assert ("audio", pcm_b, False) not in sink.calls
    assert sink.ends == 1  # only A's; flush never sends end=True
    assert not pacer.request_open
    assert len(sleep.calls) == 1

    # The new turn's first chunk opens a request at once (waiver).
    pcm_c = _speech(100)
    assert await pacer.push_audio(pcm_c) is PushResult.SENT
    assert sink.calls[-1] == ("audio", pcm_c, False)
    assert pacer.request_open
    assert len(sleep.calls) == 1


async def test_flush_closes_open_request_locally(clock):
    sink = FakeSink()
    sleep = FakeSleep(clock)
    pacer = _pacer(clock, sink, sleep)
    await pacer.push_audio(_speech(2000))

    await pacer.flush()

    assert not pacer.request_open
    assert sink.ends == 0
    assert sink.interrupts == 1

    # The next request skips the gate exactly once.
    await pacer.push_audio(_speech(100))
    assert sleep.calls == []
    assert pacer.request_open
    await pacer.end_request("tts_audio_end")
    await pacer.push_audio(_speech(100))
    assert len(sleep.calls) == 1

    # A closed request does not pad after the flush.
    pacer2 = _pacer(clock, FakeSink())
    await pacer2.flush()
    await pacer2.tick()


async def test_flush_when_idle_still_interrupts(clock):
    sink = FakeSink()
    pacer = _pacer(clock, sink)
    await pacer.flush()
    assert sink.calls == [("interrupt",)]
    assert not pacer.request_open


async def test_padding_is_whole_samples(clock):
    rate = 22050
    sink = FakeSink()
    pacer = Pacer(sink, rate, clock=clock.now, sleep=FakeSleep(clock))
    await pacer.push_audio(_speech(100, rate))
    clock.advance(300)
    await pacer.tick()

    chunks = [
        c[1]
        for c in sink.calls
        if c[0] == "audio" and not c[2] and not c[1].strip(b"\x00")
    ]
    assert chunks
    chunk_bytes = rate * 100 // 1000 * 2
    for chunk in chunks:
        assert len(chunk) % 2 == 0
        assert len(chunk) == chunk_bytes


async def test_fractional_tick_is_rejected_at_construction(clock):
    # 44100 Hz * 1 ms is 44.1 samples: not representable.
    with pytest.raises(ValueError):
        Pacer(FakeSink(), 44100, clock=clock.now, pad_tick_ms=1)


async def test_session_reset_clears_playhead(clock):
    sink = FakeSink()
    sleep = FakeSleep(clock)
    pacer = _pacer(clock, sink, sleep)
    await pacer.push_audio(_speech(2000))
    assert pacer.request_open

    pacer.on_session_reset()

    assert not pacer.request_open
    assert pacer.ahead_ms == -math.inf
    # Nothing is sent for the abandoned request: no end=True, no padding.
    await pacer.tick()
    assert sink.ends == 0
    assert len(sink.calls) == 1

    # The next audio opens a request without waiting on the old playhead.
    await pacer.push_audio(_speech(100))
    assert sleep.calls == []
    assert pacer.request_open


async def test_session_reset_wakes_sender_waiting_at_gate(clock):
    """A reconnect while a sender waits at the gate: the old request's tail
    is gone with the old session, so the pending audio goes out at once."""
    sink = FakeSink()
    sleep = HangingSleep()
    pacer = _pacer(clock, sink, sleep)

    await pacer.push_audio(_speech(2000))
    await pacer.end_request("tts_audio_end")

    pcm_b = _speech(300)
    sender = asyncio.create_task(pacer.push_audio(pcm_b))
    for _ in range(10):
        await asyncio.sleep(0)
        if sleep.calls:
            break
    assert sleep.calls == [pytest.approx(4.0)]
    assert not sender.done()

    pacer.on_session_reset()
    await asyncio.wait_for(sender, timeout=1.0)

    assert sink.calls[-1] == ("audio", pcm_b, False)
    assert sink.interrupts == 0
    assert pacer.request_open
    assert pacer.ahead_ms == pytest.approx(300.0)
    # Woken by the reset, not by a second sleep.
    assert len(sleep.calls) == 1


async def test_failed_send_is_not_counted_and_forgets_the_request(clock):
    """A send the sink refuses (session gone) leaves no request open and no
    playhead, so the chunk can be retried on the next session without a
    gate wait; a refused padding chunk is not counted either."""
    sink = FakeSink()
    sleep = FakeSleep(clock)
    pacer = _pacer(clock, sink, sleep)
    sink.connected = False

    pcm = _speech(300)
    assert await pacer.push_audio(pcm) is PushResult.NOT_SENT
    assert sink.calls == [("audio", pcm, False)]
    assert not pacer.request_open
    assert pacer.ahead_ms == -math.inf

    sink.connected = True
    assert await pacer.push_audio(pcm) is PushResult.SENT
    assert sleep.calls == []
    assert pacer.request_open
    assert pacer.ahead_ms == pytest.approx(300.0)

    # Padding that the sink refuses does not move the playhead.
    sink.connected = False
    clock.advance(500)
    await pacer.tick()
    assert pacer.ahead_ms == pytest.approx(-200.0)
    assert len(sink.silence_chunks()) == 1


async def test_run_ticks_every_pad_tick(clock):
    sink = FakeSink()
    sleep = FakeSleep(clock)
    pacer = _pacer(clock, sink, sleep)
    await pacer.push_audio(_speech(200))

    task = asyncio.create_task(pacer.run())
    for _ in range(50):
        await asyncio.sleep(0)
        if sink.ends:
            break
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task

    assert all(s == pytest.approx(0.1) for s in sleep.calls)
    assert sink.ends == 1
    assert sink.audio_ms(silent=True) == 3000.0
