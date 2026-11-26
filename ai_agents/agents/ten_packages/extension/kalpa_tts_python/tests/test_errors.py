#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

import pytest

from harness import ERROR, FATAL, NON_FATAL, OK, run
from mock_server import AUDIO, CHUNKS, FIRST

SECRET = "sk-live-not-a-real-key-0123456789"
# 8,850 characters, where an utterance holds 8,000.
LONG = "This sentence is repeated to fill more than one utterance. " * 150


@pytest.fixture
def quick(monkeypatch, running):
    """Failed connections are tried again at once."""
    monkeypatch.setattr(running.kalpa_tts, "RECONNECT_BASE_DELAY_S", 0.01)


@pytest.mark.parametrize(
    "params, refusal, error_type, named",
    [
        ({"api_key": ""}, None, "invalid_config", "api_key"),
        ({"sample_rate": 12345}, None, "invalid_config", "sample_rate"),
        (
            {"base_url": "https://kalpa.test"},
            None,
            "invalid_config",
            "endpoint",
        ),
        ({"api_key": SECRET}, None, "authentication_error", None),
        ({}, ("allowance_exhausted", 4402), "allowance_exhausted", None),
    ],
)
def test_what_no_retry_can_fix_is_fatal_and_not_retried(
    server, config, params, refusal, error_type, named
):
    config["params"].update(params)
    server.refuse_sessions = refusal

    async def script(rec):
        await rec.until(rec.errors)
        await rec.say("r1", "Hello world.")
        await rec.until(lambda: len(rec.errors()) == 2)

    rec = run(config, script)
    # Once at startup and once for the request.
    assert rec.errors() == [("", FATAL, error_type), ("r1", FATAL, error_type)]
    assert not rec.audio and not rec.ends()
    # A setting that cannot work is named, and nothing reaches Kalpa. A
    # refused session is not tried a second time.
    assert all(named is None or named in e["message"] for e in rec.of("error"))
    assert len(server.sessions) == (0 if named else 1)
    # The key is not echoed in anything the extension emits.
    assert SECRET not in repr(rec.events)


@pytest.mark.parametrize(
    "setup, error_types, end, heard",
    [
        ({"responses": ["error"]}, ["inference_error"], None, b""),
        ({"responses": ["cancel"]}, ["response_cancelled"], None, b""),
        ({"responses": ["corrupt"]}, ["protocol_error"], None, b""),
        ({"responses": ["error_mid"]}, ["inference_error"], ERROR, FIRST),
        ({"responses": ["drop_mid"]}, ["connection_closed"], ERROR, FIRST),
        # Kalpa takes the text and sends nothing, or stops part-way.
        ({"holds": [0]}, ["response_timeout"], None, b""),
        ({"holds": [1]}, ["response_timeout"], ERROR, FIRST),
        # Nothing of r1 was heard, so it is sent again and completes.
        ({"drop_on_text": 1}, ["connection_closed"], OK, AUDIO),
        ({"responses": ["overload"]}, ["overloaded"], OK, AUDIO),
        # Sent three times in all, then failed.
        ({"responses": ["overload"] * 3}, ["overloaded"] * 3, None, b""),
        # Frames that are not Kalpa messages are ignored.
        ({"responses": ["noise"]}, [], OK, AUDIO),
    ],
)
def test_a_request_in_trouble_ends_once_and_the_next_one_completes(
    server, config, monkeypatch, running, setup, error_types, end, heard
):
    monkeypatch.setattr(running.extension, "RESPONSE_TIMEOUT_S", 0.5)
    for name, value in setup.items():
        setattr(server, name, value)
    texts = ["This request is in trouble.", "The next request."]

    async def script(rec):
        await rec.say("r1", texts[0])
        await rec.say("r2", texts[1])

    rec = run(config, script, "r2")
    # Each failure is reported for r1 and none is fatal. A request that
    # failed before any audio has no audio end.
    assert rec.errors() == [("r1", NON_FATAL, kind) for kind in error_types]
    assert rec.ends() == ([("r1", end)] if end else []) + [("r2", OK)]
    # The error is reported before the audio end it explains.
    assert end != ERROR or rec.before("error", "tts_audio_end")
    # What was heard of r1 is not repeated, and r2 is whole.
    assert bytes(rec.audio) == heard + AUDIO
    # r2 ran on a session that holds nothing of a request that failed.
    assert server.sessions[-1]["utterances"] == texts[end != OK :]
    if "overloaded" in error_types:
        # Kalpa was at capacity: each new connection waits about a second.
        opened = [session["opened_at"] for session in server.sessions]
        assert all(b - a >= 1.0 for a, b in zip(opened, opened[1:]))


@pytest.mark.parametrize(
    "how, error_type, sent_again",
    [("overload", "overloaded", 1), ("error_early", "inference_error", 0)],
)
def test_a_failure_before_the_flush_ends_the_connection(
    server, config, how, error_type, sent_again
):
    server.responses = [how]
    first = "This sentence is long enough to start speaking before the flush."

    async def script(rec):
        await rec.say("r1", first, end=False)
        await rec.say("r1", " And a little more.")
        await rec.say("r2", "The next request.")

    rec = run(config, script, "r2")
    # Kalpa ended the first connection at the failure. Refused as
    # overloaded, nothing was generated, so r1 was sent again in full on a
    # new connection. A generation that failed is not tried again.
    assert rec.errors() == [("r1", NON_FATAL, error_type)]
    assert rec.ends() == [("r1", OK)] * sent_again + [("r2", OK)]
    said = [first + " And a little more."] * sent_again + ["The next request."]
    assert [session["utterances"] for session in server.sessions] == [[], said]
    assert bytes(rec.audio) == AUDIO * len(said)


def test_a_request_longer_than_kalpa_can_queue_is_refused(
    server, config, monkeypatch, running
):
    monkeypatch.setattr(running.extension, "MAX_UTTERANCES", 2)

    async def script(rec):
        # 17,700 characters: a third utterance, where two are allowed.
        await rec.say("r1", LONG * 2)
        await rec.until(rec.errors)
        # Its text had ended, so this is a new message under its id.
        await rec.say("r1", "A message under the same id.")
        # Two utterances are within the limit.
        await rec.say("r2", LONG)

    rec = run(config, script, "r2")
    assert rec.errors() == [("r1", NON_FATAL, "request_too_long")]
    assert rec.ends() == [("r1", OK), ("r2", OK)]
    assert server.utterances[0] == "A message under the same id."
    assert (
        len(server.utterances) == 3 and "".join(server.utterances[1:]) == LONG
    )


def test_input_for_a_request_that_failed_is_dropped_after_its_last_events(
    server, config, slow_end
):
    # r1 fails while its text is still arriving, and its last events take
    # a while: the session is replaced among them.
    server.responses = ["error"]

    async def script(rec):
        # The first utterance ends, and fails, before the request does.
        await rec.say("r1", LONG, end=False)
        await rec.until(slow_end.is_set)
        await rec.say("r1", " Late text for it.")
        await rec.say("r2", "The next request.")

    rec = run(config, script, "r2")
    # The late text is not spoken, and r2 found the new session in place.
    assert rec.errors() == [("r1", NON_FATAL, "inference_error")]
    assert rec.ends() == [("r1", ERROR), ("r2", OK)]
    assert server.sessions[-1]["utterances"] == ["The next request."]
    assert bytes(rec.audio) == FIRST + AUDIO


def test_the_deadline_runs_only_while_kalpa_is_silent_and_owes_audio(
    server, config, monkeypatch, running
):
    monkeypatch.setattr(running.extension, "RESPONSE_TIMEOUT_S", 1.0)
    monkeypatch.setattr(running.kalpa_tts, "KEEPALIVE_INTERVAL_S", 0.05)
    # The response takes 1.5 seconds, longer than the deadline, but each
    # chunk comes well within it.
    server.chunks = CHUNKS * 2
    server.chunk_delay_s = 0.25

    async def script(rec):
        # Until the text ends, Kalpa may be waiting for the rest of it.
        await rec.say("r1", "Half a sentence", end=False)
        await asyncio.sleep(1.3)
        await rec.say("r1", " and the rest.")

    rec = run(config, script, "r1")
    assert rec.ends() == [("r1", OK)] and not rec.errors()
    assert bytes(rec.audio) == AUDIO * 2
    # Meanwhile the session was kept open with empty messages, which are
    # no text to speak.
    assert server.sessions[0]["keepalives"] >= 2
    assert server.utterances == ["Half a sentence and the rest."]


@pytest.mark.parametrize("reported", [True, False])
def test_connections_that_keep_failing_are_given_up_on_until_a_new_request(
    server, config, quick, request, reported
):
    server.fail_sessions = 5
    server.holds = [1]
    if not reported:
        # The client's reports are slow, so r1 arrives after the client
        # gave up and before it has said so.
        request.getfixturevalue("slow_end")

    async def script(rec):
        if reported:
            await rec.until(rec.errors)
        else:
            await rec.until(lambda: len(server.sessions) == 5)
            await asyncio.sleep(0.1)
        await rec.say("r1", "A reason to connect again.")
        await rec.until(lambda: rec.audio)
        # The report of the client that gave up would have arrived by now.
        await asyncio.sleep(0.5)
        server.release()

    rec = run(config, script, "r1")
    # Nothing has to change for a later connection to work, so giving up
    # is not fatal. A new request connects again, and the client that gave
    # up cannot fail it.
    assert rec.errors() == [("", NON_FATAL, "connection_closed")] * reported
    assert rec.ends() == [("r1", OK)]
    assert len(server.sessions) == 6 and bytes(rec.audio) == AUDIO
    # Each failed connection waits twice as long as the one before.
    opened = [session["opened_at"] for session in server.sessions[:5]]
    gaps = [later - earlier for earlier, later in zip(opened, opened[1:])]
    assert all(gap >= 0.01 * 2**i for i, gap in enumerate(gaps))


def test_failed_connections_are_counted_in_a_row(server, config, quick):
    # Four connections fail and the fifth works. It drops at r2's text and
    # the next one fails too: six failures, but never five in a row.
    server.fail_sessions = 4

    async def script(rec):
        await rec.until(lambda: len(server.sessions) == 5)
        await rec.say("r1", "On the fifth connection.")
        await rec.until(lambda: rec.ended("r1"))
        server.drop_on_text, server.fail_sessions = 1, 1
        await rec.say("r2", "Sent again twice.")

    rec = run(config, script, "r2")
    assert rec.errors() == [("r2", NON_FATAL, "connection_closed")] * 2
    assert rec.ends() == [("r1", OK), ("r2", OK)]
    assert len(server.sessions) == 7


@pytest.mark.parametrize("fault", ["oversized message", "attempt raises"])
def test_a_connection_that_fails_in_another_way_is_a_failed_connection(
    server, config, quick, monkeypatch, running, fault
):
    if fault == "oversized message":
        # Each attempt ends at the first audio message.
        monkeypatch.setattr(running.kalpa_tts, "MAX_MESSAGE_BYTES", 1000)
        error = ("r1", NON_FATAL, "connection_closed")
    else:
        # A host name with an empty label: the library raises UnicodeError.
        config["params"]["base_url"] = "ws://a..b"
        error = ("r1", NON_FATAL, "connection_error")

    async def script(rec):
        if fault == "attempt raises":
            # The client has given up, and said so, before r1 arrives.
            await rec.until(rec.errors)
        await rec.say("r1", "Never gets through.")
        await rec.until(lambda: rec.errors().count(error) == 3)

    rec = run(config, script)
    # r1 was sent again twice and then failed, instead of waiting for ever.
    assert set(rec.errors()) <= {error, ("", NON_FATAL, "connection_error")}
    assert not rec.audio and not rec.ends()
