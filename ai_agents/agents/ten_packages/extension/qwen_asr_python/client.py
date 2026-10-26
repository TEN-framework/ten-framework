import asyncio
import json
from collections.abc import Awaitable, Callable
from typing import Any
from uuid import uuid4

import websockets
from websockets.protocol import State


class QwenProtocolError(RuntimeError):
    def __init__(self, message: str, code: str = "protocol_error") -> None:
        super().__init__(message)
        self.code = code


class QwenASRClient:
    def __init__(
        self,
        url: str,
        api_key: str,
        run_task_factory: Callable[[str], dict[str, Any]],
        finish_task_factory: Callable[[str], dict[str, Any]],
        event_callback: Callable[[dict[str, Any]], Awaitable[None]],
        close_callback: Callable[[str], Awaitable[None]],
        timeout_s: int,
    ) -> None:
        self.url = url
        self.api_key = api_key
        self.run_task_factory = run_task_factory
        self.finish_task_factory = finish_task_factory
        self.event_callback = event_callback
        self.close_callback = close_callback
        self.timeout_s = timeout_s
        self.websocket: Any = None
        self.reader_task: asyncio.Task[None] | None = None
        self.task_id: str | None = None
        self.task_ready = False
        self._task_started = asyncio.Event()
        self._task_finished = asyncio.Event()
        self._task_failed = asyncio.Event()
        self._task_error: QwenProtocolError | None = None
        self._send_lock = asyncio.Lock()
        self._expected_close = False

    @property
    def connected(self) -> bool:
        return self.websocket is not None and self.websocket.state is State.OPEN

    async def connect(self) -> None:
        self.websocket = await websockets.connect(
            self.url,
            additional_headers={"Authorization": f"Bearer {self.api_key}"},
            open_timeout=self.timeout_s,
        )
        self._expected_close = False
        self.reader_task = asyncio.create_task(self._read_loop())
        try:
            await self.start_task()
        except BaseException:
            await self.close()
            raise

    async def start_task(self) -> None:
        if not self.connected:
            raise QwenProtocolError("Qwen WebSocket is not connected", "not_connected")
        self.task_id = str(uuid4())
        self.task_ready = False
        self._task_started.clear()
        self._task_finished.clear()
        self._task_failed.clear()
        self._task_error = None
        await self._send_json(self.run_task_factory(self.task_id))
        await self._wait_for_task_event(self._task_started, "task-started")
        self.task_ready = True

    async def send_audio(self, audio: bytes) -> None:
        if not self.connected or not self.task_ready:
            raise QwenProtocolError("Qwen task is not ready", "task_not_ready")
        async with self._send_lock:
            await self.websocket.send(audio)

    async def finish_task(self) -> None:
        if not self.connected or not self.task_ready or not self.task_id:
            return
        self._task_finished.clear()
        self._task_failed.clear()
        self._task_error = None
        await self._send_json(self.finish_task_factory(self.task_id))
        await self._wait_for_task_event(self._task_finished, "task-finished")
        self.task_ready = False

    async def _send_json(self, payload: dict[str, Any]) -> None:
        async with self._send_lock:
            if not self.connected:
                raise QwenProtocolError(
                    "Qwen WebSocket is not connected", "not_connected"
                )
            await self.websocket.send(json.dumps(payload, ensure_ascii=False))

    async def _wait_for_task_event(self, event: asyncio.Event, event_name: str) -> None:
        event_waiter = asyncio.create_task(event.wait())
        failure_waiter = asyncio.create_task(self._task_failed.wait())
        try:
            done, pending = await asyncio.wait(
                {event_waiter, failure_waiter},
                timeout=self.timeout_s,
                return_when=asyncio.FIRST_COMPLETED,
            )
            for task in pending:
                task.cancel()
            if not done:
                raise QwenProtocolError(
                    f"timed out waiting for {event_name}", "timeout"
                )
            if failure_waiter in done:
                raise self._task_error or QwenProtocolError(
                    f"Qwen task failed before {event_name}", "task_failed"
                )
        finally:
            for task in (event_waiter, failure_waiter):
                if not task.done():
                    task.cancel()

    async def _read_loop(self) -> None:
        reason = "connection closed"
        try:
            while self.websocket is not None:
                message = await self.websocket.recv()
                if isinstance(message, bytes):
                    continue
                event = json.loads(message)
                await self._handle_event(event)
        except asyncio.CancelledError:
            return
        except Exception as exc:
            reason = str(exc)
        finally:
            if not self._expected_close:
                self.task_ready = False
                await self.close_callback(reason)

    async def _handle_event(self, event: dict[str, Any]) -> None:
        header = event.get("header") or {}
        event_name = header.get("event")
        if event_name == "task-started":
            self._task_started.set()
        elif event_name == "task-finished":
            self._task_finished.set()
        elif event_name == "task-failed":
            payload = event.get("payload") or {}
            error = (
                payload.get("error")
                or payload.get("message")
                or header.get("error_message")
                or payload
            )
            if isinstance(error, dict):
                message = str(
                    error.get("message") or error.get("error_message") or error
                )
                code = str(
                    error.get("code")
                    or error.get("error_code")
                    or header.get("error_code")
                    or "task_failed"
                )
            else:
                message = str(error)
                code = "task_failed"
            self._task_error = QwenProtocolError(message, code)
            self._task_failed.set()
        await self.event_callback(event)

    async def close(self) -> None:
        self._expected_close = True
        websocket = self.websocket
        self.websocket = None
        self.task_ready = False
        if websocket is not None:
            try:
                await websocket.close()
            except Exception:
                pass
        reader_task = self.reader_task
        self.reader_task = None
        if reader_task and reader_task is not asyncio.current_task():
            if not reader_task.done():
                reader_task.cancel()
            try:
                await reader_task
            except (asyncio.CancelledError, Exception):
                pass
