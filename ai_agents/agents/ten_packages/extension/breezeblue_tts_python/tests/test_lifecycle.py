import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
# The project root is 6 levels up from the parent directory of this file.
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Adapter-level lifecycle tests: request ownership, exact audio-end fields,
dump contents, and recovery, with a controlled clock and a fake HTTP layer."""
import json

import httpx

from .lifecycle_support import (
    REASON_ERROR,
    REASON_INTERRUPTED,
    REASON_REQUEST_END,
    BlockedStream,
    FakeClock,
    expected_duration_ms,
    make_response,
    run_scripted,
)

PCM_20MS = b"\x01\x00" * 480  # 20 ms at 24 kHz, 16-bit mono
ERROR_BODY_429 = json.dumps(
    {"ok": False, "code": "RATE_LIMITED", "detail": "Slow down", "error": "x"}
).encode()


def _ends_by_request(tester):
    result = {}
    for end in tester.ends:
        assert (
            end["request_id"] not in result
        ), f"duplicate tts_audio_end for {end['request_id']}"
        result[end["request_id"]] = end
    return result


# ================ cancellation before audio starts ================
def test_flush_before_audio_interrupts_pending_request_and_recovers():
    """A flush while the request is still waiting for response headers: the
    pending open is interrupted, no stale terminal event or audio appears for
    it, and the next request is served promptly."""
    blocked = BlockedStream()
    tester, http, dumps = run_scripted(
        [("r0", "hello", True)],
        [blocked, make_response([PCM_20MS, PCM_20MS])],
        stop_after_ends=1,
        flush_at=(0.4, "r0"),
        after_flush=[("r1", "again", True)],
        dump=True,
        timeout_s=10,
    )

    assert not tester.timed_out
    assert blocked.interrupted
    assert tester.flush_ended
    # r0 never produced audio, so it has no audio_start/audio_end at all.
    assert "start:r0" not in tester.order and "end:r0" not in tester.order
    assert "r0" not in tester.audio and dumps.get("r0", b"") == b""
    # r1 completes normally with exact accounting and its own audio.
    ends = _ends_by_request(tester)
    assert list(ends) == ["r1"]
    assert ends["r1"]["reason"] == REASON_REQUEST_END
    assert ends["r1"]["request_total_audio_duration_ms"] == (
        expected_duration_ms(2 * len(PCM_20MS))
    )
    assert bytes(tester.audio["r1"]) == PCM_20MS * 2
    assert http.stream.call_count == 2  # the flushed request was not reopened
    assert not tester.errors


def test_flush_during_retry_backoff_does_not_reopen_the_request():
    from unittest.mock import patch

    def respond(*args, **kwargs):
        if respond.calls == 0:
            respond.calls += 1
            return make_response(
                status=502,
                body=b'{"code":"UPSTREAM_SERVICE_UNAVAILABLE","detail":"later"}',
            )
        respond.calls += 1
        return make_response([PCM_20MS])

    respond.calls = 0
    with patch(
        "breezeblue_tts_python.breezeblue_tts._RETRY_DELAYS_S", (30, 30)
    ):
        tester, http, _ = run_scripted(
            [("r0", "hello", True)],
            respond,
            stop_after_ends=1,
            flush_at=(0.5, "r0"),
            after_flush=[("r1", "again", True)],
            timeout_s=10,
        )

    assert not tester.timed_out  # did not sit out the 30 s back-off
    assert tester.flush_ended
    assert "end:r0" not in tester.order
    assert _ends_by_request(tester)["r1"]["reason"] == REASON_REQUEST_END
    assert http.stream.call_count == 2  # failed attempt + r1, never a retry


# ================ cancellation after audio starts ================
def test_flush_after_audio_has_exact_fields_dump_and_recovery():
    clock = FakeClock()
    response = make_response([PCM_20MS] * 40, delay=0.05)
    with clock.patch():
        tester, http, dumps = run_scripted(
            [("r0", "a long sentence to flush.", True)],
            [response, make_response([PCM_20MS])],
            stop_after_ends=2,
            flush_at=("first_audio", "r0"),
            after_flush=[("r1", "next", True)],
            on_first_audio=lambda: clock.advance(700),
            dump=True,
            timeout_s=12,
        )

    assert not tester.timed_out and not tester.errors
    ends = _ends_by_request(tester)
    assert list(ends) == ["r0", "r1"]
    flushed = ends["r0"]
    emitted = bytes(tester.audio["r0"])
    assert flushed["reason"] == REASON_INTERRUPTED
    assert 0 < len(emitted) < 40 * len(PCM_20MS)
    # Exact: counts the emitted bytes and nothing else, measured start -> end.
    assert flushed["request_total_audio_duration_ms"] == expected_duration_ms(
        len(emitted)
    )
    assert flushed["request_event_interval_ms"] == 700
    assert not tester.audio_after_flush_end
    # The dump holds exactly the emitted PCM, finalized after the flush.
    assert dumps["r0"] == emitted
    # The next request is clean: its own audio, counters and terminal event.
    assert ends["r1"]["reason"] == REASON_REQUEST_END
    assert bytes(tester.audio["r1"]) == PCM_20MS
    assert ends["r1"]["request_total_audio_duration_ms"] == (
        expected_duration_ms(len(PCM_20MS))
    )


# ================ abnormal completion: exact fields + dump ================
def test_error_after_partial_audio_exact_fields_and_dump():
    clock = FakeClock()
    response = make_response(
        [PCM_20MS],
        fail_after=httpx.ReadError("conn reset"),
        before_fail=lambda: clock.advance(400),
    )
    with clock.patch():
        tester, _, dumps = run_scripted(
            [("r0", "hello", True)], [response], stop_after_ends=1, dump=True
        )

    (end,) = tester.ends
    assert end["reason"] == REASON_ERROR
    assert end["request_total_audio_duration_ms"] == expected_duration_ms(
        len(PCM_20MS)
    )
    assert end["request_event_interval_ms"] == 400
    assert [e["code"] for e in tester.errors] == [1000]  # non-fatal
    assert tester.order.count("end:r0") == 1
    assert dumps["r0"] == bytes(tester.audio["r0"]) == PCM_20MS


def test_error_before_audio_exact_fields_and_no_dump_data():
    clock = FakeClock()
    response = make_response(
        status=429, body=ERROR_BODY_429, on_read=lambda: clock.advance(250)
    )
    with clock.patch():
        tester, _, dumps = run_scripted(
            [("r0", "hello", True)], [response], stop_after_ends=1, dump=True
        )

    (end,) = tester.ends
    assert end["reason"] == REASON_ERROR
    assert end["request_total_audio_duration_ms"] == 0
    # No audio started, so the interval runs from the request itself.
    assert end["request_event_interval_ms"] == 250
    assert "r0" not in tester.audio and dumps.get("r0", b"") == b""
    assert tester.errors[0]["message"] == "Slow down"


def test_empty_body_exact_fields_and_empty_dump():
    clock = FakeClock()
    response = make_response([], on_enter=lambda: clock.advance(300))
    with clock.patch():
        tester, _, dumps = run_scripted(
            [("r0", "hello", True)], [response], stop_after_ends=1, dump=True
        )

    (end,) = tester.ends
    assert end["reason"] == REASON_REQUEST_END
    assert end["request_total_audio_duration_ms"] == 0
    assert end["request_event_interval_ms"] == 300
    assert not tester.errors and "r0" not in tester.audio
    assert dumps.get("r0", b"") == b""


def test_success_dump_equals_emitted_pcm_across_odd_chunks():
    pcm = bytes(range(256)) * 10  # 2560 bytes = 1280 whole samples
    # Network chunks split samples, and the stream ends on a stray half sample.
    stream = pcm + b"\x7f"
    pieces = [stream[:1001], stream[1001:1002], stream[1002:]]
    tester, _, dumps = run_scripted(
        [("r0", "hello", True)],
        [make_response(pieces)],
        stop_after_ends=1,
        dump=True,
    )

    (end,) = tester.ends
    # Whole samples only, same bytes, same order: the half sample is dropped
    # instead of being played as noise or shifting the next sample.
    assert bytes(tester.audio["r0"]) == pcm
    assert dumps["r0"] == pcm
    assert end["request_total_audio_duration_ms"] == expected_duration_ms(
        len(pcm)
    )


# ================ request ownership ================
def _respond_with_text_pattern(*args, **kwargs):
    """Each text produces audio whose bytes identify the text."""
    text = kwargs["json"]["text"]
    return make_response([bytes([ord(text[0])]) * 480])


def test_appended_text_on_one_request_id_is_one_request():
    tester, http, dumps = run_scripted(
        [("A", "a1.", False), ("A", "b2.", True)],
        _respond_with_text_pattern,
        stop_after_ends=1,
        dump=True,
    )

    assert tester.order == ["start:A", "end:A"]  # one start, one terminal
    texts = [c.kwargs["json"]["text"] for c in http.stream.call_args_list]
    assert texts == ["a1.", "b2."]  # both pieces sent, in order
    expected = b"a" * 480 + b"b" * 480
    assert bytes(tester.audio["A"]) == expected
    assert dumps["A"] == expected
    assert tester.ends[0]["request_total_audio_duration_ms"] == (
        expected_duration_ms(len(expected))
    )


def test_interleaved_unfinished_request_ids_keep_their_own_audio():
    tester, http, dumps = run_scripted(
        [
            ("A", "a1.", False),
            ("B", "b1.", False),
            ("A", "a2.", True),
            ("B", "b2.", True),
        ],
        _respond_with_text_pattern,
        stop_after_ends=2,
        dump=True,
        timeout_s=12,
    )

    assert not tester.timed_out
    ends = _ends_by_request(tester)
    assert set(ends) == {"A", "B"}  # exactly one terminal event each
    for rid, marker in (("A", ord("a")), ("B", ord("b"))):
        audio = bytes(tester.audio[rid])
        assert audio and set(audio) <= {marker}, f"{rid} got foreign audio"
        assert ends[rid]["request_total_audio_duration_ms"] == (
            expected_duration_ms(len(audio))
        )
        assert dumps[rid] == audio
    # Requests are served one after another, never overlapped.
    assert tester.order.index("end:A") < tester.order.index("start:B")
