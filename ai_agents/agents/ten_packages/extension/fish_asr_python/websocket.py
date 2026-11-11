"""Small MessagePack WebSocket client for Fish Audio realtime ASR."""

import asyncio
import inspect
import time
from collections.abc import Awaitable, Callable
from typing import Any

import ormsgpack
from websockets.asyncio.client import ClientConnection, connect
from websockets.exceptions import (
    ConnectionClosed,
    ConnectionClosedError,
    ConnectionClosedOK,
)


MessageCallback = Callable[[dict[str, Any]], Awaitable[None]]
CloseCallback = Callable[..., Awaitable[None]]


def pack_message(message: dict[str, Any]) -> bytes:
    """Encode a protocol map as a binary MessagePack frame."""
    return ormsgpack.packb(message)


def unpack_message(message: bytes | bytearray | memoryview) -> dict[str, Any]:
    """Decode a Fish Audio binary frame."""
    value = ormsgpack.unpackb(message)
    if not isinstance(value, dict):
        raise ValueError("Fish Audio message must be a MessagePack map")
    return value


class FishAudioASRClient:
    """Own one Fish Audio realtime ASR WebSocket session."""

    def __init__(
        self,
        url: str,
        api_key: str,
        model: str,
        config_message: dict[str, Any],
        on_message: MessageCallback,
        on_close: CloseCallback,
    ) -> None:
        self.url = url
        self.api_key = api_key
        self.model = model
        self.config_message = config_message
        self.on_message = on_message
        self.on_close = on_close
        self.websocket: ClientConnection | None = None
        self.receive_task: asyncio.Task[None] | None = None
        self.connected = False
        self.ready = asyncio.Event()
        self.session_done = asyncio.Event()
        self._send_lock = asyncio.Lock()
        self._closing = False
        self.connection_start_ms = 0

    async def connect(self) -> None:
        """Open the socket, send config, and wait for session_started."""
        self.connection_start_ms = int(time.time() * 1000)
        self._closing = False
        self.ready.clear()
        self.session_done.clear()
        try:
            self.websocket = await connect(
                self.url,
                additional_headers={
                    "Authorization": f"Bearer {self.api_key}",
                    "model": self.model,
                },
                max_size=2**20,
                ping_interval=20,
                open_timeout=10,
            )
            await self.websocket.send(pack_message(self.config_message))
            self.receive_task = asyncio.create_task(self._receive_loop())
            await asyncio.wait_for(self.ready.wait(), timeout=10.0)
            if not self.connected:
                raise ConnectionError("Fish Audio did not start an ASR session")
        except BaseException:
            await self.close(graceful=False)
            raise

    async def _receive_loop(self) -> None:
        code = 0
        message = "closed"
        close_exception: BaseException | None = None
        try:
            assert self.websocket is not None
            async for frame in self.websocket:
                if isinstance(frame, str):
                    raise ValueError("Fish Audio returned a text frame")
                event = unpack_message(frame)
                if event.get("type") == "session_started":
                    self.connected = True
                if event.get("type") == "session_done":
                    self.session_done.set()
                await self.on_message(event)
                if event.get("type") == "session_started":
                    self.ready.set()
        except ConnectionClosedOK as error:
            close_exception = error
            code = error.code
            message = error.reason or "closed"
        except ConnectionClosedError as error:
            close_exception = error
            code = error.code
            message = error.reason or "closed"
        except ConnectionClosed as error:
            close_exception = error
            code = error.code
            message = error.reason or "closed"
        except Exception as error:
            close_exception = error
            code = -1
            message = str(error)
            if not self.ready.is_set():
                self.ready.set()
        finally:
            # A rejected config is reported as an error and followed by a
            # close frame.  Wake connect() immediately so it can surface the
            # vendor rejection instead of waiting for its ten-second timeout.
            if not self.ready.is_set():
                self.ready.set()
            self.connected = False
            if not self._closing:
                await self._notify_close(code, message, close_exception)

    async def _notify_close(
        self,
        code: int,
        message: str,
        close_exception: BaseException | None,
    ) -> None:
        """Notify callers and retain compatibility with old hooks."""
        try:
            parameters = list(
                inspect.signature(self.on_close).parameters.values()
            )
        except (TypeError, ValueError):
            parameters = []

        positional = [
            parameter
            for parameter in parameters
            if parameter.kind
            in (
                inspect.Parameter.POSITIONAL_ONLY,
                inspect.Parameter.POSITIONAL_OR_KEYWORD,
            )
        ]
        accepts_varargs = any(
            parameter.kind == inspect.Parameter.VAR_POSITIONAL
            for parameter in parameters
        )
        accepts_keyword = any(
            parameter.kind == inspect.Parameter.VAR_KEYWORD
            for parameter in parameters
        ) or "close_exception" in {parameter.name for parameter in parameters}

        if accepts_varargs or len(positional) >= 3:
            await self.on_close(code, message, close_exception)
        elif accepts_keyword:
            await self.on_close(code, message, close_exception=close_exception)
        else:
            await self.on_close(code, message)

    async def send(self, message: dict[str, Any]) -> None:
        """Send one MessagePack protocol map over the active socket."""
        async with self._send_lock:
            if not self.websocket or not self.connected:
                raise ConnectionError("Fish Audio WebSocket is not ready")
            await self.websocket.send(pack_message(message))

    async def send_audio(self, audio: bytes) -> None:
        """Send one raw PCM audio message."""
        await self.send({"type": "audio", "data": audio})

    async def finalize(self) -> None:
        """Ask Fish Audio to commit the current turn."""
        await self.send({"type": "finalize"})

    async def keepalive(self) -> None:
        """Keep an idle realtime session open."""
        await self.send({"type": "keepalive"})

    async def close(self, graceful: bool = True) -> None:
        """Close the session, optionally waiting for ``session_done``."""
        if self._closing:
            return
        self._closing = True
        if graceful and self.websocket and self.connected:
            try:
                await self.send({"type": "audio_done"})
                await asyncio.wait_for(self.session_done.wait(), timeout=3.0)
            except Exception:
                pass
        if self.websocket:
            try:
                await self.websocket.close()
            except Exception:
                pass
        task = self.receive_task
        self.receive_task = None
        if task and task is not asyncio.current_task() and not task.done():
            task.cancel()
            await asyncio.gather(task, return_exceptions=True)
        self.websocket = None
        self.connected = False
