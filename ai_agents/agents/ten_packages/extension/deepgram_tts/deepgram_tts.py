#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
from datetime import datetime
from typing import AsyncIterator
from urllib.parse import urlencode

import websockets
from websockets.asyncio.client import ClientConnection
from websockets.exceptions import InvalidStatus

from .config import DeepgramTTSConfig
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR

# Event types communicated back to the extension.
# 4 is reserved (used by other TTS extensions for flush events).
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_TTFB_METRIC = 5

# Seconds to wait for a WebSocket response before timeout
WS_RECV_TIMEOUT = 8.0


class DeepgramTTSConnectionException(Exception):
    """Exception raised when Deepgram TTS connection fails"""

    def __init__(self, status_code: int, body: str):
        self.status_code = status_code
        self.body = body
        super().__init__(
            f"Deepgram TTS connection failed " f"(code: {status_code}): {body}"
        )


class DeepgramTTSClient:
    """WebSocket client for Deepgram TTS.

    Each get() call sends Speak+Flush and streams audio
    until Flushed. Connection is reused across calls but
    reconnected when needed (cancel, error, new request).
    """

    def __init__(
        self,
        config: DeepgramTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        self.config = config
        self.ten_env = ten_env

        self._ws: ClientConnection | None = None
        self._is_cancelled = False
        self._needs_reconnect = False

        # TTFB tracking
        self._sent_ts: datetime | None = None
        self._ttfb_sent: bool = False

        self._ws_url = self._build_ws_url()

    def _build_ws_url(self) -> str:
        base = self.config.base_url
        query_params: dict[str, str | int | float | bool] = {
            "model": self.config.model,
            "encoding": self.config.encoding,
            "sample_rate": self.config.sample_rate,
        }

        # Forward any additional Deepgram vendor params through the websocket
        # query string while keeping auth and endpoint configuration out of it.
        for key, value in self.config.params.items():
            if key in {"api_key", "base_url"} or value is None:
                continue
            query_params[key] = value

        return f"{base}?{urlencode(query_params, doseq=True)}"

    async def start(self) -> None:
        """Preheat: establish initial connection."""
        try:
            await self._connect()
        except Exception as e:
            self.ten_env.log_error(f"Deepgram TTS preheat failed: {e}")

    async def stop(self) -> None:
        self._is_cancelled = True
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
        """Cancel current TTS.

        Sends Flush and drains until Flushed so the
        connection is clean for the next request.
        """
        self.ten_env.log_debug("Cancelling current TTS task.")
        self._is_cancelled = True
        self.reset_ttfb()
        if self._ws:
            try:
                await self._ws.send(json.dumps({"type": "Flush"}))
                # Drain until Flushed to leave connection clean
                await asyncio.wait_for(self._drain_until_flushed(), timeout=3.0)
            except Exception as e:
                self.ten_env.log_warn(
                    f"Cancel drain failed: {e}, "
                    "will reconnect on next request"
                )
                self._needs_reconnect = True

    async def _drain_until_flushed(self) -> None:
        """Read and discard WS messages until Flushed."""
        while self._ws:
            msg = await self._ws.recv()
            if isinstance(msg, str):
                try:
                    data = json.loads(msg)
                    if data.get("type") == "Flushed":
                        return
                except json.JSONDecodeError:
                    pass

    def reset_ttfb(self) -> None:
        self._sent_ts = None
        self._ttfb_sent = False

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | int | None, int]]:
        """Send text and yield audio events."""
        if len(text.strip()) == 0:
            self.ten_env.log_warn("DeepgramTTS: empty text, returning END")
            yield None, EVENT_TTS_END
            return

        # Reconnect if needed (after error or cancel)
        if self._needs_reconnect:
            await self._reconnect()
            self._needs_reconnect = False

        await self._ensure_connection()

        if not self._ttfb_sent:
            self._sent_ts = datetime.now()

        # Clear cancel flag just before sending, not at
        # method entry — avoids race with concurrent cancel()
        self._is_cancelled = False

        # Send Speak + Flush
        speak_msg = {"type": "Speak", "text": text}
        await self._ws.send(json.dumps(speak_msg))
        await self._ws.send(json.dumps({"type": "Flush"}))

        # Receive audio until Flushed
        try:
            while True:
                if self._is_cancelled:
                    self.ten_env.log_debug("Cancelled, stopping stream.")
                    break

                try:
                    message = await asyncio.wait_for(
                        self._ws.recv(), timeout=WS_RECV_TIMEOUT
                    )
                except asyncio.TimeoutError:
                    self.ten_env.log_error("Timeout waiting for Deepgram audio")
                    self._needs_reconnect = True
                    yield (
                        b"Timeout waiting for Deepgram audio",
                        EVENT_TTS_ERROR,
                    )
                    break

                if isinstance(message, bytes):
                    if self._is_cancelled:
                        self.ten_env.log_debug("Dropping audio (cancelled)")
                        break

                    # TTFB on first audio chunk
                    if self._sent_ts and not self._ttfb_sent:
                        ttfb_ms = int(
                            (datetime.now() - self._sent_ts).total_seconds()
                            * 1000
                        )
                        yield ttfb_ms, EVENT_TTS_TTFB_METRIC
                        self._ttfb_sent = True

                    self.ten_env.log_debug(
                        f"DeepgramTTS: audio chunk, " f"length: {len(message)}"
                    )
                    yield message, EVENT_TTS_RESPONSE
                else:
                    try:
                        data = json.loads(message)
                        msg_type = data.get("type", "")

                        if msg_type == "Flushed":
                            self.ten_env.log_debug("DeepgramTTS: Flushed")
                            yield None, EVENT_TTS_END
                            break

                        elif msg_type == "Warning":
                            self.ten_env.log_warn(
                                f"Deepgram warning: "
                                f"{data.get('warn_msg', '')}"
                            )

                        elif msg_type == "Error":
                            error_msg = data.get("err_msg", "Unknown error")
                            self.ten_env.log_error(
                                f"Deepgram error: {error_msg}"
                            )
                            self._needs_reconnect = True
                            yield (
                                error_msg.encode("utf-8"),
                                EVENT_TTS_ERROR,
                            )
                            break

                    except json.JSONDecodeError:
                        self.ten_env.log_warn(f"Failed to parse: {message}")

            if not self._is_cancelled:
                self.ten_env.log_debug("DeepgramTTS: complete")

        except Exception as e:
            self.ten_env.log_error(
                f"vendor_error: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            self._needs_reconnect = True
            yield (
                str(e).encode("utf-8"),
                EVENT_TTS_ERROR,
            )

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
        except InvalidStatus as e:
            raise DeepgramTTSConnectionException(
                status_code=e.response.status_code,
                body=str(e),
            ) from e
        except Exception as e:
            error_message = str(e)
            # Fallback string match for non-websockets
            # exceptions (e.g., mocked tests)
            if "401" in error_message or "Unauthorized" in error_message:
                raise DeepgramTTSConnectionException(
                    status_code=401, body=error_message
                ) from e
            self.ten_env.log_error(f"Deepgram TTS connection failed: {e}")
            raise

    async def _ensure_connection(self) -> None:
        if not self._ws:
            await self._connect()

    async def _reconnect(self) -> None:
        """Close and re-establish the connection."""
        if self._ws:
            try:
                await self._ws.close()
            except Exception:
                pass
            self._ws = None
        await self._connect()
