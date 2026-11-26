#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import os

import pytest

from ten_packages.extension.kalpa_tts_python.config import (
    DEFAULT_MODEL,
    DEFAULT_VOICE_ID,
)

from harness import OK, run
from mock_server import AUDIO, AUDIO_MS, FIRST, SAMPLE_RATE

SECRET = "sk-live-not-a-real-key-0123456789"


def test_one_request_is_one_audio_lifecycle(server, config):
    endpoint = f"{server.url}/v1/tts/{{voice_id}}/stream"
    config["params"]["url"] = f"{endpoint}?token={SECRET}"
    first = "This first sentence is long enough for Kalpa to start speaking."

    async def script(rec):
        # A TEN agent cuts a reply after each punctuation mark, and the
        # space after the mark starts the next piece. Kalpa speaks a
        # sentence once whitespace follows it: before the next piece.
        await rec.say("r1", first, end=False)
        await rec.until(lambda: rec.audio)
        await rec.say("r1", " How are you?", end=False)
        await rec.say("r1", "")

    rec = run(config, script, "r1")
    # The text reaches Kalpa as it was written, in one utterance.
    assert server.utterances == [first + " How are you? "]
    assert bytes(rec.audio) == AUDIO
    frames = rec.of("pcm_frame")
    assert {tuple(frame.values()) for frame in frames} == {(SAMPLE_RATE, 1, 2)}
    audio_events = ("tts_audio_start", "pcm_frame", "tts_audio_end")
    assert [name for name in rec.names() if name in audio_events] == (
        ["tts_audio_start"] + ["pcm_frame"] * len(frames) + ["tts_audio_end"]
    )
    assert len(rec.metric("ttfb")) == 1
    (end,) = rec.of("tts_audio_end")
    assert (end["request_id"], end["reason"]) == ("r1", OK)
    assert end["request_total_audio_duration_ms"] == AUDIO_MS
    metadata = end["metadata"]
    assert (metadata["session_id"], metadata["turn_id"]) == ("session", 1)
    # The endpoint is reported without the token it was given with.
    assert metadata["vendor_metadata"] == {
        "url": endpoint.format(voice_id=DEFAULT_VOICE_ID),
        "model": DEFAULT_MODEL,
        "voice_id": DEFAULT_VOICE_ID,
    }
    assert SECRET not in repr(rec.events) and not rec.errors()


def test_interleaved_requests_keep_their_own_text_and_metrics(server, config):
    texts = ["First request, first ending.", "Second request, second ending."]

    async def script(rec):
        # All four arrive before the first request has been spoken.
        await rec.say("r1", "First request,", end=False)
        await rec.say("r2", "Second request,", end=False)
        await rec.say("r1", " first ending.")
        await rec.say("r2", " second ending.")

    rec = run(config, script, "r2")
    assert server.utterances == texts
    assert [e["request_id"] for e in rec.of("tts_audio_start")] == ["r1", "r2"]
    assert rec.ends() == [("r1", OK), ("r2", OK)]
    assert bytes(rec.audio) == AUDIO * 2
    ends = rec.of("tts_audio_end")
    assert [e["request_total_audio_duration_ms"] for e in ends] == [
        AUDIO_MS
    ] * 2
    # Each has its own TTFB and usage, on the session opened at startup.
    assert [turn for turn, _ in rec.metric("ttfb")] == [1, 2]
    assert rec.metric("output_characters") == [(1, 28), (2, 30)]
    assert len(server.sessions) == 1 and not rec.errors()


def test_pieces_of_a_request_that_waited_keep_their_order(server, config):
    async def script(rec):
        # r2's first piece waits behind r1. Its last piece is queued when
        # r1 ends and the first one is released.
        await rec.say("r1", "  ", end=False)
        await rec.say("r2", "The first piece of r2,", end=False)
        await asyncio.sleep(0.1)
        await rec.say("r1", "")
        await rec.say("r2", " and the piece that ends it.")

    rec = run(config, script, "r2")
    # r1 was empty: it ends at once, with no audio, and never reaches Kalpa.
    assert server.utterances == [
        "The first piece of r2, and the piece that ends it."
    ]
    assert rec.ends() == [("r1", OK), ("r2", OK)]
    empty = rec.of("tts_audio_end")[0]
    assert empty["request_event_interval_ms"] == 0
    assert empty["request_total_audio_duration_ms"] == 0
    assert bytes(rec.audio) == AUDIO and not rec.errors()


def test_a_long_request_continues_in_a_second_utterance(server, config):
    # 8,850 characters; an utterance holds 8,000.
    text = "This sentence is repeated to fill more than one utterance. " * 150

    async def script(rec):
        await rec.say("r1", text)

    rec = run(config, script, "r1")
    assert [len(u) <= 8000 for u in server.utterances] == [True, True]
    assert "".join(server.utterances) == text
    # Kalpa finishing the first utterance does not end the request.
    assert rec.ends() == [("r1", OK)]
    assert bytes(rec.audio) == AUDIO * 2


def test_ttfb_runs_from_the_text_sent_to_the_first_audio(server, config, clock):
    # The first connection drops at r1's text and the next session is slow
    # to open. TTFB starts when the text goes out on the connection that
    # answers: neither wait is part of it.
    server.drop_on_text = 1
    server.holds = [0]

    async def script(rec):
        await rec.until(lambda: rec.of("connection_status_changed")[1:])
        server.hold_session = True
        await rec.say("r1", "Hello world.")
        await rec.until(
            lambda: server.sessions[1:] and server.sessions[1]["init"]
        )
        clock.advance(500)
        server.release()
        await rec.until(lambda: server.utterances)
        clock.advance(125)
        server.release()

    rec = run(config, script, "r1")
    assert rec.metric("ttfb") == [(1, 125)]


def test_usage_is_reported_once_for_each_request(server, config):
    texts = ["Sent twice.", "Fails before audio.", "Heard in full."]
    # The first connection drops at r1's text, so r1 is sent again. The
    # response to r2 fails before it has any audio.
    server.drop_on_text = 1
    server.responses = ["", "error"]

    async def script(rec):
        for turn, text in enumerate(texts, start=1):
            await rec.say(f"r{turn}", text)

    rec = run(config, script, "r3")
    # Not twice for the text sent again, and not with the next request for
    # the one that failed.
    assert rec.metric("output_characters") == [(1, 11), (2, 19), (3, 14)]


@pytest.mark.parametrize("dump", [True, False])
def test_dump_writes_the_audio_of_each_request(server, config, tmp_path, dump):
    config.update(dump=dump, dump_path=str(tmp_path))

    async def script(rec):
        await rec.say("r1", "First request.")
        await rec.say("r2", "")
        # An id is not a path: this one stays inside the dump directory.
        await rec.say("../r3", "Third request.")
        await rec.until(lambda: rec.ended("../r3"))
        server.holds = [1]
        await rec.say("r4", "Stopped part-way.")
        await rec.until(lambda: len(rec.audio) == 2 * len(AUDIO) + len(FIRST))

    # The extension is stopped here, with r4 still open.
    run(config, script)
    files = {
        name: (tmp_path / name).read_bytes() for name in os.listdir(tmp_path)
    }
    # No file for the empty r2, and what was heard of r4 is kept.
    heard = {"r1": AUDIO, ".._r3": AUDIO, "r4": FIRST} if dump else {}
    assert files == {
        f"kalpa_dump_{name}.pcm": pcm for name, pcm in heard.items()
    }
