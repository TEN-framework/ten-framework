#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""In-process mock of the Blaze realtime STT WebSocket."""

import asyncio
import json
import math
import struct
from typing import Any, Awaitable, Callable

import websockets


class MockBlazeSTTServer:
    """Speaks the Blaze realtime STT handshake, then runs `script`.

    `script(ws, server)` receives the connection after "ready" is sent; it
    can read audio with `ws.recv()` and push events with `server.send()`.
    """

    def __init__(
        self,
        valid_token: str = "test-key",
        script: (
            Callable[[Any, "MockBlazeSTTServer"], Awaitable[None]] | None
        ) = None,
    ):
        self.valid_token = valid_token
        self.script = script
        self.init_messages: list[dict] = []
        self.audio = bytearray()
        self.connections = 0
        self._server = None
        self.port = 0

    @property
    def base_url(self) -> str:
        return f"http://127.0.0.1:{self.port}"

    async def __aenter__(self) -> "MockBlazeSTTServer":
        self._server = await websockets.serve(self._handler, "127.0.0.1", 0)
        self.port = self._server.sockets[0].getsockname()[1]
        return self

    async def __aexit__(self, *_exc) -> None:
        self._server.close()
        await self._server.wait_closed()

    @staticmethod
    async def send(ws, payload: dict) -> None:
        await ws.send(json.dumps(payload, ensure_ascii=False))

    async def _handler(self, ws) -> None:
        self.connections += 1
        init = json.loads(await ws.recv())
        self.init_messages.append(init)
        if init.get("token") != self.valid_token:
            await self.send(ws, {"type": "failed-authentication"})
            await ws.close(1008, "failed-authentication")
            return
        await self.send(ws, {"type": "successful-authentication"})
        await self.send(ws, {"type": "ready", "text": "ok"})
        if self.script is not None:
            await self.script(ws, self)
            return
        await self.collect_audio(ws)

    async def collect_audio(self, ws) -> None:
        try:
            async for message in ws:
                if isinstance(message, bytes):
                    self.audio += message
        except websockets.exceptions.ConnectionClosed:
            pass


def tone_pcm(duration_ms: int, sample_rate: int = 16000) -> bytes:
    """16-bit mono 440 Hz tone."""
    count = int(sample_rate * duration_ms / 1000)
    return b"".join(
        struct.pack(
            "<h", int(8000 * math.sin(2 * math.pi * 440 * i / sample_rate))
        )
        for i in range(count)
    )


def silence_pcm(duration_ms: int, sample_rate: int = 16000) -> bytes:
    return b"\x00\x00" * int(sample_rate * duration_ms / 1000)


def frames(pcm: bytes, frame_ms: int = 20, sample_rate: int = 16000):
    size = int(sample_rate * 2 * frame_ms / 1000)
    for i in range(0, len(pcm), size):
        yield pcm[i : i + size]


async def wait_until(predicate: Callable[[], bool], timeout: float = 2.0):
    deadline = asyncio.get_running_loop().time() + timeout
    while not predicate():
        if asyncio.get_running_loop().time() > deadline:
            raise AssertionError("condition not met in time")
        await asyncio.sleep(0.01)
