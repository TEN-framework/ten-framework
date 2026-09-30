import asyncio
import secrets
from collections.abc import AsyncIterator, Callable

import ormsgpack
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_runtime import AsyncTenEnv
from websockets.asyncio.client import ClientConnection, connect
from websockets.exceptions import ConnectionClosed, WebSocketException

from .config import FishAudioTTSConfig

EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_INVALID_KEY_ERROR = 4
EVENT_TTS_FLUSH = 5


class FishAudioTTSClient:
    def __init__(
        self,
        config: FishAudioTTSConfig,
        ten_env: AsyncTenEnv,
        on_request_start: Callable[[], None] | None = None,
    ):
        self.config = config
        self.ten_env = ten_env
        self.on_request_start = on_request_start
        self._traceparent = ""
        self._response_headers: dict[str, str] = {}
        self._websocket: ClientConnection | None = None
        self._sender_task: asyncio.Task[None] | None = None
        self._connect_task: asyncio.Task[ClientConnection] | None = None
        self._is_cancelled = False

    @property
    def websocket_url(self) -> str:
        base_url = self.config.base_url.strip()
        if not base_url:
            base_url = "wss://api.fish.audio"
        elif base_url.startswith("https://"):
            base_url = "wss://" + base_url.removeprefix("https://")
        elif base_url.startswith("http://"):
            base_url = "ws://" + base_url.removeprefix("http://")

        return f"{base_url.rstrip('/')}/v1/tts/live/with-timestamp"

    @staticmethod
    def _new_traceparent() -> str:
        trace_id = secrets.token_hex(16)
        span_id = secrets.token_hex(8)
        return f"00-{trace_id}-{span_id}-01"

    def _build_headers(self) -> dict[str, str]:
        self._traceparent = self._new_traceparent()
        model = {
            "speech-1.5": "s2.1-pro",
        }.get(self.config.backend, self.config.backend)
        return {
            "Authorization": f"Bearer {self.config.api_key}",
            "model": model,
            "traceparent": self._traceparent,
        }

    def _build_request(self) -> dict[str, object]:
        request: dict[str, object] = {
            "text": "",
            "chunk_length": 200,
        }
        request.update(self.config.params)
        request["text"] = ""
        request["chunk_length"] = 200
        return request

    @staticmethod
    def _headers_to_dict(headers) -> dict[str, str]:
        raw_items = getattr(headers, "raw_items", None)
        items = raw_items() if callable(raw_items) else headers.items()
        normalized: dict[str, str] = {}
        for name, value in items:
            name = name.lower()
            if name in normalized:
                normalized[name] = f"{normalized[name]}, {value}"
            else:
                normalized[name] = value
        return normalized

    async def _capture_response_headers(self, websocket: ClientConnection) -> None:
        response = getattr(websocket, "response", None)
        headers = getattr(response, "headers", None)
        if headers is None:
            headers = getattr(websocket, "response_headers", {})

        self._response_headers = self._headers_to_dict(headers)
        datacenter = self._response_headers.get("x-fishaudio-datacenter", "")
        trace_id = self._traceparent.split("-")[1]
        self.ten_env.log_info(
            "FishAudioTTS: WebSocket response headers: "
            f"{self._response_headers}; "
            f"x-fishaudio-datacenter={datacenter}; trace_id={trace_id}"
        )

    async def _send_request(
        self,
        websocket: ClientConnection,
        text: str,
    ) -> None:
        await websocket.send(
            ormsgpack.packb(
                {"event": "start", "request": self._build_request()}
            )
        )
        await websocket.send(ormsgpack.packb({"event": "text", "text": text}))
        await websocket.send(ormsgpack.packb({"event": "stop"}))

    async def _close_active_connection(
        self,
        websocket: ClientConnection | None = None,
        sender_task: asyncio.Task[None] | None = None,
    ) -> None:
        if sender_task is None:
            sender_task = self._sender_task
        if sender_task is self._sender_task:
            self._sender_task = None
        if sender_task is not None and not sender_task.done():
            sender_task.cancel()
            await asyncio.gather(sender_task, return_exceptions=True)

        if websocket is None:
            websocket = self._websocket
        if websocket is self._websocket:
            self._websocket = None
        if websocket is not None:
            try:
                await websocket.close()
            except Exception as close_error:
                self.ten_env.log_warn(
                    "FishAudioTTS: failed to close WebSocket: "
                    f"type={type(close_error).__name__}, "
                    f"message={str(close_error)}"
                )

    async def get(self, text: str) -> AsyncIterator[tuple[bytes | None, int]]:
        """Stream one text request through the timestamped WebSocket."""
        self._is_cancelled = False
        headers = self._build_headers()
        active_websocket: ClientConnection | None = None
        sender_task: asyncio.Task[None] | None = None
        connect_context = None
        connect_task: asyncio.Task[ClientConnection] | None = None
        context_entered = False

        try:
            if self.on_request_start is not None:
                self.on_request_start()

            connect_context = connect(
                self.websocket_url, additional_headers=headers
            )
            connect_task = asyncio.create_task(connect_context.__aenter__())
            self._connect_task = connect_task
            websocket = await connect_task
            context_entered = True
            if self._connect_task is connect_task:
                self._connect_task = None
            try:
                active_websocket = websocket
                self._websocket = websocket
                await self._capture_response_headers(websocket)
                sender_task = asyncio.create_task(
                    self._send_request(websocket, text)
                )
                self._sender_task = sender_task

                while True:
                    message = await websocket.recv()
                    if isinstance(message, str):
                        raise RuntimeError(
                            "Fish Audio returned a text WebSocket message"
                        )

                    data = ormsgpack.unpackb(message)
                    event = data.get("event")
                    if event == "audio":
                        audio = data.get("audio")
                        if isinstance(audio, bytes) and audio:
                            yield audio, EVENT_TTS_RESPONSE
                    elif event == "error":
                        raise RuntimeError(str(data.get("error", "Unknown error")))
                    elif event == "finish":
                        break

                await sender_task
                if self._sender_task is sender_task:
                    self._sender_task = None
            finally:
                await connect_context.__aexit__(None, None, None)

            if not self._is_cancelled:
                yield None, EVENT_TTS_END
        except asyncio.CancelledError:
            if self._is_cancelled:
                yield None, EVENT_TTS_FLUSH
                return
            raise
        except (ConnectionClosed, WebSocketException, OSError) as error:
            if self._is_cancelled:
                yield None, EVENT_TTS_FLUSH
                return
            self._log_vendor_error(error)
            yield self._error_event(error)
        except Exception as error:
            if self._is_cancelled:
                yield None, EVENT_TTS_FLUSH
                return

            self._log_vendor_error(error)
            yield self._error_event(error)
        finally:
            if self._connect_task is connect_task:
                self._connect_task = None
            if connect_task is not None and not connect_task.done():
                connect_task.cancel()
                await asyncio.gather(connect_task, return_exceptions=True)
            if connect_context is not None and not context_entered:
                await connect_context.__aexit__(None, None, None)
            await self._close_active_connection(active_websocket, sender_task)

    @staticmethod
    def _error_event(error: Exception) -> tuple[bytes, int]:
        error_message = str(error)
        if "402" in error_message or "Payment Required" in error_message:
            return error_message.encode("utf-8"), EVENT_TTS_INVALID_KEY_ERROR
        return error_message.encode("utf-8"), EVENT_TTS_ERROR

    def _log_vendor_error(self, error: Exception) -> None:
        self.ten_env.log_error(
            "vendor_error: "
            f"type={type(error).__name__}, message={str(error)}",
            category=LOG_CATEGORY_VENDOR,
        )

    async def cancel(self) -> None:
        self.ten_env.log_debug("FishAudioTTS: cancel() called.")
        self._is_cancelled = True
        connect_task = self._connect_task
        if connect_task is not None and not connect_task.done():
            connect_task.cancel()
            await asyncio.gather(connect_task, return_exceptions=True)
        await self._close_active_connection()

    async def clean(self) -> None:
        self.ten_env.log_debug("FishAudioTTS: clean() called.")
        self._is_cancelled = True
        await self._close_active_connection()
