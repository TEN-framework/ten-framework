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


class FakeFrame:
    """Minimal AudioFrame stand-in with optional session metadata."""

    def __init__(self, data: bytes, session_id: str | None = "s1"):
        self.buf = bytearray(data)
        self.session_id = session_id
        self.unlocked = 0

    def get_buf(self):
        return self.buf

    def lock_buf(self):
        return self.buf

    def unlock_buf(self, _buf):
        self.unlocked += 1

    def get_property_to_json(self, _key):
        if self.session_id is None:
            return "", None
        return json.dumps({"session_id": self.session_id}), None


class FakeClient:
    """Stands in for BlazeSTTRecognition inside extension tests.

    `connect_errors` is consumed one per connect(); `server_close()` plays
    the receive loop reporting a closed socket.
    """

    instances: list["FakeClient"] = []
    connect_errors: list[BaseException] = []

    def __init__(self, config, ten_env, on_message, on_close):
        del config, ten_env
        self.on_message = on_message
        self.on_close = on_close
        self.connected = False
        self.closed = False
        self.sent: list[bytes] = []
        self.send_error: BaseException | None = None
        FakeClient.instances.append(self)

    async def connect(self):
        if FakeClient.connect_errors:
            raise FakeClient.connect_errors.pop(0)
        self.connected = True

    async def send_audio(self, audio: bytes):
        if self.send_error is not None:
            raise self.send_error
        self.sent.append(audio)

    async def close(self):
        self.closed = True
        self.connected = False

    async def server_close(self, code: int = 1006, exc=None):
        self.connected = False
        await self.on_close(code, "closed", exc)


def reset_fake_client(connect_errors=None) -> None:
    FakeClient.instances = []
    FakeClient.connect_errors = list(connect_errors or [])


def make_extension(**params):
    """A configured extension with an awaitable ten_env.send_data."""
    from unittest.mock import AsyncMock, MagicMock

    from ..config import BlazeSTTConfig
    from ..extension import BlazeSTTExtension

    extension = BlazeSTTExtension("blaze_stt_python")
    env = MagicMock()
    env.send_data = AsyncMock()
    extension.ten_env = env
    extension.config = BlazeSTTConfig(
        finalize_silence_ms=params.pop("finalize_silence_ms", 100),
        finalize_timeout_ms=params.pop("finalize_timeout_ms", 300),
        params={"api_key": "test-key", "language": "vi", **params},
    )
    extension.config.apply_defaults()
    extension.metadata = {"session_id": "s1"}
    extension.session_id = "s1"
    return extension, env


def outputs(env, name: str) -> list[dict]:
    """JSON payloads of every Data the extension sent with `name`."""
    return [
        json.loads(call.args[0].get_property_to_json("")[0])
        for call in env.send_data.call_args_list
        if call.args[0].get_name() == name
    ]


def connection_states(env) -> list[str]:
    return [
        event["current"] for event in outputs(env, "connection_status_changed")
    ]


def finalize_data(finalize_id: str, session_id: str):
    from ten_runtime import Data

    data = Data.create("asr_finalize")
    data.set_property_from_json(
        None,
        json.dumps(
            {"finalize_id": finalize_id, "metadata": {"session_id": session_id}}
        ),
    )
    return data


def patch_client(connect_errors=None):
    """Use FakeClient and zero reconnect backoff inside a `with` block."""
    import contextlib
    from unittest.mock import patch

    from .. import extension as extension_module

    reset_fake_client(connect_errors)
    stack = contextlib.ExitStack()
    stack.enter_context(
        patch.object(extension_module, "BlazeSTTRecognition", FakeClient)
    )
    stack.enter_context(
        patch.object(extension_module, "RECONNECT_BASE_DELAY_SECONDS", 0)
    )
    return stack


async def vendor_event(extension, event: dict) -> None:
    """Deliver a Blaze message as the current socket's receive loop would."""
    await extension._handle_message(event, generation=extension._generation)
