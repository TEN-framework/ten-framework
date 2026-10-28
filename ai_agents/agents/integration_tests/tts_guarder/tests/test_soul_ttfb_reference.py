"""Soul gateway control frames must not count as text or audio."""

import gzip
import json
import struct

import pytest

from .ttfb_reference import TTFBReference
from .ttfb_hooks.soul import SoulObserver


def gateway_frame(body, compressed=False):
    payload = json.dumps(body).encode()
    if compressed:
        payload = gzip.compress(payload)
    header = struct.pack("!BI", 1, 5) + b"trace"
    return (
        struct.pack(
            "!IBBBBIB",
            0x736F756C,
            1,
            2,
            1,
            int(compressed),
            len(header) + len(payload),
            1,
        )
        + header
        + payload
    )


@pytest.mark.parametrize("wire_format", ["json", "gateway", "gzip"])
def test_soul_reference_excludes_control_and_empty_audio(monkeypatch, wire_format):
    clock = [0]
    monkeypatch.setattr("tests.ttfb_hooks.base.time.monotonic_ns", lambda: clock[0])
    reference = TTFBReference()
    observer = SoulObserver(reference)

    def frame(body):
        return (
            json.dumps(body)
            if wire_format == "json"
            else gateway_frame(body, wire_format == "gzip")
        )

    for request_id in ["streamed", "single-final"]:
        reference.begin_request(request_id)
        clock[0] += 100_000_000
        observer.observe_send(frame({"event": "task_start", "text": "control"}))
        clock[0] += 150_000_000
        observer.observe_send(frame({"event": "task_continue", "text": "Hello."}))
        observer.observe_send(frame({"event": "task_continue", "text": ""}))
        observer.observe_send(frame({"event": "task_finish"}))
        observer.observe_receive(
            frame({"event": "task_started", "data": {"audio": "AQ A="}})
        )
        observer.observe_receive(
            frame({"event": "task_continued", "data": {"audio": ""}})
        )
        clock[0] += 20_000_000
        observer.observe_receive(
            frame({"event": "task_continued", "data": {"audio": "AQA="}})
        )
        clock[0] += 80_000_000
        observer.observe_receive(
            frame({"event": "task_continued", "data": {"audio": "AgA="}})
        )
        assert reference.elapsed_ms(request_id) == 20
