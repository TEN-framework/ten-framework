"""Happy-path and streaming tests for BlazeSTTRealtimeExtension.

These run the extension inside the TEN runtime via AsyncExtensionTester with a
faked websocket transport, and assert that:

  * a successful handshake + transcript stream is surfaced as `asr_result`
    data (partial then final), with the init message matching the server's
    `/v1/stt/realtime` protocol;
  * an in-stream `{"type": "error"}` frame is surfaced as `error` data;
  * a server that never sends `ready` fails the connection via the handshake
    timeout rather than blocking forever.

Companion to test_error_msg.py, which covers the credential/connection error
paths. Together they exercise the production code path in extension.py (which
test_blaze_stt_realtime.py does not — that file tests the standalone
BlazeRealtimeClient transport instead).
"""

import asyncio
import json
from unittest.mock import AsyncMock, patch

import websockets
from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
)

EXTENSION_NAME = "blaze_stt_realtime_python"
WS_CONNECT_TARGET = (
    "ten_packages.extension.blaze_stt_realtime_python.extension"
    ".websockets.connect"
)


class FakeRealtimeWS:
    """Minimal stand-in for a connected `websockets` client.

    `recv()` replays the handshake messages (consumed while the extension waits
    for "ready"); async iteration replays the post-handshake transcript stream
    (consumed by the extension's `_recv_loop`). `send()` records everything the
    extension transmits so tests can assert on the init handshake.
    """

    def __init__(self, handshake, stream, hang_forever=False):
        self._handshake = list(handshake)
        self._stream = list(stream)
        self._hang_forever = hang_forever
        self.sent = []
        self.closed = False

    async def send(self, data):
        self.sent.append(data)

    async def recv(self):
        # Yield control so concurrent tasks can run, like a real socket.
        await asyncio.sleep(0)
        if self._hang_forever:
            # Emulate a server that accepts the socket but never replies.
            await asyncio.Event().wait()
        if self._handshake:
            return self._handshake.pop(0)
        raise websockets.ConnectionClosed(None, None)

    def __aiter__(self):
        return self

    async def __anext__(self):
        await asyncio.sleep(0)
        if self._stream:
            return self._stream.pop(0)
        raise StopAsyncIteration

    async def close(self):
        self.closed = True

    @property
    def sent_text(self):
        return [m for m in self.sent if isinstance(m, str)]

    @property
    def sent_audio(self):
        return [m for m in self.sent if isinstance(m, (bytes, bytearray))]


def _patch_connect(fake_ws):
    """Patch the extension's `websockets.connect` to return `fake_ws`."""
    mock_connect = AsyncMock(return_value=fake_ws)
    return patch(WS_CONNECT_TARGET, mock_connect)


def _config(extra=None):
    params = {
        "api_url": "http://localhost:8000",
        "api_key": "test-token",
        "language": "vi",
        "model": "stt-stream-1.5",
    }
    if extra:
        params.update(extra)
    return json.dumps({"params": params})


class ResultCollector(AsyncExtensionTester):
    """Collects `asr_result` data; stops once a final result arrives."""

    def __init__(self):
        super().__init__()
        self.results = []
        self.final_received = False

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        ten_env_tester.log_info("on_start")

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        if data.get_name() != "asr_result":
            return
        payload_json, _ = data.get_property_to_json("")
        payload = json.loads(payload_json)
        self.results.append(payload)
        if payload.get("final"):
            self.final_received = True
            ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass


class ErrorCollector(AsyncExtensionTester):
    """Stops as soon as an `error` data message is received."""

    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_payload = None

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        ten_env_tester.log_info("on_start")

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        if data.get_name() != "error":
            return
        error_json, _ = data.get_property_to_json("")
        self.error_payload = json.loads(error_json)
        self.error_received = True
        ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass


def test_streaming_emits_asr_results():
    """A successful handshake + partial/final stream yields asr_result data."""
    fake_ws = FakeRealtimeWS(
        handshake=[json.dumps({"type": "ready", "text": "ok"})],
        stream=[
            json.dumps({"type": "partial", "text": "xin chào"}),
            json.dumps(
                {"type": "final", "text": "Xin chào, tôi là trợ lý ảo."}
            ),
        ],
    )

    tester = ResultCollector()
    tester.set_test_mode_single(EXTENSION_NAME, _config())

    with _patch_connect(fake_ws):
        err = tester.run()

    assert err is None, f"Unexpected tester error: {err}"
    assert tester.final_received, "Expected a final asr_result"

    # Partial then final, with the expected text and final flags.
    assert [r["text"] for r in tester.results] == [
        "xin chào",
        "Xin chào, tôi là trợ lý ảo.",
    ]
    assert [r["final"] for r in tester.results] == [False, True]
    assert tester.results[-1]["language"] == "vi"

    # The init handshake matches the server's /v1/stt/realtime protocol.
    init = json.loads(fake_ws.sent_text[0])
    assert init == {
        "token": "test-token",
        "language": "vi",
        "model": "stt-stream-1.5",
        "enable_log": False,
    }


def test_streaming_error_frame_is_surfaced():
    """An in-stream {"type": "error"} frame is surfaced as error data."""
    fake_ws = FakeRealtimeWS(
        handshake=[json.dumps({"type": "ready", "text": "ok"})],
        stream=[json.dumps({"type": "error", "text": "upstream blew up"})],
    )

    tester = ErrorCollector()
    tester.set_test_mode_single(EXTENSION_NAME, _config())

    with _patch_connect(fake_ws):
        err = tester.run()

    assert err is None, f"Unexpected tester error: {err}"
    assert tester.error_received, "Expected an error message"
    assert tester.error_payload is not None
    # NON_FATAL_ERROR is 1000; a transcript-stream error is recoverable.
    assert (
        str(tester.error_payload.get("code")) == "1000"
    ), f"Expected NON_FATAL_ERROR (1000), got: {tester.error_payload}"
    assert tester.error_payload.get("message") == "upstream blew up"


def test_handshake_timeout_is_surfaced():
    """A server that never sends 'ready' fails via the handshake timeout."""
    fake_ws = FakeRealtimeWS(handshake=[], stream=[], hang_forever=True)

    tester = ErrorCollector()
    # Use a 1s handshake timeout to keep the test fast.
    tester.set_test_mode_single(
        EXTENSION_NAME, _config({"handshake_timeout": 1})
    )

    with _patch_connect(fake_ws):
        err = tester.run()

    assert err is None, f"Unexpected tester error: {err}"
    assert tester.error_received, "Expected a timeout error message"
    assert tester.error_payload is not None
    # FATAL_ERROR is -1000; a failed handshake is unrecoverable.
    assert (
        str(tester.error_payload.get("code")) == "-1000"
    ), f"Expected FATAL_ERROR (-1000), got: {tester.error_payload}"
    assert "ready" in (tester.error_payload.get("message") or "").lower()
