#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""What `tts_audio_end` reports, checked against known audio and a clock
that the test moves. The server waits at its `holds`, so the clock moves at
known points of a request.
"""
import pytest

from harness import ERROR, INTERRUPTED, NON_FATAL, OK, run
from mock_server import AUDIO, AUDIO_MS, FIRST, SAMPLE_RATE


@pytest.mark.parametrize(
    "path, reason, heard",
    [
        ("success", OK, AUDIO),
        ("error", ERROR, FIRST),
        ("flush", INTERRUPTED, FIRST),
    ],
)
def test_the_audio_end_reports_the_audio_sent_and_the_time_since_its_start(
    server, config, clock, tmp_path, path, reason, heard
):
    config.update(dump=True, dump_path=str(tmp_path))
    # Kalpa waits before the first chunk and after it. There r1 goes on,
    # fails or is flushed.
    server.holds = [0, 1]
    server.responses = ["error_mid"] if path == "error" else []

    async def script(rec):
        await rec.say("r1", "Hello world.")
        await rec.until(lambda: server.utterances)
        clock.advance(125)
        server.release()
        await rec.until(lambda: bytes(rec.audio) == FIRST)
        clock.advance(375)
        if path == "flush":
            await rec.flush()
        else:
            server.release()
            await rec.until(lambda: rec.ended("r1"))
        clock.advance(250)
        await rec.say("r2", "Heard in full.")

    rec = run(config, script, "r2")
    first, second = rec.of("tts_audio_end")
    assert (first["request_id"], first["reason"]) == ("r1", reason)
    # The duration is that of the bytes sent downstream, summed before it
    # is computed: the chunks split a sample between them, and a request
    # that stops after the first one leaves half a sample out of the audio,
    # the dump and the count.
    assert first["request_total_audio_duration_ms"] == (
        len(heard) * 1000 // (SAMPLE_RATE * 2)
    )
    # From the audio start to the audio end: not the 125 ms before it.
    assert first["request_event_interval_ms"] == 375
    assert bytes(rec.audio) == heard + AUDIO
    assert (tmp_path / "kalpa_dump_r1.pcm").read_bytes() == heard
    assert (tmp_path / "kalpa_dump_r2.pcm").read_bytes() == AUDIO
    # r2 counts its own audio, from its own start.
    assert second["request_total_audio_duration_ms"] == AUDIO_MS
    assert second["request_event_interval_ms"] == 0
    assert rec.metric("ttfb") == [(1, 125), (2, 0)]
    error = [("r1", NON_FATAL, "inference_error")]
    assert rec.errors() == (error if path == "error" else [])


@pytest.mark.parametrize("sample_rate", [16000, 22050])
def test_only_whole_samples_are_sent_dumped_and_counted(
    server, config, tmp_path, sample_rate
):
    config["params"]["sample_rate"] = sample_rate
    config.update(dump=True, dump_path=str(tmp_path))
    # 45 bytes: the chunks split a sample, and the response ends inside one.
    server.chunks = [
        b"\x01\x00" * 10 + b"\x02",
        b"\x00" + b"\x03\x00" * 11 + b"\x04",
    ]

    async def script(rec):
        await rec.say("r1", "Hello world.")

    rec = run(config, script, "r1")
    emitted = b"".join(server.chunks)[:44]
    assert server.sessions[0]["init"]["sample_rate"] == sample_rate
    assert bytes(rec.audio) == emitted
    assert {f["sample_rate"] for f in rec.of("pcm_frame")} == {sample_rate}
    assert (tmp_path / "kalpa_dump_r1.pcm").read_bytes() == emitted
    # 44 bytes are 1.375 ms at 16 kHz and 0.998 ms at 22.05 kHz.
    (end,) = rec.of("tts_audio_end")
    assert end["request_total_audio_duration_ms"] == (sample_rate == 16000)
