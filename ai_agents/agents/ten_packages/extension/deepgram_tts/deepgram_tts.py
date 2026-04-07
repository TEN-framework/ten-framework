#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
from collections.abc import Callable
from datetime import datetime
from typing import AsyncIterator

import websockets
from websockets.asyncio.client import ClientConnection

from .config import DeepgramTTSConfig
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR

# Event types for the output queue
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_FLUSH = 4
EVENT_TTS_TTFB_METRIC = 5

MAX_RETRY_TIMES = 5

# Sentinel to signal the send loop to stop
_SEND_STOP = None


class DeepgramTTSConnectionException(Exception):
    """Exception raised when Deepgram TTS connection fails"""

    def __init__(self, status_code: int, body: str):
        self.status_code = status_code
        self.body = body
        super().__init__(
            f"Deepgram TTS connection failed " f"(code: {status_code}): {body}"
        )


class DeepgramTTSClient:
    """Duplex WebSocket client for Deepgram TTS.

    Uses separate send and receive tasks on a single WebSocket
    connection. Text goes into _text_queue via send_text(),
    audio/events come out of _output_queue via get().
    """

    def __init__(
        self,
        config: DeepgramTTSConfig,
        ten_env: AsyncTenEnv,
        send_fatal_tts_error: Callable[[str], asyncio.Future] | None = None,
        send_non_fatal_tts_error: Callable[[str], asyncio.Future] | None = None,
    ):
        self.config = config
        self.ten_env = ten_env
        self.send_fatal_tts_error = send_fatal_tts_error
        self.send_non_fatal_tts_error = send_non_fatal_tts_error

        self._ws: ClientConnection | None = None
        self._closing = False
        self._is_cancelled = False

        # Duplex queues
        self._text_queue: asyncio.Queue[str | None] = asyncio.Queue()
        self._output_queue: asyncio.Queue[tuple[bytes | int | None, int]] = (
            asyncio.Queue()
        )

        # Background tasks
        self._connection_task: asyncio.Task | None = None
        self._channel_tasks: list[asyncio.Task] = []
        self._connect_failures = 0

        # TTFB tracking
        self._sent_ts: datetime | None = None
        self._ttfb_sent: bool = False

        self._ws_url = self._build_ws_url()

    def _build_ws_url(self) -> str:
        base = self.config.base_url
        params = (
            f"model={self.config.model}"
            f"&encoding={self.config.encoding}"
            f"&sample_rate={self.config.sample_rate}"
        )
        return f"{base}?{params}"

    # ── Lifecycle ────────────────────────────────────────────────

    async def start(self) -> None:
        """Start client: connect and launch send/receive loops."""
        self._closing = False
        self._connection_task = asyncio.create_task(self._connection_loop())
        # Wait briefly for connection to establish
        await asyncio.sleep(0.1)

    async def stop(self) -> None:
        """Stop client: close connection and cancel tasks."""
        self._closing = True
        self._is_cancelled = True

        # Signal send loop to exit
        await self._text_queue.put(_SEND_STOP)

        # Cancel channel tasks
        for task in self._channel_tasks:
            task.cancel()
        self._channel_tasks.clear()

        if self._connection_task:
            self._connection_task.cancel()
            try:
                await self._connection_task
            except asyncio.CancelledError:
                pass
            self._connection_task = None

        # Signal any consumer waiting on output_queue
        await self._output_queue.put((None, EVENT_TTS_END))

        if self._ws:
            try:
                await self._ws.send(json.dumps({"type": "Close"}))
            except Exception:
                pass
            try:
                await self._ws.close()
            except Exception:
                pass
            self._ws = None

    async def cancel(self) -> None:
        """Cancel current TTS request."""
        self.ten_env.log_debug("Cancelling current TTS task.")
        self._is_cancelled = True
        self.reset_ttfb()
        # Send Flush to Deepgram to stop audio generation
        if self._ws:
            try:
                await self._ws.send(json.dumps({"type": "Flush"}))
            except Exception:
                pass

    def reset_ttfb(self) -> None:
        self._sent_ts = None
        self._ttfb_sent = False

    # ── Public interface for extension ───────────────────────────

    async def send_text(self, text: str) -> None:
        """Queue text for sending to Deepgram."""
        await self._text_queue.put(text)

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | int | None, int]]:
        """Send text and yield audio events.

        For empty text, immediately yields EVENT_TTS_END.
        Otherwise sends text to the send loop and reads
        events from the output queue until END or ERROR.
        """
        if len(text.strip()) == 0:
            self.ten_env.log_warn("DeepgramTTS: empty text, returning END")
            yield None, EVENT_TTS_END
            return

        self._is_cancelled = False

        # Track TTFB from when we send
        if not self._ttfb_sent:
            self._sent_ts = datetime.now()

        # Put text into send queue
        await self._text_queue.put(text)

        # Read events from output queue
        while True:
            try:
                data_msg, event = await asyncio.wait_for(
                    self._output_queue.get(), timeout=5.0
                )
            except asyncio.TimeoutError:
                self.ten_env.log_error("Timeout waiting for Deepgram audio")
                yield (
                    b"Timeout waiting for Deepgram audio",
                    EVENT_TTS_ERROR,
                )
                break

            if event == EVENT_TTS_END:
                yield None, EVENT_TTS_END
                break
            elif event == EVENT_TTS_ERROR:
                yield data_msg, EVENT_TTS_ERROR
                break
            else:
                yield data_msg, event

    # ── Connection loop with auto-reconnect ─────────────────────

    async def _connection_loop(self) -> None:
        min_delay = 0.1
        max_delay = 3.0

        while not self._closing:
            try:
                await self._connect()
                self._connect_failures = 0

                if self._closing:
                    return

                # Launch duplex tasks
                self._channel_tasks = [
                    asyncio.create_task(self._send_loop()),
                    asyncio.create_task(self._receive_loop()),
                ]

                # Wait for either to finish
                done, pending = await asyncio.wait(
                    self._channel_tasks,
                    return_when=asyncio.FIRST_COMPLETED,
                )

                for task in pending:
                    task.cancel()
                self._channel_tasks.clear()

                for task in done:
                    exc = task.exception()
                    if exc and not isinstance(exc, asyncio.CancelledError):
                        self.ten_env.log_warn(
                            f"Channel task exception: {exc}",
                            category=LOG_CATEGORY_VENDOR,
                        )

            except DeepgramTTSConnectionException:
                raise

            except asyncio.CancelledError:
                return

            except Exception as e:
                self.ten_env.log_warn(
                    f"vendor_status: connection error: {e}",
                    category=LOG_CATEGORY_VENDOR,
                )

            finally:
                if self._ws:
                    try:
                        await self._ws.close()
                    except Exception:
                        pass
                    self._ws = None

            if self._closing:
                return

            self._connect_failures += 1
            if self._connect_failures > MAX_RETRY_TIMES:
                self.ten_env.log_error(
                    f"Max retries ({MAX_RETRY_TIMES}) " f"exceeded",
                    category=LOG_CATEGORY_VENDOR,
                )
                return

            delay = min(
                min_delay * (2 ** (self._connect_failures - 1)),
                max_delay,
            )
            self.ten_env.log_debug(
                f"vendor_status: reconnecting in "
                f"{delay:.1f}s "
                f"(attempt {self._connect_failures}"
                f"/{MAX_RETRY_TIMES})",
                category=LOG_CATEGORY_VENDOR,
            )
            await asyncio.sleep(delay)

    async def _connect(self) -> None:
        try:
            extra_headers = {
                "Authorization": f"Token {self.config.api_key}",
            }
            self._ws = await websockets.connect(
                self._ws_url,
                additional_headers=extra_headers,
            )
            self.ten_env.log_debug(
                "vendor_status: connected to deepgram tts",
                category=LOG_CATEGORY_VENDOR,
            )
        except Exception as e:
            error_message = str(e)
            if "401" in error_message or "Unauthorized" in error_message:
                if self.send_fatal_tts_error:
                    await self.send_fatal_tts_error(error_message=error_message)
                else:
                    raise DeepgramTTSConnectionException(
                        status_code=401, body=error_message
                    ) from e
            else:
                self.ten_env.log_error(f"Deepgram TTS connection failed: {e}")
                if self.send_non_fatal_tts_error:
                    await self.send_non_fatal_tts_error(
                        error_message=error_message
                    )
                raise

    # ── Send loop ───────────────────────────────────────────────

    async def _send_loop(self) -> None:
        """Read text from queue and send Speak+Flush to WS."""
        try:
            while not self._closing:
                text = await self._text_queue.get()
                if text is _SEND_STOP:
                    return

                if not self._ws:
                    self.ten_env.log_error("WS not connected in send loop")
                    return

                self.ten_env.log_debug(
                    f"send_text: {text[:80]}",
                    category=LOG_CATEGORY_VENDOR,
                )

                speak_msg = {"type": "Speak", "text": text}
                await self._ws.send(json.dumps(speak_msg))
                await self._ws.send(json.dumps({"type": "Flush"}))

        except asyncio.CancelledError:
            return
        except Exception as e:
            self.ten_env.log_error(
                f"vendor_error: send_loop error: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise

    # ── Receive loop ────────────────────────────────────────────

    async def _receive_loop(self) -> None:
        """Read from WS and dispatch to output queue."""
        if not self._ws:
            return

        try:
            async for message in self._ws:
                if self._closing:
                    return

                if isinstance(message, bytes):
                    await self._handle_audio(message)
                else:
                    await self._handle_text_message(message)

        except asyncio.CancelledError:
            return
        except websockets.exceptions.ConnectionClosed:
            self.ten_env.log_warn(
                "vendor_status: WS closed by server",
                category=LOG_CATEGORY_VENDOR,
            )
        except Exception as e:
            self.ten_env.log_error(
                f"vendor_error: receive_loop: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise

    async def _handle_audio(self, data: bytes) -> None:
        """Handle binary audio message from WS."""
        if self._is_cancelled:
            self.ten_env.log_debug("Dropping audio chunk (cancelled)")
            return

        # TTFB on first audio chunk
        if self._sent_ts and not self._ttfb_sent:
            ttfb_ms = int(
                (datetime.now() - self._sent_ts).total_seconds() * 1000
            )
            await self._output_queue.put((ttfb_ms, EVENT_TTS_TTFB_METRIC))
            self._ttfb_sent = True

        self.ten_env.log_debug(
            f"DeepgramTTS: audio chunk, " f"length: {len(data)}"
        )
        await self._output_queue.put((data, EVENT_TTS_RESPONSE))

    async def _handle_text_message(self, raw: str) -> None:
        """Handle JSON text message from WS."""
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            self.ten_env.log_warn(f"Failed to parse message: {raw}")
            return

        msg_type = data.get("type", "")

        if msg_type == "Flushed":
            self.ten_env.log_debug("DeepgramTTS: Flushed received")
            # Always signal END so get() returns promptly
            # (even after cancel — the extension checks
            # cancel state separately)
            await self._output_queue.put((None, EVENT_TTS_END))

        elif msg_type == "Warning":
            self.ten_env.log_warn(
                f"Deepgram warning: " f"{data.get('warn_msg', '')}"
            )

        elif msg_type == "Error":
            error_msg = data.get("err_msg", "Unknown error")
            self.ten_env.log_error(f"Deepgram error: {error_msg}")
            await self._output_queue.put(
                (
                    error_msg.encode("utf-8"),
                    EVENT_TTS_ERROR,
                )
            )

        else:
            self.ten_env.log_debug(f"Unknown message type: {msg_type}")
