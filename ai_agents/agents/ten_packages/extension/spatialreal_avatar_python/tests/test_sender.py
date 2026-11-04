#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Unit tests for the audio sender: queue ordering, flush and session state.

The sender runs against the real ``Pacer`` and a recording sink, with the
``FakeClock`` and a sleep the test releases by hand, so the gate wait is
held open for as long as a test needs to queue more behind it.
"""

import asyncio

import pytest

from spatialreal_avatar_python.pacer import Pacer
from spatialreal_avatar_python.sender import AudioSender, End, InputQueue, Turn

pytestmark = pytest.mark.asyncio

RATE = 24000


async def settle(rounds: int = 50) -> None:
    """Let every ready task step run."""
    for _ in range(rounds):
        await asyncio.sleep(0)


class FakeSink:
    """Records every call the pacer makes, in order."""

    def __init__(self):
        self.calls: list[tuple] = []

    async def send_audio(self, pcm: bytes, end: bool = False) -> bool:
        self.calls.append(("audio", pcm, end))
        return True

    async def interrupt(self) -> bool:
        self.calls.append(("interrupt",))
        return True

    @property
    def ends(self) -> int:
        return sum(1 for c in self.calls if c[0] == "audio" and c[2])

    @property
    def interrupts(self) -> int:
        return sum(1 for c in self.calls if c[0] == "interrupt")

    def audio(self) -> list[bytes]:
        """Non-end PCM payloads, in order."""
        return [c[1] for c in self.calls if c[0] == "audio" and not c[2]]


class FakeLog:
    def __init__(self):
        self.lines: list[tuple[str, str]] = []

    def log_debug(self, msg: str) -> None:
        self.lines.append(("debug", msg))

    def log_info(self, msg: str) -> None:
        self.lines.append(("info", msg))

    def log_warn(self, msg: str) -> None:
        self.lines.append(("warn", msg))

    def log_error(self, msg: str) -> None:
        self.lines.append(("error", msg))

    def level(self, name: str) -> list[str]:
        return [msg for lvl, msg in self.lines if lvl == name]


class ControlledSleep:
    """Parks every sleep until the test releases it.

    ``release`` wakes the oldest sleeper and advances the clock by what it
    asked for, so the pacer's gate re-check sees the time as elapsed.
    """

    def __init__(self, clock):
        self.clock = clock
        self.calls: list[float] = []
        self._pending: list[tuple[float, asyncio.Future]] = []

    async def __call__(self, seconds: float) -> None:
        self.calls.append(seconds)
        fut = asyncio.get_running_loop().create_future()
        entry = (seconds, fut)
        self._pending.append(entry)
        try:
            await fut
        finally:
            if entry in self._pending:
                self._pending.remove(entry)

    def release(self) -> None:
        seconds, fut = self._pending.pop(0)
        self.clock.advance(seconds * 1000)
        if not fut.done():
            fut.set_result(None)


def _speech(ms: int, rate: int = RATE) -> bytes:
    """Non-silent int16 PCM of ``ms`` (every sample is 0x0101)."""
    return b"\x01\x01" * (rate * ms // 1000)


class FakeClient:
    """A session client whose sessions the test opens and drops by hand.

    ``send_audio`` answers False once the session is gone, as the real
    client does; what each session received is kept per connection id.
    """

    def __init__(self):
        self.connected = False
        self.refuse_sends = False
        self.sessions: dict[str, list[tuple[bytes, bool]]] = {}
        self.interrupts = 0
        self._current = ""

    def connect(self, connection_id: str) -> None:
        self.connected = True
        self._current = connection_id
        self.sessions[connection_id] = []

    def disconnect(self) -> None:
        self.connected = False
        self._current = ""

    async def send_audio(self, pcm: bytes, end: bool = False) -> bool:
        if not self.connected or self.refuse_sends:
            return False
        self.sessions[self._current].append((pcm, end))
        return True

    async def interrupt(self) -> bool:
        if not self.connected:
            return False
        self.interrupts += 1
        return True


class Harness:
    """A sender over the real pacer, with the sender task running."""

    def __init__(self, clock, *, sink=None, sleep=None, max_queue_ms=10_000):
        self.sink = sink or FakeSink()
        self.log = FakeLog()
        self.sleep = sleep or ControlledSleep(clock)
        self.states: list[bool] = []
        self.pacer = Pacer(self.sink, RATE, clock=clock.now, sleep=self.sleep)
        self.sender = AudioSender(
            self.pacer,
            RATE,
            self.log,
            on_request_state=self.states.append,
            max_queue_ms=max_queue_ms,
        )
        self.task: asyncio.Task | None = None

    async def __aenter__(self) -> "Harness":
        self.task = asyncio.create_task(self.sender.run())
        return self

    async def __aexit__(self, *exc) -> None:
        self.task.cancel()
        try:
            await self.task
        except asyncio.CancelledError:
            pass


# -- ordering -----------------------------------------------------------------


async def test_end_follows_its_own_audio_behind_the_gate(clock):
    """Regression: a second request plus its end queued while the first
    request's gate holds the sender. The end must not overtake the PCM."""
    async with Harness(clock) as h:
        h.sender.session_up()
        pcm_a = _speech(2000)
        h.sender.put_audio(pcm_a, RATE)
        h.sender.put_end(End("tts_audio_end reason=1", tts_reason=1))
        await settle()
        assert h.sink.calls == [("audio", pcm_a, False), ("audio", b"", True)]
        assert not h.sender.request_open

        # Request B arrives complete while A is still less than GATE_MS
        # past drained: its first frame waits at the gate.
        pcm_b1, pcm_b2 = _speech(300), _speech(200)
        h.sender.put_audio(pcm_b1, RATE)
        h.sender.put_audio(pcm_b2, RATE)
        h.sender.put_end(End("finalize"))
        await settle()
        assert h.sleep.calls == [pytest.approx(4.0)]
        # Nothing of B has gone out, and its end is still behind its audio.
        assert h.sink.ends == 1
        assert len(h.sink.calls) == 2
        assert h.sender.queued == 2

        h.sleep.release()
        await settle()

        assert h.sink.calls[2:] == [
            ("audio", pcm_b1, False),
            ("audio", pcm_b2, False),
            ("audio", b"", True),
        ]
        assert not h.sender.request_open
        assert h.states[-1] is False
        assert "request closed: finalize" in h.log.level("info")


async def test_end_waits_for_audio_queued_before_the_session_is_up(clock):
    async with Harness(clock) as h:
        pcm = _speech(500)
        h.sender.put_audio(pcm, RATE)
        h.sender.put_end(End("finalize"))
        await settle()
        # The frame is parked on the session; the marker waits behind it.
        assert h.sink.calls == []
        assert h.sender.queued == 1

        h.sender.session_up()
        await settle()

        assert h.sink.calls == [("audio", pcm, False), ("audio", b"", True)]
        assert not h.sender.request_open


async def test_tts_audio_end_marker_applies_the_reason_rules(clock):
    async with Harness(clock) as h:
        h.sender.session_up()
        h.sender.put_audio(_speech(100), RATE)
        await settle()
        assert h.sender.request_open

        # A flush, then the new turn, then the late reason 2 that belongs to
        # the pre-flush request: the marker is reached and ignored (D5).
        await h.sender.flush()
        h.sender.put_audio(_speech(100), RATE)
        h.sender.put_end(End("tts_audio_end reason=2", tts_reason=2))
        await settle()
        assert h.sender.request_open
        assert h.sink.ends == 0
        assert any(
            msg.startswith("end ignored: tts_audio_end reason=2")
            for msg in h.log.level("debug")
        )

        h.sender.put_end(End("tts_audio_end reason=1", tts_reason=1))
        await settle()
        assert not h.sender.request_open
        assert h.sink.ends == 1


async def test_end_with_nothing_open_is_a_noop(clock):
    async with Harness(clock) as h:
        h.sender.session_up()
        h.sender.put_end(End("finalize"))
        await settle()
        assert h.sink.calls == []
        # session_up and the marker each report the state; never "open".
        assert h.states == [False, False]


# -- flush ----------------------------------------------------------------------


@pytest.mark.parametrize("reason", [1, 2, 3, -1, None])
async def test_old_turn_end_cannot_close_the_turn_after_flush(clock, reason):
    async with Harness(clock) as h:
        h.sender.session_up()
        a, b = Turn("session", 1), Turn("session", 2)
        h.sender.put_audio(_speech(100), RATE, a)
        await settle()
        await h.sender.flush()
        h.sender.put_audio(_speech(100), RATE, b)
        h.sender.put_end(End("late A", reason, a, "request-a"))
        await settle()
        assert h.sender.request_open
        assert h.sink.ends == 0
        h.sender.put_end(End("B", 1, b, "request-b"))
        await settle()
        assert not h.sender.request_open
        assert h.sink.ends == 1


async def test_identified_interrupted_end_closes_the_current_turn(clock):
    async with Harness(clock) as h:
        h.sender.session_up()
        await h.sender.flush()
        b = Turn("session", 2)
        h.sender.put_audio(_speech(100), RATE, b)
        h.sender.put_end(End("B interrupted", 2, b, "request-b"))
        await settle()
        assert not h.sender.request_open
        assert h.sink.ends == 1


async def test_late_cancelled_pcm_cannot_restart_or_contaminate_next_turn(
    clock,
):
    async with Harness(clock) as h:
        h.sender.session_up()
        a, b = Turn("session", 1), Turn("session", 2)
        first, cancelled, second = _speech(100), _speech(200), _speech(300)
        h.sender.put_audio(first, RATE, a)
        await settle()
        await h.sender.flush()
        h.sender.put_audio(cancelled, RATE, a)
        h.sender.put_audio(second, RATE, b)
        h.sender.put_audio(cancelled, RATE, a)
        h.sender.put_end(End("B", 1, b, "request-b"))
        await settle()
        assert h.sink.audio() == [first, second]
        assert h.sink.ends == 1


async def test_duplicate_end_does_not_close_another_request_in_the_same_turn(
    clock,
):
    async with Harness(clock) as h:
        h.sender.session_up()
        turn = Turn("session", 1)
        h.sender.put_audio(_speech(100), RATE, turn)
        h.sender.put_end(End("A", 1, turn, "request-a"))
        await settle()
        h.sender.put_audio(_speech(200), RATE, turn)
        h.sender.put_end(End("A duplicate", 1, turn, "request-a"))
        await settle()
        h.sleep.release()
        await settle()
        assert h.sender.request_open
        assert h.sink.ends == 1
        h.sender.put_end(End("B", 1, turn, "request-b"))
        await settle()
        assert h.sink.ends == 2


async def test_flush_retires_queued_and_inflight_turns(clock):
    async with Harness(clock) as h:
        a, b = Turn("session", 1), Turn("session", 2)
        h.sender.put_audio(_speech(100), RATE, a)
        h.sender.put_audio(_speech(100), RATE, b)
        await settle()
        await h.sender.flush()
        h.sender.session_up()
        h.sender.put_audio(_speech(100), RATE, a)
        h.sender.put_audio(_speech(100), RATE, b)
        h.sender.put_audio(_speech(300), RATE, Turn("session", 3))
        await settle()
        assert h.sink.audio() == [_speech(300)]


async def test_new_turn_closes_predecessor_and_ignores_its_late_events(clock):
    async with Harness(clock) as h:
        h.sender.session_up()
        a, b = Turn("session", 1), Turn("session", 2)
        first, second = _speech(100), _speech(200)
        h.sender.put_audio(first, RATE, a)
        h.sender.put_audio(second, RATE, b)
        h.sender.put_audio(_speech(300), RATE, a)
        h.sender.put_end(End("late A", 1, a, "request-a"))
        await settle()
        assert h.sink.calls == [("audio", first, False), ("audio", b"", True)]
        h.sleep.release()
        await settle()
        assert h.sink.audio() == [first, second]
        assert h.sender.request_open
        assert h.sink.ends == 1
        h.sender.put_end(End("B", 1, b, "request-b"))
        await settle()
        assert h.sink.ends == 2


async def test_turn_zero_in_another_session_survives_flush(clock):
    async with Harness(clock) as h:
        h.sender.session_up()
        a, b = Turn("old-session", 0), Turn("new-session", 0)
        h.sender.put_audio(_speech(100), RATE, a)
        h.sender.put_end(End("A", 1, a, "request-0"))
        await settle()
        await h.sender.flush()
        h.sender.put_audio(_speech(200), RATE, b)
        h.sender.put_end(End("B", 1, b, "request-0"))
        await settle()
        assert h.sink.audio() == [_speech(100), _speech(200)]
        assert h.sink.ends == 2


async def test_new_turn_waits_until_the_old_interrupt_has_been_sent(clock):
    class SlowInterrupt(FakeSink):
        def __init__(self):
            super().__init__()
            self.started = asyncio.Event()
            self.release = asyncio.Event()

        async def interrupt(self):
            self.started.set()
            await self.release.wait()
            return await super().interrupt()

    sink = SlowInterrupt()
    async with Harness(clock, sink=sink) as h:
        h.sender.session_up()
        a, b = Turn("session", 1), Turn("session", 2)
        first, second = _speech(100), _speech(200)
        h.sender.put_audio(first, RATE, a)
        await settle()
        flushing = asyncio.create_task(h.sender.flush())
        await asyncio.wait_for(sink.started.wait(), timeout=1)
        try:
            h.sender.put_audio(second, RATE, b)
            h.sender.put_end(End("B", 1, b, "request-b"))
            await settle()
            assert sink.audio() == [first]
        finally:
            sink.release.set()
            await asyncio.wait_for(flushing, timeout=1)
        await settle()
        assert sink.calls == [
            ("audio", first, False),
            ("interrupt",),
            ("audio", second, False),
            ("audio", b"", True),
        ]


async def test_unseen_old_turn_first_pcm_cannot_supersede_new_turn(clock):
    async with Harness(clock) as h:
        h.sender.session_up()
        await h.sender.flush()  # A has not delivered its first PCM yet.
        a, b = Turn("session", 1), Turn("session", 2)
        first, second = _speech(100), _speech(300)
        h.sender.put_audio(first, RATE, b)
        # Queue A before B has been processed, to exercise dequeue checks.
        h.sender.put_audio(_speech(200), RATE, a)
        h.sender.put_audio(second, RATE, b)
        h.sender.put_end(End("B", 1, b, "request-b"))
        await settle()
        assert h.sink.audio() == [first, second]
        assert h.sink.ends == 1
        assert not h.sender.request_open


async def test_turn_order_check_preserves_earlier_audio_already_in_queue(clock):
    async with Harness(clock) as h:
        h.sender.session_up()
        a, b = Turn("session", 1), Turn("session", 2)
        first, second = _speech(100), _speech(200)
        h.sender.put_audio(first, RATE, a)
        h.sender.put_end(End("A", 1, a, "request-a"))
        h.sender.put_audio(second, RATE, b)
        h.sender.put_end(End("B", 1, b, "request-b"))
        await settle()
        assert h.sink.audio() == [first]
        h.sleep.release()
        await settle()
        assert h.sink.audio() == [first, second]
        assert h.sink.ends == 2


@pytest.mark.parametrize(
    "metadata", [None, [], {}, {"turn_id": True}, {"turn_id": "1"}]
)
def test_invalid_turn_metadata_is_unscoped(metadata):
    assert Turn.from_metadata(metadata) is None


def test_turn_zero_and_session_identity_are_preserved():
    assert Turn.from_metadata({"session_id": "s", "turn_id": 0}) == Turn("s", 0)
    assert Turn("s", 0) != Turn("another-session", 0)


async def test_flush_drops_queued_audio_and_markers(clock):
    async with Harness(clock) as h:
        h.sender.put_audio(_speech(500), RATE)
        h.sender.put_audio(_speech(500), RATE)
        h.sender.put_end(End("finalize"))
        await settle()

        await h.sender.flush()
        assert h.sender.queued == 0
        assert h.sink.calls == [("interrupt",)]

        # The frame that was already parked on the session is dropped too.
        h.sender.session_up()
        await settle()
        assert h.sink.calls == [("interrupt",)]
        assert not h.sender.request_open


# -- queue bound ----------------------------------------------------------------


async def test_queue_bound_drops_oldest_audio_but_never_a_marker():
    queue = InputQueue(max_ms=1000)
    first, second = _speech(600), _speech(600)
    assert queue.put_audio(first, RATE) is False
    queue.put_end(End("finalize"))
    assert queue.put_audio(second, RATE) is True
    assert queue.queued_ms == pytest.approx(600.0)

    assert await queue.get() == End("finalize")
    item = await queue.get()
    assert item.pcm == second
    assert len(queue) == 0
    assert queue.queued_ms == 0.0


async def test_sender_warns_once_per_overflow(clock):
    async with Harness(clock, max_queue_ms=1000) as h:
        for _ in range(4):
            h.sender.put_audio(_speech(400), RATE)
        await settle()
        assert len(h.log.level("warn")) == 1


async def test_flush_drops_the_frame_waiting_at_the_gate(clock):
    """The sender checked the flush generation before entering the pacer; a
    flush that lands while the frame waits at the gate must still drop it,
    so the cancelled turn does not restart after the interrupt."""
    async with Harness(clock) as h:
        h.sender.session_up()
        pcm_a = _speech(2000)
        h.sender.put_audio(pcm_a, RATE)
        h.sender.put_end(End("finalize"))
        await settle()
        assert h.sink.ends == 1

        pcm_b = _speech(300)
        h.sender.put_audio(pcm_b, RATE)
        await settle()
        assert h.sleep.calls == [pytest.approx(4.0)]
        assert h.sink.audio() == [pcm_a]

        await h.sender.flush()
        await settle()
        assert h.sink.audio() == [pcm_a]
        assert h.sink.interrupts == 1
        assert not h.sender.request_open

        # The next turn goes out at once, after the interrupt.
        pcm_c = _speech(100)
        h.sender.put_audio(pcm_c, RATE)
        await settle()
        assert h.sink.audio() == [pcm_a, pcm_c]
        assert h.sink.calls.index(("interrupt",)) < h.sink.calls.index(
            ("audio", pcm_c, False)
        )
        assert h.sender.request_open
        assert len(h.sleep.calls) == 1


# -- reconnect ------------------------------------------------------------------


async def test_frame_waiting_at_the_gate_survives_a_reconnect(clock):
    """The session drops while a frame waits at the gate: the woken send is
    refused, the frame is held, and the replacement session receives it."""
    client = FakeClient()
    async with Harness(clock, sink=client) as h:
        client.connect("conn-0")
        h.sender.session_up()
        pcm_a = _speech(2000)
        h.sender.put_audio(pcm_a, RATE)
        h.sender.put_end(End("finalize"))
        await settle()
        assert client.sessions["conn-0"] == [(pcm_a, False), (b"", True)]

        pcm_b = _speech(300)
        h.sender.put_audio(pcm_b, RATE)
        await settle()
        assert h.sleep.calls == [pytest.approx(4.0)]

        # The reset wakes the gate; the send that follows is refused because
        # the session is gone. Nothing is counted and the frame is held.
        client.disconnect()
        h.sender.session_down()
        await settle()
        assert client.sessions["conn-0"] == [(pcm_a, False), (b"", True)]
        assert not h.sender.request_open
        assert h.sender.queued == 0

        client.connect("conn-1")
        h.sender.session_up()
        await settle()
        assert client.sessions["conn-1"] == [(pcm_b, False)]
        assert h.sender.request_open
        assert h.pacer.ahead_ms == pytest.approx(300.0)
        assert len(h.sleep.calls) == 1


async def test_frame_held_for_reconnect_is_dropped_by_a_flush(clock):
    client = FakeClient()
    async with Harness(clock, sink=client) as h:
        client.connect("conn-0")
        h.sender.session_up()
        h.sender.put_audio(_speech(2000), RATE)
        h.sender.put_end(End("finalize"))
        await settle()
        h.sender.put_audio(_speech(300), RATE)
        await settle()
        assert len(h.sleep.calls) == 1

        client.disconnect()
        h.sender.session_down()
        await settle()
        await h.sender.flush()

        client.connect("conn-1")
        h.sender.session_up()
        await settle()
        assert client.sessions["conn-1"] == []
        assert not h.sender.request_open


async def test_frame_refused_while_the_session_is_up_is_dropped(clock):
    """A send that fails without the session going down is not retried: the
    frame is dropped with a warning and the sender moves on."""
    client = FakeClient()
    async with Harness(clock, sink=client) as h:
        client.connect("conn-0")
        h.sender.session_up()
        client.refuse_sends = True
        h.sender.put_audio(_speech(300), RATE)
        await settle()
        assert client.sessions["conn-0"] == []
        assert not h.sender.request_open
        assert len(h.log.level("warn")) == 1

        client.refuse_sends = False
        pcm = _speech(100)
        h.sender.put_audio(pcm, RATE)
        await settle()
        assert client.sessions["conn-0"] == [(pcm, False)]
        assert h.sender.request_open
