#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""BlazeSTTRecognition against a mock Blaze realtime WebSocket."""

import asyncio
from unittest.mock import MagicMock

import pytest

from ..config import BlazeSTTConfig
from ..recognition import BlazeSTTAuthError, BlazeSTTRecognition
from .mock import MockBlazeSTTServer, wait_until


class Recorder:
    def __init__(self):
        self.messages: list[dict] = []
        self.closes: list[tuple] = []

    async def on_message(self, event):
        self.messages.append(event)

    async def on_close(self, code, message, close_exception):
        self.closes.append((code, message, close_exception))


def _client(server: MockBlazeSTTServer, token: str = "test-key"):
    config = BlazeSTTConfig(
        params={
            "base_url": server.base_url,
            "api_key": token,
            "language": "vi",
            "topic": "weather",
        }
    )
    config.apply_defaults()
    recorder = Recorder()
    client = BlazeSTTRecognition(
        config=config,
        ten_env=MagicMock(),
        on_message=recorder.on_message,
        on_close=recorder.on_close,
    )
    return client, recorder


def test_handshake_sends_token_and_options():
    async def _run():
        async with MockBlazeSTTServer() as server:
            client, _ = _client(server)
            await client.connect()
            assert client.connected
            assert server.init_messages == [
                {
                    "token": "test-key",
                    "language": "vi",
                    "model": "stt-stream-1.5",
                    "topic": "weather",
                }
            ]
            await client.close()

    asyncio.run(_run())


def test_bad_token_raises_auth_error():
    async def _run():
        async with MockBlazeSTTServer() as server:
            client, recorder = _client(server, token="wrong")
            with pytest.raises(BlazeSTTAuthError):
                await client.connect()
            assert not client.connected
            assert recorder.closes == []

    asyncio.run(_run())


def test_events_are_delivered_in_order():
    async def script(ws, server):
        await server.send(ws, {"type": "partial", "text": "xin"})
        await server.send(
            ws,
            {"type": "final", "text": "xin chào.", "start_ms": 1, "end_ms": 9},
        )
        await server.send(
            ws, {"type": "error", "code": "upstream_error", "text": "busy"}
        )
        await server.collect_audio(ws)

    async def _run():
        async with MockBlazeSTTServer(script=script) as server:
            client, recorder = _client(server)
            await client.connect()
            await wait_until(lambda: len(recorder.messages) == 3)
            assert [m["type"] for m in recorder.messages] == [
                "partial",
                "final",
                "error",
            ]
            await client.close()

    asyncio.run(_run())


def test_audio_is_streamed_as_binary():
    async def _run():
        async with MockBlazeSTTServer() as server:
            client, _ = _client(server)
            await client.connect()
            await client.send_audio(b"\x01\x02" * 160)
            await wait_until(lambda: len(server.audio) == 320)
            await client.close()

    asyncio.run(_run())


def test_server_close_reports_code_once():
    async def script(ws, _server):
        await ws.close(1011, "internal error")

    async def _run():
        async with MockBlazeSTTServer(script=script) as server:
            client, recorder = _client(server)
            await client.connect()
            await wait_until(lambda: recorder.closes)
            await asyncio.sleep(0.05)
            assert len(recorder.closes) == 1
            assert recorder.closes[0][0] == 1011
            assert not client.connected
            with pytest.raises(ConnectionError):
                await client.send_audio(b"\x00\x00")

    asyncio.run(_run())


def test_local_close_does_not_report_a_vendor_close():
    async def _run():
        async with MockBlazeSTTServer() as server:
            client, recorder = _client(server)
            await client.connect()
            await client.close()
            await asyncio.sleep(0.05)
            assert recorder.closes == []

    asyncio.run(_run())
