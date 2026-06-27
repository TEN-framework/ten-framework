"""
Pytest fixtures for Blaze Realtime STT Extension tests
"""

import asyncio
import json

import pytest
import websockets


class FakeWebSocket:
    """
    Minimal stand-in for a `websockets` client connection.

    `recv()` replays the queued `incoming` messages in order, then raises
    `ConnectionClosed` to emulate the server closing the stream. `send()`
    records everything the extension transmits so tests can assert on the
    init handshake and forwarded audio chunks.
    """

    def __init__(self, incoming):
        self._incoming = list(incoming)
        self.sent = []

    async def send(self, data):
        self.sent.append(data)

    async def recv(self):
        # Yield control so a concurrent sender task can run, mirroring a real
        # socket that suspends on network I/O.
        await asyncio.sleep(0)
        if self._incoming:
            return self._incoming.pop(0)
        raise websockets.ConnectionClosed(None, None)

    @property
    def sent_text(self):
        """Text (JSON/string) messages the extension sent."""
        return [m for m in self.sent if isinstance(m, str)]

    @property
    def sent_audio(self):
        """Binary audio chunks the extension sent."""
        return [m for m in self.sent if isinstance(m, (bytes, bytearray))]


class FakeConnect:
    """Async context manager returned by a patched `websockets.connect`."""

    def __init__(self, ws):
        self.ws = ws

    async def __aenter__(self):
        return self.ws

    async def __aexit__(self, *exc):
        return False


@pytest.fixture
def patch_ws(monkeypatch):
    """
    Return a factory that patches `websockets.connect` to yield a FakeWebSocket
    pre-loaded with the given incoming messages, and returns that FakeWebSocket
    so the test can inspect what was sent.
    """
    import blaze_stt_realtime_python.blaze_stt_realtime as mod

    def _install(incoming):
        ws = FakeWebSocket(incoming)
        monkeypatch.setattr(
            mod.websockets, "connect", lambda *a, **k: FakeConnect(ws)
        )
        return ws

    return _install


@pytest.fixture
def sample_pcm():
    """A small PCM-ish buffer (content is irrelevant for the mocked socket)."""
    return b"\x00\x01" * 4000  # 8000 bytes


@pytest.fixture
def mock_config():
    """Mock configuration for BlazeRealtimeClient"""
    return {
        "api_url": "http://localhost:8000",
        "api_key": "test-token",
        "language": "vi",
        "model": "stt-stream-1.5",
        "timeout": 3600,
    }


@pytest.fixture
def ready_partial_final():
    """A typical message sequence: ready -> partial -> final."""
    return [
        # Server sends {"type": "ready", "text": "ok"} (see realtime_ws.py).
        json.dumps({"type": "ready", "text": "ok"}),
        json.dumps({"type": "partial", "text": "xin chào"}),
        json.dumps({"type": "final", "text": "Xin chào, tôi là trợ lý ảo."}),
    ]
