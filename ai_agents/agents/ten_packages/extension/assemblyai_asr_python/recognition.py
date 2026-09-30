import asyncio
import json
from typing import Any, Awaitable, Callable, Dict, Optional
from urllib.parse import urlencode

import websockets
from websockets.exceptions import (
    ConnectionClosed,
    InvalidStatus,
    WebSocketException,
)
from websockets.protocol import State

from .audio_buffer_manager import AudioBufferManager
from ten_ai_base.timeline import AudioTimeline
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.utils import redact_json
from ten_runtime import AsyncTenEnv

# Close code reported when the socket died without a close frame.
ABNORMAL_CLOSURE_CODE = 1006


class AssemblyAIConnectionError(Exception):
    """The server rejected the WebSocket handshake (auth, quota, bad request).

    ``code`` carries the HTTP status as a string so it can be surfaced as the
    vendor error code.
    """

    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code
        self.message = message


class AssemblyAIWSRecognitionCallback:
    """AssemblyAI WebSocket Speech Recognition Callback Interface"""

    async def on_open(self, session_id: str, configuration: Dict[str, Any]):
        """Called when the server confirms the session (``Begin`` message)."""

    async def on_result(self, message_data: Dict[str, Any]):
        """Called for every ``Turn`` message."""

    async def on_event(self, message_data: Dict[str, Any]):
        """Called for informational messages (SpeechStarted, Heartbeat, ...)."""

    async def on_error(self, error_msg: str, error_code: Optional[str] = None):
        """Called for errors that do not close the connection."""

    async def on_close(self, code: int, reason: str):
        """Called once when the server-side connection ends.

        ``code`` is the WebSocket close code (AssemblyAI reports errors via
        close codes, e.g. 1008 auth, 3005-3009 session errors).
        """


class AssemblyAIWSRecognition:
    """Async WebSocket client for the AssemblyAI Streaming v3 API."""

    def __init__(
        self,
        api_key: str,
        ws_url: str = "wss://streaming.assemblyai.com/v3/ws",
        audio_timeline: Optional[AudioTimeline] = None,
        ten_env: Optional[AsyncTenEnv] = None,
        config: Optional[Dict[str, Any]] = None,
        callback: Optional[AssemblyAIWSRecognitionCallback] = None,
    ):
        """
        :param api_key: AssemblyAI API key (sent as the Authorization header)
        :param ws_url: WebSocket endpoint
        :param audio_timeline: Audio timeline for timestamp bookkeeping
        :param ten_env: TEN environment used for logging
        :param config: Connection query parameters (already model-gated)
        :param callback: Receiver for session events
        """
        self.api_key = api_key
        self.ws_url = ws_url
        self.audio_timeline = audio_timeline
        self.ten_env = ten_env
        self.config = config or {}
        self.callback = callback
        self.websocket = None
        self.session_id: Optional[str] = None
        self.is_started = False
        self._closed_by_client = False
        self._message_task: Optional[asyncio.Task] = None
        self._consumer_task: Optional[asyncio.Task] = None

        self.audio_buffer = AudioBufferManager(
            ten_env=self.ten_env, threshold=1600
        )

    # ------------------------------------------------------------------
    # Inbound messages
    # ------------------------------------------------------------------

    async def _handle_message(self, message: str):
        """Dispatch one server message to the callback."""
        try:
            message_data = json.loads(message)
            self.ten_env.log_debug(
                f"vendor_result: on_recognized: {message}",
                category=LOG_CATEGORY_VENDOR,
            )

            message_type = message_data.get("type", "")

            if message_type == "Begin":
                self.session_id = message_data.get("id")
                configuration = message_data.get("configuration") or {}
                self.ten_env.log_info(
                    f"[AssemblyAI] Session started: {self.session_id}, "
                    f"expires at: {message_data.get('expires_at')}, "
                    f"configuration: {configuration}"
                )
                self.is_started = True
                if self.callback:
                    await self.callback.on_open(
                        self.session_id or "", configuration
                    )

            elif message_type == "Turn":
                if self.callback:
                    await self.callback.on_result(message_data)

            else:
                # Termination, SpeechStarted, Heartbeat, SpeakerRevision and
                # any future message types are informational vendor events,
                # not errors. Errors arrive as WebSocket close codes.
                if message_type == "Termination":
                    self.ten_env.log_info(
                        f"[AssemblyAI] Session terminated: {message_data}"
                    )
                if self.callback:
                    await self.callback.on_event(message_data)

        except json.JSONDecodeError as e:
            error_msg = f"Failed to parse message JSON: {e}"
            self.ten_env.log_error(f"[AssemblyAI] {error_msg}")
            if self.callback:
                await self.callback.on_error(error_msg)
        except Exception as e:
            error_msg = f"Error processing message: {e}"
            self.ten_env.log_error(f"[AssemblyAI] {error_msg}")
            if self.callback:
                await self.callback.on_error(error_msg)

    @staticmethod
    def _close_details(exc: ConnectionClosed) -> tuple[int, str]:
        """Extract (code, reason) from a websockets ConnectionClosed."""
        frame = exc.rcvd or exc.sent
        if frame is None:
            return ABNORMAL_CLOSURE_CODE, "connection closed abnormally"
        return frame.code, frame.reason or ""

    async def _message_handler(self):
        """Read server messages until the connection ends."""
        if self.websocket is None:
            self.ten_env.log_info(
                "[AssemblyAI] WebSocket connection not established, "
                "skipping message handler"
            )
            return

        close_code = 1000
        close_reason = "closed"
        try:
            async for message in self.websocket:
                try:
                    await self._handle_message(message)
                except Exception as e:
                    self.ten_env.log_error(
                        f"[AssemblyAI] Error handling message: {e}"
                    )
        except ConnectionClosed as e:
            close_code, close_reason = self._close_details(e)
            self.ten_env.log_info(
                f"[AssemblyAI] WebSocket connection closed "
                f"(code={close_code}, reason='{close_reason}')"
            )
        except WebSocketException as e:
            close_code, close_reason = ABNORMAL_CLOSURE_CODE, str(e)
            self.ten_env.log_error(f"[AssemblyAI] WebSocket error: {e}")
        except Exception as e:
            # asyncio.CancelledError derives from BaseException and is not
            # caught here: task cancellation during shutdown propagates after
            # the finally block.
            close_code, close_reason = ABNORMAL_CLOSURE_CODE, str(e)
            self.ten_env.log_error(f"[AssemblyAI] Receive loop error: {e}")
        finally:
            self.is_started = False
            self._cancel_consumer()
            if self.callback and not self._closed_by_client:
                await self.callback.on_close(close_code, close_reason)

    def _cancel_consumer(self) -> None:
        """Stop the audio consumer without awaiting it (safe from any task)."""
        task = self._consumer_task
        if task and not task.done() and task is not asyncio.current_task():
            task.cancel()

    # ------------------------------------------------------------------
    # Connection
    # ------------------------------------------------------------------

    def _build_websocket_url(self) -> str:
        """Build the connection URL.

        Mirrors the official SDK: ``None`` values are omitted, booleans are
        lower-cased, lists and dicts are JSON-encoded, and everything is
        URL-encoded. The API key never appears in the URL.
        """
        query: Dict[str, str] = {}
        for key, value in self.config.items():
            if value is None:
                continue
            if isinstance(value, bool):
                query[key] = "true" if value else "false"
            elif isinstance(value, (list, dict)):
                query[key] = json.dumps(value)
            else:
                query[key] = str(value)

        self.ten_env.log_info(
            "[AssemblyAI] Building websocket url with params: "
            f"{redact_json(query)}"
        )
        if not query:
            return self.ws_url
        return f"{self.ws_url}?{urlencode(query)}"

    async def start(
        self,
        timeout: int = 10,
        connect: Optional[Callable[..., Awaitable[Any]]] = None,
    ) -> bool:
        """
        Open the WebSocket and start the receive / send loops.

        :param timeout: Handshake timeout in seconds
        :param connect: Connect coroutine (defaults to ``websockets.connect``)
        :raises AssemblyAIConnectionError: the server rejected the handshake
        """
        if self.is_connected():
            self.ten_env.log_info("[AssemblyAI] Recognition already started")
            return True

        ws_url = self._build_websocket_url()
        headers = {"Authorization": self.api_key}
        connect = connect or websockets.connect

        self.ten_env.log_info(
            f"[AssemblyAI] Connecting to AssemblyAI: {self.ws_url}"
        )

        try:
            self.websocket = await connect(
                ws_url, additional_headers=headers, open_timeout=timeout
            )
        except InvalidStatus as e:
            status = getattr(getattr(e, "response", None), "status_code", None)
            code = str(status) if status is not None else "unknown"
            raise AssemblyAIConnectionError(
                code=code,
                message=f"WebSocket handshake rejected (HTTP {code}): {e}",
            ) from e

        self._closed_by_client = False
        self._message_task = asyncio.create_task(self._message_handler())
        self._consumer_task = asyncio.create_task(self._consume_and_send())

        self.ten_env.log_info("[AssemblyAI] WebSocket connection established")
        return True

    # ------------------------------------------------------------------
    # Outbound audio
    # ------------------------------------------------------------------

    async def send_audio_frame(self, audio_data: bytes):
        """Producer side: push audio bytes into the buffer."""
        try:
            await self.audio_buffer.push_audio(audio_data)
        except Exception as e:
            self.ten_env.log_error(
                f"[AssemblyAI] Failed to enqueue audio frame: {e}"
            )
            if self.callback:
                await self.callback.on_error(
                    f"Failed to enqueue audio frame: {e}"
                )

    async def _consume_and_send(self):
        """Consumer loop: pull fixed-size chunks and send them as binary."""
        sample_rate = self.config.get("sample_rate", 16000)
        try:
            while True:
                if not self.is_connected():
                    await asyncio.sleep(0.01)
                    continue

                chunk = await self.audio_buffer.pull_chunk()
                if chunk == b"" or self.websocket is None:
                    break
                if getattr(self.websocket, "state", State.OPEN) in (
                    State.CLOSING,
                    State.CLOSED,
                ):
                    break

                duration_ms = int(len(chunk) / (sample_rate / 1000 * 2))
                if self.audio_timeline:
                    self.audio_timeline.add_user_audio(duration_ms)

                await self.websocket.send(chunk)

        except ConnectionClosed:
            self.ten_env.log_error(
                "[AssemblyAI] WebSocket connection closed while sending audio"
            )
        except Exception as e:
            self.ten_env.log_error(f"[AssemblyAI] Consumer loop error: {e}")
            if self.callback:
                await self.callback.on_error(f"Consumer loop error: {e}")

    # ------------------------------------------------------------------
    # Outbound control messages
    # ------------------------------------------------------------------

    async def _send_control(self, message: Dict[str, Any], what: str) -> bool:
        if not self.is_connected():
            self.ten_env.log_info(
                f"[AssemblyAI] Recognition not started, cannot send {what}"
            )
            return False
        try:
            await self.websocket.send(json.dumps(message))
            # Payloads can carry conversational text (agent_context) or
            # prompts; log only the message type and field names.
            self.ten_env.log_info(
                f"[AssemblyAI] Sent {what}: type={message.get('type')} "
                f"fields={sorted(k for k in message if k != 'type')}"
            )
            return True
        except ConnectionClosed:
            self.ten_env.log_error(
                f"[AssemblyAI] WebSocket connection closed while sending {what}"
            )
        except Exception as e:
            error_msg = f"Failed to send {what}: {e}"
            self.ten_env.log_error(f"[AssemblyAI] {error_msg}")
            if self.callback:
                await self.callback.on_error(error_msg)
        return False

    async def send_update_configuration(
        self, config_update: Dict[str, Any]
    ) -> bool:
        """Change session settings mid-stream (prompt, agent_context, ...)."""
        return await self._send_control(
            {"type": "UpdateConfiguration", **config_update},
            "configuration update",
        )

    async def force_endpoint(self) -> bool:
        """Force the current turn to end immediately."""
        return await self._send_control(
            {"type": "ForceEndpoint"}, "force endpoint"
        )

    # ------------------------------------------------------------------
    # Shutdown
    # ------------------------------------------------------------------

    async def stop_consumer(self):
        """Stop the audio consumer task."""
        if self._consumer_task and not self._consumer_task.done():
            self._consumer_task.cancel()
            try:
                await self._consumer_task
            except asyncio.CancelledError:
                pass

    async def close(self):
        """Terminate the session and close the WebSocket."""
        self.ten_env.log_info("[AssemblyAI] Starting close process")
        self._closed_by_client = True

        if self.websocket is not None:
            try:
                if self.websocket.state == State.OPEN:
                    await self.websocket.send(json.dumps({"type": "Terminate"}))
                    await self.websocket.close()
            except Exception as e:
                self.ten_env.log_info(
                    f"[AssemblyAI] Error closing websocket: {e}"
                )

        await self.stop_consumer()

        message_task = self._message_task
        if (
            message_task
            and not message_task.done()
            and message_task is not asyncio.current_task()
        ):
            message_task.cancel()
            try:
                await message_task
            except asyncio.CancelledError:
                pass

        self.is_started = False

    def is_connected(self) -> bool:
        """True once the server sent ``Begin`` and the socket is still open."""
        if self.websocket is None:
            return False
        try:
            state = getattr(self.websocket, "state", None)
            if state is None:
                return self.is_started
            return self.is_started and state == State.OPEN
        except Exception:
            return False
