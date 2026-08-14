#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from collections.abc import Awaitable, Callable
from contextlib import suppress
import json
from typing import Any, Protocol
import uuid

import websockets

from .config import IFlytekAsrConfig
from .protocol import (
    STATUS_CONTINUE,
    STATUS_FIRST,
    IFlytekResponse,
    build_audio_request,
    build_finalize_request,
    parse_response,
)


class IFlytekAsrClientListener(Protocol):
    async def on_response(self, response: IFlytekResponse) -> None: ...

    async def on_error(self, error: Exception) -> None: ...

    async def on_closed(self, terminal_received: bool) -> None: ...


ConnectCallable = Callable[..., Awaitable[Any]]


class IFlytekAsrClient:
    def __init__(
        self,
        config: IFlytekAsrConfig,
        listener: IFlytekAsrClientListener,
        *,
        connect: ConnectCallable = websockets.connect,
    ) -> None:
        self.config = config
        self.listener = listener
        self._connect = connect
        self._websocket: Any | None = None
        self._receive_task: asyncio.Task[None] | None = None
        self._connection_lock = asyncio.Lock()
        self._send_lock = asyncio.Lock()
        self._trace_id = ""
        self._first_frame = True
        self._finalizing = False

    async def start(self) -> None:
        async with self._connection_lock:
            if self.is_connected():
                return

            self._trace_id = f"{self.config.trace_id_prefix}-{uuid.uuid4().hex}"
            websocket = await self._connect(
                self.config.url,
                open_timeout=self.config.connect_timeout,
            )
            self._websocket = websocket
            self._first_frame = True
            self._finalizing = False
            self._receive_task = asyncio.create_task(
                self._receive_loop(websocket)
            )

    async def stop(self) -> None:
        async with self._connection_lock:
            websocket = self._websocket
            receive_task = self._receive_task
            self._websocket = None
            self._receive_task = None

            if websocket is not None:
                await websocket.close()

            current_task = asyncio.current_task()
            if receive_task is not None and receive_task is not current_task:
                receive_task.cancel()
                with suppress(asyncio.CancelledError):
                    await receive_task

    def is_connected(self) -> bool:
        websocket = self._websocket
        if websocket is None:
            return False
        if bool(getattr(websocket, "closed", False)):
            return False
        state = getattr(websocket, "state", None)
        if state is not None:
            return getattr(state, "name", "") == "OPEN"
        return True

    async def send_audio(self, audio: bytes) -> bool:
        async with self._send_lock:
            if not self.is_connected() or self._finalizing:
                return False
            websocket = self._websocket
            if websocket is None:
                return False

            status = STATUS_FIRST if self._first_frame else STATUS_CONTINUE
            request = build_audio_request(
                config=self.config,
                trace_id=self._trace_id,
                status=status,
                audio=audio,
            )
            await websocket.send(
                json.dumps(request, ensure_ascii=False, separators=(",", ":"))
            )
            self._first_frame = False
            return True

    async def finalize(self) -> bool:
        async with self._send_lock:
            if not self.is_connected() or self._finalizing:
                return False
            websocket = self._websocket
            if websocket is None:
                return False

            request = build_finalize_request(self.config, self._trace_id)
            await websocket.send(
                json.dumps(request, ensure_ascii=False, separators=(",", ":"))
            )
            self._finalizing = True
            return True

    async def _receive_loop(self, websocket: Any) -> None:
        terminal_received = False
        try:
            async for message in websocket:
                response = parse_response(
                    message, self.config.output_language()
                )
                await self.listener.on_response(response)
                if response.terminal:
                    terminal_received = True
                    break
        except Exception as error:
            await self.listener.on_error(error)
        finally:
            close_error: Exception | None = None
            try:
                await websocket.close()
            except Exception as error:
                close_error = error

            is_current_connection = self._websocket is websocket
            if is_current_connection:
                self._websocket = None
                self._receive_task = None
                if close_error is not None:
                    await self.listener.on_error(close_error)
                await self.listener.on_closed(terminal_received)
