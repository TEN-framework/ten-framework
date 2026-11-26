#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

import pytest

from harness import ERROR, INTERRUPTED, NON_FATAL, OK, run
from mock_server import AUDIO, CHUNKS, FIRST, SAMPLE_RATE


def test_a_flush_interrupts_and_the_next_request_gets_a_new_session(
    server, config
):
    # Kalpa keeps sending audio until the connection is gone.
    server.chunks = [b"\x05\x00" * 240] * 60
    server.chunk_delay_s = 0.01
    heard = []

    async def script(rec):
        await rec.say("r1", "A long reply.")
        await rec.until(lambda: len(rec.of("pcm_frame")) >= 3)
        # The second flush finds the session that replaced r1's.
        await rec.flush(wait=False)
        await rec.flush()
        heard.append(bytes(rec.audio))
        # Nothing of r1 arrives after its flush.
        await asyncio.sleep(0.3)
        server.chunks = CHUNKS
        await rec.say("r2", "This one completes.")
        await rec.until(lambda: rec.ended("r2"))
        # A flush with nothing to interrupt.
        await rec.flush()
        await rec.say("r3", "So does this one.")

    rec = run(config, script, "r3")
    assert rec.ends() == [("r1", INTERRUPTED), ("r2", OK), ("r3", OK)]
    assert 0 < len(heard[0]) < 60 * 480
    assert bytes(rec.audio) == heard[0] + AUDIO * 2
    first = rec.of("tts_audio_end")[0]
    assert first["request_total_audio_duration_ms"] == (
        len(heard[0]) * 1000 // (SAMPLE_RATE * 2)
    )
    # r1 ends after its last frame and before the flush is reported done.
    names = rec.names()
    end, flush_end = names.index("tts_audio_end"), names.index("tts_flush_end")
    assert end < flush_end and "pcm_frame" not in names[end:flush_end]
    assert [f["flush_id"] for f in rec.of("tts_flush_end")] == ["flush"] * 3
    # Only r1's session was dropped. The one that replaced it served r2
    # and r3: the other two flushes left it alone.
    assert [session["utterances"] for session in server.sessions] == [
        ["A long reply."],
        ["This one completes.", "So does this one."],
    ]
    statuses = [s["current"] for s in rec.of("connection_status_changed")]
    assert " ".join(statuses[:5]) == (
        "connecting connected disconnected connecting connected"
    )
    assert not rec.errors()


@pytest.mark.parametrize("stop", [False, True])
@pytest.mark.parametrize("stage", ["connecting", "sending", "awaiting audio"])
def test_a_request_with_no_audio_yet_can_be_flushed_or_stopped(
    server, config, request, stage, stop
):
    if stage == "connecting":
        # The session does not open while r1 waits for it.
        server.hold_session = True
        waiting = lambda: server.sessions and server.sessions[0]["init"]
    elif stage == "sending":
        # r1's text is stuck on its way out, and its sender holds the lock.
        waiting = request.getfixturevalue("blocked_send").is_set
    else:
        # Kalpa has the text and has sent nothing yet.
        server.holds = [0]
        waiting = lambda: server.utterances

    async def script(rec):
        await rec.say("r1", "Never heard.")
        await rec.until(waiting)
        if not stop:
            await rec.flush()
            await rec.say("r2", "This one completes.")
            await rec.until(lambda: rec.ended("r2"))

    # With `stop` the extension is stopped here; `run` returns once it has.
    rec = run(config, script)
    assert not rec.errors()
    if stop:
        assert not rec.audio and not rec.ends()
        return
    assert rec.ends() == [("r1", INTERRUPTED), ("r2", OK)]
    first = rec.of("tts_audio_end")[0]
    assert first["request_event_interval_ms"] == 0
    assert first["request_total_audio_duration_ms"] == 0
    assert rec.before("tts_audio_end", "tts_flush_end")
    # Nothing of r1 came back, and r2 was not held up behind it.
    assert [e["request_id"] for e in rec.of("tts_audio_start")] == ["r2"]
    assert bytes(rec.audio) == AUDIO
    assert server.sessions[-1]["utterances"] == ["This one completes."]


@pytest.mark.parametrize("final", [False, True])
def test_text_that_arrives_after_its_flush_is_not_spoken(server, config, final):
    async def script(rec):
        await rec.say("r1", "The start of a reply,", end=False)
        await asyncio.sleep(0.2)
        await rec.flush()
        # A piece of r1 that was on its way when the flush came.
        await rec.say("r1", " and a late piece of it.", end=final)
        # Once r1's text has ended, its id names a new request. Until then
        # this is more of its late text, and ends it.
        await rec.say("r1", "A message under the same id.")
        await rec.say("r2", "The next request.")

    rec = run(config, script, "r2")
    # Neither the unfinished text nor the late piece became speech, and
    # the late piece did not hold up what came after it.
    again = [("r1", OK)] if final else []
    assert rec.ends() == [("r1", INTERRUPTED)] + again + [("r2", OK)]
    said = ["A message under the same id."] * final + ["The next request."]
    assert server.utterances == said
    assert bytes(rec.audio) == AUDIO * len(said) and not rec.errors()


def test_an_id_used_again_is_a_new_request_that_a_flush_can_interrupt(
    server, config
):
    # The example agent sends every message of a turn under one id.
    texts = ["Heard in full.", "Interrupted.", "Fails.", "Heard in full too."]

    async def script(rec):
        await rec.say("r1", texts[0])
        await rec.until(lambda: rec.ended("r1"))
        server.holds = [1]
        await rec.say("r1", texts[1])
        await rec.until(lambda: len(rec.audio) > len(AUDIO))
        await rec.flush()
        # The text of the interrupted and of the failed message had ended,
        # so what follows under their id is not their late input.
        server.responses = ["error"]
        await rec.say("r1", texts[2])
        await rec.until(rec.errors)
        await rec.say("r1", texts[3])
        await rec.until(lambda: len(rec.ends()) == 3)

    rec = run(config, script)
    assert server.utterances == texts
    assert rec.ends() == [("r1", OK), ("r1", INTERRUPTED), ("r1", OK)]
    assert rec.errors() == [("r1", NON_FATAL, "inference_error")]
    assert bytes(rec.audio) == AUDIO + FIRST + AUDIO
    assert all(e["metadata"]["turn_id"] == 1 for e in rec.of("tts_audio_end"))


@pytest.mark.parametrize("finished", [True, False])
def test_a_message_that_starts_while_its_id_is_speaking_joins_the_request(
    server, config, finished
):
    server.chunk_delay_s = 0.05

    async def script(rec):
        await rec.say("r1", "The first message.")
        await rec.until(lambda: rec.audio)
        # The next message of the turn starts before the first was heard.
        await rec.say("r1", " The second message", end=False)
        # Kalpa finishes speaking the first one.
        await rec.until(lambda: len(rec.audio) == len(AUDIO))
        await asyncio.sleep(0.3)
        if finished:
            await rec.say("r1", " ends here.")
        else:
            await rec.flush()
        await rec.say("r2", "The next request.")

    rec = run(config, script, "r2")
    # r1 stays open until the second message ends, so a flush interrupts
    # it and none of its text is left on the session for r2.
    second = [" The second message ends here."] * finished
    assert server.utterances == (
        ["The first message."] + second + ["The next request."]
    )
    assert rec.ends() == [("r1", OK if finished else INTERRUPTED), ("r2", OK)]
    assert bytes(rec.audio) == AUDIO * (len(second) + 2)


def test_an_id_used_again_while_it_is_ending_waits_and_can_be_interrupted(
    server, config, slow_end
):
    # r1 fails after its text ended, and its last events take a while.
    server.responses = ["error"]

    async def script(rec):
        await rec.say("r1", "Fails after its text ended.")
        await rec.until(slow_end.is_set)
        server.holds = [1]
        await rec.say("r1", "The next message of the turn.")
        await rec.until(lambda: rec.audio)
        await rec.flush()
        await rec.say("r2", "The next request.")

    rec = run(config, script, "r2")
    # The message waited for the session that replaced r1's, and the base
    # class took it for a request in progress, so the flush reached it.
    assert rec.errors() == [("r1", NON_FATAL, "inference_error")]
    assert rec.ends() == [("r1", INTERRUPTED), ("r2", OK)]
    assert rec.before("tts_audio_end", "tts_flush_end")
    assert bytes(rec.audio) == FIRST + AUDIO


@pytest.mark.parametrize("fault", ["flush not heard", "report fails"])
def test_an_interruption_that_goes_wrong_still_frees_the_next_request(
    server, config, monkeypatch, running, fault
):
    extension = running.extension.KalpaTTSExtension
    failures = [RuntimeError("the report was not sent")]
    report = extension.on_disconnected

    async def failing_report(self, *args, **kwargs):
        if failures:
            raise failures.pop()
        await report(self, *args, **kwargs)

    async def unheard(self):
        pass

    if fault == "report fails":
        monkeypatch.setattr(extension, "on_disconnected", failing_report)
    else:
        # A flush that never reaches the extension leaves r1 open, and r2
        # interrupts it when it starts.
        monkeypatch.setattr(extension, "cancel_tts", unheard)
    server.holds = [1]

    async def script(rec):
        await rec.say("r1", "Interrupted.")
        await rec.until(lambda: rec.audio)
        await rec.flush()
        await rec.say("r2", "This one completes.")

    rec = run(config, script, "r2")
    # r1 still ended, on a session of its own, and r2 found a new one.
    assert not failures or fault != "report fails"
    assert rec.ends() == [("r1", INTERRUPTED), ("r2", OK)]
    assert bytes(rec.audio) == FIRST + AUDIO
    assert server.sessions[-1]["utterances"] == ["This one completes."]
    assert len(server.sessions) == 2 and not rec.errors()


def test_late_events_of_a_replaced_connection_do_not_reach_the_next_request(
    server, config, monkeypatch, running
):
    # Every client the extension starts, with the loop it runs on.
    clients = []
    kalpa_tts = running.kalpa_tts
    start = kalpa_tts.KalpaTTSClient.start

    def recorded_start(self):
        clients.append((self, asyncio.get_running_loop()))
        start(self)

    monkeypatch.setattr(kalpa_tts.KalpaTTSClient, "start", recorded_start)
    server.holds = [1]

    async def script(rec):
        await rec.say("r1", "Interrupted.")
        await rec.until(lambda: rec.audio)
        await rec.flush()
        server.holds = [1]
        await rec.say("r2", "This one completes.")
        await rec.until(lambda: len(rec.audio) == 2 * len(FIRST))
        # What r1's connection could still have delivered while r2 is
        # speaking: audio, the end of its response, an error and its loss.
        old, loop = clients[0]
        late = kalpa_tts.KalpaTTSError("inference_error", "Too late.")
        for name, args in (
            ("on_audio", (b"\x7f\x00" * 480, 0.0)),
            ("on_response_done", ("completed",)),
            ("on_response_error", (late,)),
            ("on_connection_failed", (late, False)),
        ):
            deliver = getattr(old, "_emit")(name, *args)
            asyncio.run_coroutine_threadsafe(deliver, loop).result(5)
        await asyncio.sleep(0.2)
        server.release()

    rec = run(config, script, "r2")
    # None of it touched r2: no extra audio, no early end, no error.
    assert len(clients) == 2
    assert rec.ends() == [("r1", INTERRUPTED), ("r2", OK)]
    assert bytes(rec.audio) == FIRST + AUDIO and not rec.errors()


@pytest.mark.parametrize("stop", [False, True])
def test_a_request_that_is_ending_gets_its_last_events_out_first(
    server, config, slow_end, monkeypatch, running, stop
):
    # What the extension does is recorded here: the tester is gone by the
    # time the extension stops.
    order = []
    extension = running.extension.KalpaTTSExtension
    for name in ("send_tts_audio_end", "on_disconnected"):

        def recorded(self, _name=name, _method=getattr(extension, name), **kw):
            order.append((_name, kw.get("request_id") or kw.get("message")))
            return _method(self, **kw)

        monkeypatch.setattr(extension, name, recorded)
    # r1 fails after some audio, and its last events take a while.
    server.holds = [1]
    server.responses = ["error_mid"]

    async def script(rec):
        await rec.say("r1", "Fails part-way.")
        await rec.until(lambda: rec.audio)
        server.release()
        await rec.until(slow_end.is_set)
        if not stop:
            await rec.flush()
            await rec.say("r2", "This one completes.")
            await rec.until(lambda: rec.ended("r2"))

    # With `stop` the extension is stopped here, while r1 is ending.
    rec = run(config, script)
    if stop:
        # r1 ended before the extension reported that it had stopped.
        assert order[-2:] == [
            ("send_tts_audio_end", "r1"),
            ("on_disconnected", "stopped"),
        ]
        return
    # r1 ends once, for its own reason, before the flush is reported done.
    assert rec.ends() == [("r1", ERROR), ("r2", OK)]
    assert rec.before("tts_audio_end", "tts_flush_end")
    assert rec.errors() == [("r1", NON_FATAL, "inference_error")]
    assert bytes(rec.audio) == FIRST + AUDIO
