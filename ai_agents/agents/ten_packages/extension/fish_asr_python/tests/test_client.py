import asyncio
from unittest.mock import AsyncMock, patch

import ormsgpack
from websockets.exceptions import ConnectionClosedError

from .. import websocket
from ..websocket import FishAudioASRClient


class FakeSocket:
    def __init__(self, incoming):
        self.incoming = list(incoming)
        self.sent = []
        self.closed = False
        self.close_event = asyncio.Event()

    def __aiter__(self):
        return self

    async def __anext__(self):
        if not self.incoming:
            await self.close_event.wait()
            raise StopAsyncIteration
        return self.incoming.pop(0)

    async def send(self, frame):
        self.sent.append(ormsgpack.unpackb(frame))

    async def close(self):
        self.closed = True
        self.close_event.set()


def test_client_sends_binary_config_and_audio():
    async def run():
        socket = FakeSocket(
            [
                ormsgpack.packb(
                    {"type": "session_started", "session_id": "session-1"}
                )
            ]
        )
        messages = []
        closed = []

        async def on_message(message):
            messages.append(message)

        async def on_close(code, message):
            closed.append((code, message))

        client = FishAudioASRClient(
            url="wss://api.fish.audio/v1/asr/realtime",
            api_key="test-key",
            model="transcribe-stream-1",
            config_message={"type": "config"},
            on_message=on_message,
            on_close=on_close,
        )
        with patch.object(
            websocket, "connect", new=AsyncMock(return_value=socket)
        ):
            await client.connect()
            await client.send_audio(b"pcm")
            await client.finalize()
            await client.close(graceful=False)

        assert socket.sent == [
            {"type": "config"},
            {"type": "audio", "data": b"pcm"},
            {"type": "finalize"},
        ]
        assert messages[0]["type"] == "session_started"
        assert socket.closed is True
        assert closed == []

    asyncio.run(run())


def test_client_does_not_wait_for_config_rejection_close():
    async def run():
        socket = FakeSocket(
            [
                ormsgpack.packb(
                    {"type": "error", "code": 400, "message": "bad config"}
                )
            ]
        )
        closed = []

        async def on_message(_message):
            await socket.close()

        async def on_close(code, message):
            closed.append((code, message))

        client = FishAudioASRClient(
            url="wss://api.fish.audio/v1/asr/realtime",
            api_key="test-key",
            model="transcribe-stream-1",
            config_message={"type": "config"},
            on_message=on_message,
            on_close=on_close,
        )
        with patch.object(
            websocket, "connect", new=AsyncMock(return_value=socket)
        ):
            try:
                await asyncio.wait_for(client.connect(), timeout=1.0)
            except ConnectionError:
                pass
            else:
                raise AssertionError("config rejection must fail connect")
        assert closed

    asyncio.run(run())


def test_connect_waits_until_the_session_callback_finishes():
    async def run():
        socket = FakeSocket([ormsgpack.packb({"type": "session_started"})])
        entered = asyncio.Event()
        release = asyncio.Event()

        async def on_message(_message):
            entered.set()
            await release.wait()

        client = FishAudioASRClient(
            url="wss://api.fish.audio/v1/asr/realtime",
            api_key="test-key",
            model="transcribe-stream-1",
            config_message={"type": "config"},
            on_message=on_message,
            on_close=AsyncMock(),
        )
        with patch.object(
            websocket, "connect", new=AsyncMock(return_value=socket)
        ):
            connection = asyncio.create_task(client.connect())
            try:
                await asyncio.wait_for(entered.wait(), timeout=1)
                assert not client.ready.is_set()
                assert not connection.done()
                release.set()
                await asyncio.wait_for(connection, timeout=1)
                assert client.ready.is_set()
            finally:
                release.set()
                await client.close(graceful=False)
                await asyncio.gather(connection, return_exceptions=True)

    asyncio.run(run())


def test_close_callback_preserves_connection_closed_exception_type():
    async def run():
        closed = []

        async def on_close(code, message, close_exception):
            closed.append((code, message, close_exception))

        client = FishAudioASRClient(
            url="wss://api.fish.audio/v1/asr/realtime",
            api_key="test-key",
            model="transcribe-stream-1",
            config_message={"type": "config"},
            on_message=AsyncMock(),
            on_close=on_close,
        )
        close_exception = ConnectionClosedError(None, None)
        await client._notify_close(1000, "incomplete", close_exception)

        assert closed == [(1000, "incomplete", close_exception)]

    asyncio.run(run())
