import asyncio
import base64
import json
from time import monotonic
from typing import Any, Optional
from urllib.parse import urlencode, urlsplit, urlunsplit, parse_qsl

import websockets
from websockets.exceptions import (
    ConnectionClosed,
    ConnectionClosedError,
    ConnectionClosedOK,
)
from websockets.protocol import State

from ten_ai_base.const import LOG_CATEGORY_TRANSCRIPTS, LOG_CATEGORY_VENDOR
from ten_ai_base.timeline import AudioTimeline
from ten_ai_base.utils import redact_url
from ten_runtime import AsyncTenEnv


class ElevenLabsWSRecognitionCallback:
    """Callbacks from the websocket client into the TEN extension."""

    async def on_open(
        self,
        generation: int | None = None,
        session_id: str | None = None,
        connect_delay_ms: int | None = None,
    ) -> None:
        del generation, session_id, connect_delay_ms

    async def on_result(
        self, message_data: dict[str, Any], generation: int | None = None
    ) -> None:
        del message_data, generation

    async def on_event(
        self, message_data: dict[str, Any], generation: int | None = None
    ) -> None:
        del message_data, generation

    async def on_error(
        self,
        error_msg: str,
        error_code: Optional[str] = None,
        generation: int | None = None,
    ) -> None:
        del error_msg, error_code, generation

    async def on_close(
        self,
        vendor_code: int = 0,
        vendor_message: str = "closed",
        generation: int | None = None,
        normal: bool = False,
    ) -> None:
        del vendor_code, vendor_message, generation, normal


class ElevenLabsWSRecognition:
    """Websocket client for ElevenLabs Scribe Realtime v2."""

    def __init__(
        self,
        audio_timeline: AudioTimeline,
        ten_env: AsyncTenEnv,
        params: Optional[dict[str, Any]] = None,
        callback: Optional[ElevenLabsWSRecognitionCallback] = None,
        generation: int = 0,
    ):
        self.audio_timeline = audio_timeline
        self.ten_env = ten_env
        self.params = dict(params or {})
        self.callback = callback
        self.generation = generation
        self.websocket = None
        self.is_started = False
        self.session_id: str | None = None
        self._message_task: asyncio.Task | None = None
        self._closed = False
        self._session_started = asyncio.Event()
        self._start_error: tuple[str, str] | None = None
        self._send_lock = asyncio.Lock()
        self._connection_started_at = 0.0

        self.api_key = self.params.get("api_key", "")
        self.ws_url = self.params.get("ws_url", "")
        self.sample_rate = int(self.params.get("sample_rate", 16000))
        self.channels = 1
        self.sample_width = (
            1 if self.params.get("audio_format") == "ulaw_8000" else 2
        )

    def _log(self, message: str, category: str = LOG_CATEGORY_VENDOR) -> None:
        self.ten_env.log_info(message, category=category)

    @staticmethod
    def _close_details(exc: ConnectionClosed) -> tuple[int, str]:
        """Read close details without using deprecated exception properties."""
        received = getattr(exc, "rcvd", None)
        if received is None:
            received = getattr(exc, "sent", None)
        if received is None:
            return 0, "closed"
        return (
            int(getattr(received, "code", 0)),
            getattr(received, "reason", "") or "closed",
        )

    @staticmethod
    def _is_idle_close(close_code: int, close_message: str) -> bool:
        """Return whether the vendor closed an otherwise idle session."""
        message = (close_message or "").lower()
        return close_code in {1000, 1001} or any(
            marker in message
            for marker in ("idle", "inactiv", "no message", "timeout")
        )

    def _build_websocket_url(self) -> str:
        """Build an encoded URL with only vendor query parameters."""
        excluded = {
            "api_key",
            "ws_url",
            # TEN's generic public language property is translated to the
            # provider's language_code by the config layer.
            "language",
            "sample_rate",
            "dump",
            "dump_path",
            "connect_timeout",
            "finalize_timeout",
        }
        query: list[tuple[str, str]] = []
        for key, value in self.params.items():
            if key in excluded or value is None:
                continue
            if isinstance(value, bool):
                query.append((key, str(value).lower()))
            elif isinstance(value, (list, tuple)):
                for item in value:
                    if item is not None:
                        query.append((key, str(item)))
            elif value != "":
                query.append((key, str(value)))

        parts = urlsplit(self.ws_url)
        existing = parse_qsl(parts.query, keep_blank_values=True)
        encoded_query = urlencode(existing + query, doseq=True)
        return urlunsplit(
            (
                parts.scheme,
                parts.netloc,
                parts.path,
                encoded_query,
                parts.fragment,
            )
        )

    async def _handle_message(self, message: str) -> None:
        try:
            message_data = json.loads(message)
            message_type = message_data.get("message_type", "")
            self.ten_env.log_debug(
                f"vendor_result: {message_type}",
                category=LOG_CATEGORY_TRANSCRIPTS,
            )

            if message_type == "session_started":
                self.session_id = message_data.get("session_id")
                self.is_started = True
                self._session_started.set()
                delay_ms = int(
                    max(0.0, monotonic() - self._connection_started_at) * 1000
                )
                if self.callback:
                    await self.callback.on_open(
                        self.generation, self.session_id, delay_ms
                    )
                return

            if message_type in {
                "partial_transcript",
                "committed_transcript",
                "committed_transcript_with_timestamps",
            }:
                if self.callback:
                    await self.callback.on_result(message_data, self.generation)
                return

            if message_type in {
                "auth_error",
                "quota_exceeded",
                "transcriber_error",
                "input_error",
                "invalid_request",
                "error",
                "commit_throttled",
                "unaccepted_terms",
                "rate_limited",
                "queue_overflow",
                "resource_exhausted",
                "session_time_limit_exceeded",
                "chunk_size_exceeded",
                "insufficient_audio_activity",
            }:
                error = str(message_data.get("error", message_type))
                self._start_error = (message_type, error)
                self._session_started.set()
                if self.callback:
                    await self.callback.on_error(
                        error, message_type, self.generation
                    )
                return

            if self.callback:
                await self.callback.on_event(message_data, self.generation)
        except json.JSONDecodeError as exc:
            error = f"invalid ElevenLabs JSON response: {exc}"
            self._start_error = ("invalid_response", error)
            self._session_started.set()
            if self.callback:
                await self.callback.on_error(
                    error, "invalid_response", self.generation
                )
        except Exception as exc:
            error = f"error processing ElevenLabs response: {exc}"
            if self.callback:
                await self.callback.on_error(
                    error, "response_error", self.generation
                )

    async def _message_handler(self) -> None:
        close_code = 0
        close_message = "closed"
        normal = False
        try:
            if self.websocket is None:
                return
            async for message in self.websocket:
                await self._handle_message(message)
        except ConnectionClosedOK as exc:
            close_code, close_message = self._close_details(exc)
            normal = True
        except ConnectionClosedError as exc:
            close_code, close_message = self._close_details(exc)
            normal = self._is_idle_close(close_code, close_message)
            if self.callback and not normal:
                await self.callback.on_error(
                    f"ElevenLabs websocket closed: {close_message}",
                    str(close_code or "websocket_closed"),
                    self.generation,
                )
        except ConnectionClosed as exc:
            close_code, close_message = self._close_details(exc)
            normal = self._is_idle_close(close_code, close_message)
            if self.callback and not normal:
                await self.callback.on_error(
                    f"ElevenLabs websocket closed: {close_message}",
                    str(close_code or "websocket_closed"),
                    self.generation,
                )
        except Exception as exc:
            close_message = str(exc)
            if self.callback:
                await self.callback.on_error(
                    f"ElevenLabs websocket error: {exc}",
                    "websocket_error",
                    self.generation,
                )
        finally:
            self.is_started = False
            if self.callback and not self._closed:
                await self.callback.on_close(
                    close_code,
                    close_message,
                    self.generation,
                    normal,
                )

    async def start(self, timeout: float = 10) -> bool:
        """Open the socket and wait for the vendor handshake."""
        if self.is_connected():
            return True
        self._closed = False
        self._session_started.clear()
        self._start_error = None
        self._connection_started_at = monotonic()
        ws_url = self._build_websocket_url()
        self._log(
            "Connecting to ElevenLabs realtime ASR: " f"{redact_url(ws_url)}"
        )
        try:
            headers = {}
            if isinstance(self.api_key, str) and self.api_key:
                headers["xi-api-key"] = self.api_key
            self.websocket = await websockets.connect(
                ws_url,
                additional_headers=headers,
                open_timeout=timeout,
            )
            self._message_task = asyncio.create_task(self._message_handler())
            await asyncio.wait_for(
                self._session_started.wait(), timeout=timeout
            )
            if self._start_error:
                code, message = self._start_error
                raise ConnectionError(f"{code}: {message}")
            if not self.is_started:
                raise ConnectionError("ElevenLabs session did not start")
            return True
        except Exception:
            await self.close(notify=False)
            raise

    async def send_audio_frame(self, audio_data: bytes) -> bool:
        """Send one PCM frame and update the timeline after success."""
        if not audio_data or not self.is_connected():
            return False
        payload = {
            "message_type": "input_audio_chunk",
            "audio_base_64": base64.b64encode(audio_data).decode("ascii"),
            "sample_rate": self.sample_rate,
            "commit": False,
        }
        try:
            async with self._send_lock:
                if not self.is_connected() or self.websocket is None:
                    return False
                await self.websocket.send(json.dumps(payload))
            duration_ms = int(
                len(audio_data)
                / self.sample_rate
                / self.channels
                / self.sample_width
                * 1000
            )
            self.audio_timeline.add_user_audio(duration_ms)
            return True
        except ConnectionClosed:
            self.is_started = False
            return False

    async def send_final(self) -> bool:
        """Commit the current segment after all prior audio sends complete."""
        if not self.is_connected() or self.websocket is None:
            return False
        payload = {
            "message_type": "input_audio_chunk",
            "audio_base_64": "",
            "commit": True,
        }
        try:
            async with self._send_lock:
                if not self.is_connected() or self.websocket is None:
                    return False
                await self.websocket.send(json.dumps(payload))
            return True
        except ConnectionClosed:
            self.is_started = False
            return False

    async def close(self, notify: bool = False) -> None:
        """Close the socket and await the reader task without reconnecting."""
        self._closed = True
        self.is_started = False
        ws = self.websocket
        self.websocket = None
        if ws is not None:
            try:
                if getattr(ws, "state", None) == State.OPEN:
                    await ws.close()
            except Exception as exc:
                self._log(f"Error closing ElevenLabs websocket: {exc}")

        task = self._message_task
        self._message_task = None
        if task and task is not asyncio.current_task() and not task.done():
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

        if notify and self.callback:
            await self.callback.on_close(0, "closed", self.generation, True)

    def is_connected(self) -> bool:
        if self.websocket is None or not self.is_started:
            return False
        try:
            return getattr(self.websocket, "state", State.OPEN) == State.OPEN
        except Exception:
            return False
