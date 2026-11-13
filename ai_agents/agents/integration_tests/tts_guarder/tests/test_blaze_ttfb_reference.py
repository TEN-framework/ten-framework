"""Blaze auth and status frames must not count as text or audio."""

import json

from .ttfb_reference import TTFBReference
from .ttfb_hooks.blaze import BlazeObserver


def query(text):
    return json.dumps(
        {"query": text, "audio_format": "pcm", "model": "2.0-realtime"}
    )


def status(name):
    return json.dumps({"status": name})


def test_blaze_reference_counts_query_to_first_binary_audio(monkeypatch):
    clock = [0]
    monkeypatch.setattr(
        "tests.ttfb_hooks.base.time.monotonic_ns", lambda: clock[0]
    )
    reference = TTFBReference()
    observer = BlazeObserver(reference)

    # Connection setup happens before any tester request.
    observer.observe_receive(json.dumps({"type": "successful-connection"}))
    observer.observe_send(json.dumps({"token": "k", "strategy": "request"}))
    observer.observe_receive(json.dumps({"type": "successful-authentication"}))

    # Two sequential requests on the same socket.
    for request_id in ["first", "second"]:
        reference.begin_request(request_id)
        clock[0] += 100_000_000
        observer.observe_send(query("   "))
        clock[0] += 50_000_000
        observer.observe_send(query("Xin chào."))
        observer.observe_receive(json.dumps({"type": "processing-request"}))
        observer.observe_receive(status("started-byte-stream"))
        observer.observe_receive(b"")
        clock[0] += 30_000_000
        observer.observe_receive(b"\x01\x00" * 160)
        clock[0] += 80_000_000
        observer.observe_receive(b"\x02\x00" * 160)
        observer.observe_receive(status("finished-byte-stream"))
        assert reference.elapsed_ms(request_id) == 30


def test_blaze_reference_is_isolated_per_connection(monkeypatch):
    clock = [0]
    monkeypatch.setattr(
        "tests.ttfb_hooks.base.time.monotonic_ns", lambda: clock[0]
    )
    reference = TTFBReference()
    old_socket = BlazeObserver(reference)
    new_socket = BlazeObserver(reference)

    reference.begin_request("first")
    old_socket.observe_send(query("Một."))
    # The old socket was dropped; the request is retried on a new one.
    clock[0] += 40_000_000
    new_socket.observe_send(query("Một."))
    clock[0] += 25_000_000
    new_socket.observe_receive(b"\x01\x00" * 160)
    # Audio on a socket with no query is never attributed to a request.
    BlazeObserver(reference).observe_receive(b"\x01\x00" * 160)

    assert reference.elapsed_ms("first") == 65
