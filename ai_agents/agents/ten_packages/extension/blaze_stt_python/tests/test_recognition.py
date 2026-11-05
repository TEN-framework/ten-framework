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
from ..recognition import (
    BlazeSTTAuthError,
    BlazeSTTRecognition,
    BlazeSTTRecognitionCallback,
)
from .mock import MockBlazeSTTServer, wait_until


class RecordingCallback(BlazeSTTRecognitionCallback):
    def __init__(self):
        self.opened = 0
        self.partials: list[dict] = []
        self.finals: list[dict] = []
        self.errors: list[tuple] = []
        self.closed = 0

    async def on_open(self):
        self.opened += 1

    async def on_partial_result(self, message_data):
        self.partials.append(message_data)

    async def on_final_result(self, message_data):
        self.finals.append(message_data)

    async def on_error(self, error_msg, error_code=None):
        self.errors.append((error_msg, error_code))

    async def on_close(self):
        self.closed += 1


def _recognition(server: MockBlazeSTTServer, token: str = "test-key"):
    config = BlazeSTTConfig(
        params={
            "base_url": server.base_url,
            "api_key": token,
            "language": "vi",
            "topic": "weather",
        }
    )
    config.apply_defaults()
    callback = RecordingCallback()
    rec = BlazeSTTRecognition(
        config=config,
        audio_timeline=MagicMock(),
        ten_env=MagicMock(),
        callback=callback,
    )
    return rec, callback


def test_handshake_sends_token_and_options():
    async def _run():
        async with MockBlazeSTTServer() as server:
            rec, callback = _recognition(server)
            await rec.start()
            assert rec.is_connected()
            assert callback.opened == 1
            assert server.init_messages == [
                {
                    "token": "test-key",
                    "language": "vi",
                    "model": "stt-stream-1.5",
                    "topic": "weather",
                }
            ]
            await rec.close()

    asyncio.run(_run())


def test_bad_token_raises_auth_error():
    async def _run():
        async with MockBlazeSTTServer() as server:
            rec, callback = _recognition(server, token="wrong")
            with pytest.raises(BlazeSTTAuthError):
                await rec.start()
            assert not rec.is_connected()
            assert callback.opened == 0

    asyncio.run(_run())


def test_partial_final_and_error_events_reach_callback():
    async def script(ws, server):
        await server.send(ws, {"type": "partial", "text": "xin"})
        await server.send(
            ws,
            {
                "type": "final",
                "text": "xin chào.",
                "start_ms": 100,
                "end_ms": 900,
            },
        )
        await server.send(
            ws, {"type": "error", "code": "upstream_error", "text": "busy"}
        )
        await server.collect_audio(ws)

    async def _run():
        async with MockBlazeSTTServer(script=script) as server:
            rec, callback = _recognition(server)
            await rec.start()
            await wait_until(lambda: callback.errors)
            assert callback.partials[0]["text"] == "xin"
            assert callback.finals[0]["end_ms"] == 900
            assert rec.final_event.is_set()
            assert callback.errors == [("busy", "upstream_error")]
            await rec.close()

    asyncio.run(_run())


def test_audio_and_silence_are_streamed():
    async def _run():
        async with MockBlazeSTTServer() as server:
            rec, _ = _recognition(server)
            await rec.start()
            await rec.send_audio_frame(b"\x01\x02" * 160)  # 10 ms
            await rec.send_silence(20)  # 640 bytes at 16 kHz
            await wait_until(lambda: len(server.audio) == 320 + 640)
            assert server.audio[320:] == b"\x00" * 640
            rec.audio_timeline.add_user_audio.assert_called_once_with(10)
            rec.audio_timeline.add_silence_audio.assert_called_once_with(20)
            await rec.close()

    asyncio.run(_run())


def test_server_close_calls_on_close():
    async def script(ws, _server):
        await ws.close()

    async def _run():
        async with MockBlazeSTTServer(script=script) as server:
            rec, callback = _recognition(server)
            await rec.start()
            await wait_until(lambda: callback.closed == 1)
            assert not rec.is_connected()
            await rec.close()

    asyncio.run(_run())
