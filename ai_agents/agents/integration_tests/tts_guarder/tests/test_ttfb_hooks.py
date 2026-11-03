"""Deterministic wire fixtures for all seven first-batch TTFB modes."""

import gzip
import json

import pytest

from .ttfb_reference import TTFBReference
from .ttfb_hooks.elevenlabs import ElevenLabsObserver
from .ttfb_hooks.bytedance import ByteDanceObserver
from .ttfb_hooks.bytedance_duplex import ByteDanceDuplexObserver
from .ttfb_hooks.minimax import MiniMaxObserver
from .ttfb_hooks.soul import SoulObserver
from .test_soul_ttfb_reference import gateway_frame


def duplex(event, context, payload):
    session = context.encode()
    return (
        b"\x11\x94\x00\x00"
        + event.to_bytes(4, "big")
        + len(session).to_bytes(4, "big")
        + session
        + len(payload).to_bytes(4, "big")
        + payload
    )


def wire(mode, context, audio=False, empty=False):
    text = "" if empty else "Hello."
    pcm = b"" if empty else b"\x01\x00"
    if mode == "bytedance":
        if audio:
            return (
                b"\x11\xb1\x00\x00"
                + (1).to_bytes(4, "big")
                + len(pcm).to_bytes(4, "big")
                + pcm
            )
        body = gzip.compress(
            json.dumps({"request": {"reqid": context, "text": text}}).encode()
        )
        return b"\x11\x10\x11\x00" + len(body).to_bytes(4, "big") + body
    if mode == "bytedance-duplex":
        return duplex(
            352 if audio else 200,
            context,
            pcm if audio else json.dumps({"req_params": {"text": text}}).encode(),
        )
    if mode in ["tts", "ttd"]:
        if audio:
            return json.dumps(
                {
                    "contextId" if mode == "tts" else "context_id": context,
                    "audio": "" if empty else "AQA=",
                }
            )
        return json.dumps(
            {
                "context_id": context,
                **({"text": text} if mode == "tts" else {"inputs": [{"text": text}]}),
            }
        )
    body = (
        {
            "event": "task_continued",
            "data": {"audio": "" if empty else ("AQA=" if mode == "soul" else "0100")},
        }
        if audio
        else {"event": "task_continue", "text": text}
    )
    return gateway_frame(body) if mode == "soul" else json.dumps(body)


@pytest.mark.parametrize(
    "mode,observer_class",
    [
        ("tts", ElevenLabsObserver),
        ("ttd", ElevenLabsObserver),
        ("bytedance", ByteDanceObserver),
        ("bytedance-duplex", ByteDanceDuplexObserver),
        ("minimax", MiniMaxObserver),
        ("minimax-duplex", MiniMaxObserver),
        ("soul", SoulObserver),
    ],
)
def test_hook_times_only_first_text_and_nonempty_audio(
    monkeypatch, mode, observer_class
):
    now = [0]
    monkeypatch.setattr("tests.ttfb_hooks.base.time.monotonic_ns", lambda: now[0])
    reference = TTFBReference()
    observer = observer_class(reference)
    for request in ["first", "second"]:
        reference.begin_request(request)
        now[0] += 150_000_000  # Connection/session preparation is outside the interval.
        observer.observe_send(wire(mode, request, empty=True))
        observer.observe_send(wire(mode, request))
        now[0] += 5_000_000
        observer.observe_send(wire(mode, request))
        observer.observe_receive(wire(mode, request, audio=True, empty=True))
        now[0] += 15_000_000
        observer.observe_receive(wire(mode, request, audio=True))
        now[0] += 100_000_000
        observer.observe_receive(wire(mode, request, audio=True))
        assert reference.elapsed_ms(request) == 20


@pytest.mark.parametrize(
    "mode,observer_class",
    [
        ("tts", ElevenLabsObserver),
        ("ttd", ElevenLabsObserver),
        ("bytedance-duplex", ByteDanceDuplexObserver),
    ],
)
def test_context_does_not_cross_requests(monkeypatch, mode, observer_class):
    now = [0]
    monkeypatch.setattr("tests.ttfb_hooks.base.time.monotonic_ns", lambda: now[0])
    reference = TTFBReference()
    old_connection = observer_class(reference)
    reference.begin_request("old")
    old_connection.observe_send(wire(mode, "old"))
    reference.begin_request("new")
    connection = observer_class(reference)
    connection.observe_send(wire(mode, "new"))
    now[0] = 10_000_000
    old_connection.observe_receive(wire(mode, "old", audio=True))
    connection.observe_receive(wire(mode, "old", audio=True))
    assert reference.elapsed_ms("new") is None
    now[0] = 20_000_000
    connection.observe_receive(wire(mode, "new", audio=True))
    assert reference.elapsed_ms("new") == 20


@pytest.mark.parametrize(
    "mode,extension,filename",
    [
        ("tts", "elevenlabs_multiple_context_tts_python", "elevenlabs_tts"),
        ("ttd", "elevenlabs_multiple_context_tts_python", "elevenlabs_ttd"),
        ("bytedance", "bytedance_tts", "bytedance_tts"),
        ("bytedance-duplex", "bytedance_tts_duplex", "bytedance_tts"),
        ("minimax", "minimax_tts_websocket", "minimax_tts"),
        ("minimax-duplex", "minimax_tts_websocket_duplex", "minimax_tts"),
        ("soul", "soul_tts_python", "soul_tts"),
    ],
)
def test_hook_installs_on_transport_and_restores(
    monkeypatch, mode, extension, filename
):
    import asyncio

    asyncio.run(_transport_scenario(monkeypatch, mode, extension, filename))


async def _transport_scenario(monkeypatch, mode, extension, filename):
    import asyncio
    import sys
    from types import ModuleType

    websockets = pytest.importorskip(
        "websockets", reason="local WebSocket transport fixture"
    )

    from .ttfb_reference import install_ttfb_reference

    request = "transport-test"
    outgoing = wire(mode, request)
    incoming = wire(mode, request, audio=True)
    observed = []

    async def provider(socket):
        observed.append(await socket.recv())
        await socket.send(wire(mode, request, audio=True, empty=True))
        await asyncio.sleep(0.02)
        await socket.send(incoming)
        await socket.wait_closed()

    async with websockets.serve(provider, "127.0.0.1", 0) as server:
        port = server.sockets[0].getsockname()[1]
        url = f"ws://127.0.0.1:{port}/test"
        model = "eleven_v4" if mode == "ttd" else "eleven_flash_v2_5"
        # The installer only needs a dependency binding and config contract.
        # No vendor package/SDK is required by these transport utility tests.
        module_name = f"ten_packages.extension.{extension}.{filename}"
        module = ModuleType(module_name)
        module.websockets = websockets
        config_module = ModuleType(f"ten_packages.extension.{extension}.config")
        from .ttfb_reference import CONFIGS

        class Config:
            def __init__(self, **values):
                self.url = values["url"]
                self.uri = self.url
                self.api_url = self.url
                self.model_id = values["params"]["model_id"]
                self.params = values["params"]

            def apply_defaults(self):
                pass

            def update_params(self):
                pass

        setattr(config_module, CONFIGS[extension][0], Config)
        config_module.uses_ttd_protocol = lambda model: model == "eleven_v4"
        monkeypatch.setitem(sys.modules, module_name, module)
        monkeypatch.setitem(sys.modules, config_module.__name__, config_module)
        original = module.websockets
        with install_ttfb_reference(
            extension, {"url": url, "params": {"model_id": model}}, monkeypatch
        ) as reference:
            assert reference is not None
            reference.begin_request(request)
            async with module.websockets.connect(url) as socket:
                await socket.send(outgoing)
                assert await socket.recv() == wire(
                    mode, request, audio=True, empty=True
                )
                assert reference.elapsed_ms(request) is None
                assert await socket.recv() == incoming
                elapsed = reference.elapsed_ms(request)
                assert elapsed is not None and elapsed >= 15
        assert module.websockets is original
    assert observed == [outgoing]


@pytest.mark.parametrize(
    "mode,observer_class",
    [
        ("tts", ElevenLabsObserver),
        ("ttd", ElevenLabsObserver),
        ("bytedance", ByteDanceObserver),
        ("bytedance-duplex", ByteDanceDuplexObserver),
        ("minimax", MiniMaxObserver),
        ("minimax-duplex", MiniMaxObserver),
        ("soul", SoulObserver),
    ],
)
def test_hook_defers_decode_and_retains_first_audio_arrival(
    monkeypatch, mode, observer_class
):
    now = [0]
    monkeypatch.setattr("tests.ttfb_hooks.base.time.monotonic_ns", lambda: now[0])
    reference = TTFBReference()
    observer = observer_class(reference)
    reference.begin_request("request")
    original_send = observer.text_context
    parsed_sends = []

    def decode_send(raw, request_id):
        parsed_sends.append(raw)
        return original_send(raw, request_id)

    monkeypatch.setattr(observer, "text_context", decode_send)
    observer.observe_send(wire(mode, "request"))
    original = observer.audio_context
    parsed = []

    def decode(raw):
        parsed.append(raw)
        return original(raw)

    monkeypatch.setattr(observer, "audio_context", decode)
    now[0] = 5_000_000
    observer.observe_receive(wire(mode, "request", audio=True, empty=True))
    now[0] = 20_000_000
    observer.observe_receive(wire(mode, "request", audio=True))
    now[0] = 50_000_000
    observer.observe_receive(wire(mode, "request", audio=True))
    assert parsed == []
    assert parsed_sends == []
    now[0] = 100_000_000  # Reference is inspected much later than audio arrival.
    assert reference.elapsed_ms("request") == 20
    assert len(parsed) == 3
    assert len(parsed_sends) == 1
    assert reference.elapsed_ms("request") == 20
    assert len(parsed) == 3  # Each queued frame is decoded once.


def test_reading_previous_request_does_not_return_pending_request(monkeypatch):
    now = [0]
    monkeypatch.setattr("tests.ttfb_hooks.base.time.monotonic_ns", lambda: now[0])
    reference = TTFBReference()
    observer = ElevenLabsObserver(reference)
    reference.begin_request("first")
    observer.observe_send(wire("tts", "first"))
    now[0] = 20_000_000
    observer.observe_receive(wire("tts", "first", audio=True))
    assert reference.elapsed_ms("first") == 20
    reference.begin_request("second")
    now[0] = 100_000_000
    observer.observe_send(wire("tts", "second"))
    assert reference.elapsed_ms("first") == 20
    assert reference.elapsed_ms("second") is None
