#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Blaze realtime STT WebSocket client.

Protocol (wss://api.blaze.vn/v1/stt/realtime, model stt-stream-1.5):
  1. Send JSON {"token", "language", "model", ["topic"], ["context"]}.
  2. Receive {"type": "successful-authentication"} then {"type": "ready"}.
     A bad token gets {"type": "failed-authentication"} and close code 1008.
  3. Stream binary PCM16 mono at 16 kHz.
  4. Receive {"type": "partial", "text"} and
     {"type": "final", "text", "start_ms", "end_ms"}; errors arrive as
     {"type": "error", "code", "text"}.
Blaze ends utterances with server-side endpointing; it has no flush
message, so callers send silence to close the current utterance.

The client owns one socket and its receive loop. It never reconnects: it
reports messages and the final close through callbacks, and the extension
owns recovery on a separate task.
"""

import asyncio
import json
from collections.abc import Awaitable, Callable
from typing import Any

import websockets
from websockets.asyncio.client import ClientConnection
from websockets.exceptions import ConnectionClosed

from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_runtime import AsyncTenEnv

from .config import BlazeSTTConfig
from .const import READY_TIMEOUT_SECONDS, WS_CLOSE_POLICY_VIOLATION

MessageCallback = Callable[[dict[str, Any]], Awaitable[None]]
# on_close(code, message, close_exception)
CloseCallback = Callable[[int, str, BaseException | None], Awaitable[None]]


class BlazeSTTAuthError(Exception):
    """Raised when Blaze rejects the token."""

    status_code = 401


class BlazeSTTRecognition:
    """One Blaze realtime STT WebSocket session."""

    def __init__(
        self,
        config: BlazeSTTConfig,
        ten_env: AsyncTenEnv,
        on_message: MessageCallback,
        on_close: CloseCallback,
    ):
        self.config = config
        self.ten_env = ten_env
        self.on_message = on_message
        self.on_close = on_close
        self.websocket: ClientConnection | None = None
        self.receive_task: asyncio.Task[None] | None = None
        self.connected = False
        self._send_lock = asyncio.Lock()
        self._closing = False

    async def connect(self, timeout: float = READY_TIMEOUT_SECONDS) -> None:
        """Open the socket, authenticate, and wait for "ready"."""
        url = self.config.ws_url()
        self.ten_env.log_info(
            f"vendor_status: connecting to Blaze STT {url}",
            category=LOG_CATEGORY_VENDOR,
        )
        try:
            self.websocket = await websockets.connect(
                url, open_timeout=timeout, max_size=None
            )
            await self.websocket.send(json.dumps(self.config.init_message()))
            await self._wait_ready(timeout)
        except ConnectionClosed as e:
            await self.close()
            if e.rcvd and e.rcvd.code == WS_CLOSE_POLICY_VIOLATION:
                raise BlazeSTTAuthError(
                    e.rcvd.reason or "Blaze STT authentication failed"
                ) from e
            raise
        except BaseException:
            await self.close()
            raise
        self.connected = True
        self.receive_task = asyncio.create_task(self._receive_loop())

    async def _wait_ready(self, timeout: float) -> None:
        assert self.websocket is not None
        while True:
            raw = await asyncio.wait_for(self.websocket.recv(), timeout=timeout)
            if isinstance(raw, bytes):
                continue
            event = json.loads(raw)
            event_type = event.get("type")
            self.ten_env.log_debug(
                f"vendor_result: startup: {event_type}",
                category=LOG_CATEGORY_VENDOR,
            )
            if event_type == "ready":
                return
            if event_type == "failed-authentication":
                raise BlazeSTTAuthError(
                    event.get("message") or "Blaze STT authentication failed"
                )
            if event_type == "error":
                raise RuntimeError(
                    f"Blaze STT startup error: {event.get('text') or event}"
                )

    async def _receive_loop(self) -> None:
        code = 1000
        message = "closed"
        close_exception: BaseException | None = None
        try:
            assert self.websocket is not None
            async for raw in self.websocket:
                if isinstance(raw, bytes):
                    continue
                await self.on_message(json.loads(raw))
        except ConnectionClosed as e:
            close_exception = e
            if e.rcvd is not None:
                code = e.rcvd.code
                message = e.rcvd.reason or "closed"
            else:
                code = 1006
                message = str(e)
        # CancelledError is not an Exception: a cancelled loop exits here
        # without reporting a vendor close.
        except Exception as e:  # pylint: disable=broad-exception-caught
            close_exception = e
            code = -1
            message = f"Blaze STT receive error: {e}"
        else:
            # Iteration ends quietly only after a normal close handshake
            # (websockets raises ConnectionClosedError for abnormal ones).
            websocket = self.websocket
            if websocket is not None and websocket.close_code is not None:
                code = websocket.close_code
                message = websocket.close_reason or "closed"
        finally:
            self.connected = False
        if not self._closing:
            await self.on_close(code, message, close_exception)

    async def send_audio(self, audio: bytes) -> None:
        async with self._send_lock:
            if not self.connected or self.websocket is None:
                raise ConnectionError("Blaze STT WebSocket is not ready")
            await self.websocket.send(audio)

    async def close(self) -> None:
        """Close the socket and stop the receive loop without callbacks."""
        self._closing = True
        self.connected = False
        websocket = self.websocket
        self.websocket = None
        if websocket is not None:
            try:
                await websocket.close()
            except Exception as e:  # pylint: disable=broad-exception-caught
                self.ten_env.log_info(f"Error closing websocket: {e}")
        task = self.receive_task
        self.receive_task = None
        if task and task is not asyncio.current_task() and not task.done():
            task.cancel()
            await asyncio.gather(task, return_exceptions=True)
