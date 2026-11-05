#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""WebSocket client for Blaze realtime TTS.

Protocol (wss://api.blaze.vn/v1/tts/realtime):
  1. Receive {"type": "successful-connection"}.
  2. Send {"token", "strategy": "request"}; receive
     {"type": "successful-authentication"} or {"type": "failed-authentication"}
     followed by close code 1008.
  3. Per text: send a query JSON, receive "processing-request",
     "started-byte-stream", binary PCM chunks, then "finished-byte-stream".
     Failures arrive as "failed-request" / "bad-request" / "internal-error".
One socket serves many sequential queries. Blaze has no cancel message, so
an interrupted stream drops the socket and the next query reconnects.
"""

import asyncio
import json
from datetime import datetime
from typing import Any, AsyncIterator

import websockets
from websockets.asyncio.client import ClientConnection
from websockets.exceptions import ConnectionClosed, InvalidStatus

from .config import BlazeTTSConfig
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR

# Event types communicated back to the extension.
# 4 is reserved (used by other TTS extensions for flush events).
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_TTFB_METRIC = 5

# Seconds to wait for a handshake message or the next stream message.
WS_HANDSHAKE_TIMEOUT = 15.0
WS_RECV_TIMEOUT = 30.0

WS_CLOSE_POLICY_VIOLATION = 1008
STREAM_END_EVENTS = {"finished-byte-stream", "completed-request"}
STREAM_ERROR_EVENTS = {"failed-request", "bad-request", "internal-error"}


class BlazeTTSConnectionException(Exception):
    """Exception raised when the Blaze TTS connection or auth fails"""

    def __init__(self, status_code: int, body: str):
        self.status_code = status_code
        self.body = body
        super().__init__(
            f"Blaze TTS connection failed (code: {status_code}): {body}"
        )


def _event_type(data: dict[str, Any]) -> str:
    # Blaze uses "type" for most events and "status" for stream events.
    return str(data.get("type") or data.get("status") or "")


class BlazeTTSClient:
    """Blaze realtime TTS over one reusable WebSocket."""

    def __init__(
        self,
        config: BlazeTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        self.config = config
        self.ten_env = ten_env

        self._ws: ClientConnection | None = None
        self._is_cancelled = False

        # TTFB tracking
        self._sent_ts: datetime | None = None
        self._ttfb_sent: bool = False

    async def start(self) -> None:
        """Preheat: establish and authenticate the connection."""
        try:
            await self._connect()
        except BlazeTTSConnectionException:
            raise
        except Exception as e:  # pylint: disable=broad-exception-caught
            self.ten_env.log_error(f"Blaze TTS preheat failed: {e}")

    async def stop(self) -> None:
        self._is_cancelled = True
        await self._close()

    async def cancel(self) -> None:
        """Cancel current TTS.

        Blaze cannot stop a stream in place, so the socket is closed to
        drop unread audio; the next request reconnects.
        """
        self.ten_env.log_debug("Cancelling current TTS task.")
        self._is_cancelled = True
        self.reset_ttfb()
        await self._close()

    def reset_ttfb(self) -> None:
        self._sent_ts = None
        self._ttfb_sent = False

    def build_query(self, text: str) -> dict[str, Any]:
        query: dict[str, Any] = {
            "query": text,
            "language": self.config.language,
            "audio_format": "pcm",
            "audio_quality": self.config.audio_quality,
            # Blaze's realtime docs send speed as a string.
            "audio_speed": str(self.config.audio_speed),
            "speaker_id": self.config.speaker_id,
            "normalization": self.config.normalization,
            "model": self.config.model,
            "sample_rate": self.config.sample_rate,
        }
        # Forward additional Blaze params while keeping auth and the
        # endpoint out of the query.
        for key, value in self.config.params.items():
            if key in {"api_key", "base_url"} or value is None:
                continue
            query[key] = value
        return query

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | int | None, int]]:
        """Synthesize one text and yield audio events."""
        if not text.strip():
            yield None, EVENT_TTS_END
            return

        await self._ensure_connection()

        if not self._ttfb_sent and self._sent_ts is None:
            self._sent_ts = datetime.now()

        # Clear cancel flag just before sending, not at
        # method entry — avoids race with concurrent cancel()
        self._is_cancelled = False

        try:
            await self._send_query(text)
        except BlazeTTSConnectionException:
            raise
        except Exception as e:  # pylint: disable=broad-exception-caught
            await self._close()
            self.ten_env.log_error(
                f"vendor_error: Blaze TTS send failed: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            yield str(e).encode("utf-8"), EVENT_TTS_ERROR
            return

        carry = b""
        try:
            while True:
                if self._is_cancelled or self._ws is None:
                    self.ten_env.log_debug("Cancelled, stopping stream.")
                    return
                try:
                    message = await asyncio.wait_for(
                        self._ws.recv(), timeout=WS_RECV_TIMEOUT
                    )
                except asyncio.TimeoutError:
                    await self._close()
                    yield (
                        b"Timeout waiting for Blaze TTS audio",
                        EVENT_TTS_ERROR,
                    )
                    return

                if isinstance(message, bytes):
                    if self._is_cancelled:
                        self.ten_env.log_debug("Dropping audio (cancelled)")
                        return
                    # Keep PCM16 samples whole across chunk boundaries.
                    data = carry + message
                    cut = len(data) - (len(data) % 2)
                    carry = data[cut:]
                    if not cut:
                        # Empty or half-sample frames are not audio yet.
                        continue
                    # TTFB on the first non-empty, whole-sample PCM payload.
                    if self._sent_ts and not self._ttfb_sent:
                        ttfb_ms = int(
                            (datetime.now() - self._sent_ts).total_seconds()
                            * 1000
                        )
                        yield ttfb_ms, EVENT_TTS_TTFB_METRIC
                        self._ttfb_sent = True
                    yield data[:cut], EVENT_TTS_RESPONSE
                    continue

                data = json.loads(message)
                event = _event_type(data)
                if event in STREAM_END_EVENTS:
                    self.ten_env.log_debug("BlazeTTS: stream finished")
                    yield None, EVENT_TTS_END
                    return
                if event in STREAM_ERROR_EVENTS:
                    detail = str(
                        data.get("details") or data.get("message") or data
                    )
                    self.ten_env.log_error(
                        f"vendor_error: {event}: {detail}",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    # The socket does not recover after a failed request.
                    await self._close()
                    yield detail.encode("utf-8"), EVENT_TTS_ERROR
                    return
                # processing-request / started-byte-stream are progress only.

        except ConnectionClosed as e:
            self._ws = None
            if self._is_cancelled:
                return
            if e.rcvd and e.rcvd.code == WS_CLOSE_POLICY_VIOLATION:
                raise BlazeTTSConnectionException(
                    status_code=401, body=e.rcvd.reason or str(e)
                ) from e
            self.ten_env.log_error(
                f"vendor_error: Blaze TTS socket closed: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            yield str(e).encode("utf-8"), EVENT_TTS_ERROR

    async def _send_query(self, text: str) -> None:
        payload = json.dumps(self.build_query(text), ensure_ascii=False)
        try:
            await self._ws.send(payload)
        except ConnectionClosed:
            # Blaze may close an idle socket; reconnect once and resend.
            self.ten_env.log_warn(
                "Blaze TTS socket was closed, reconnecting",
                category=LOG_CATEGORY_VENDOR,
            )
            await self._connect()
            await self._ws.send(payload)

    async def _recv_json(self) -> dict[str, Any]:
        while True:
            raw = await asyncio.wait_for(
                self._ws.recv(), timeout=WS_HANDSHAKE_TIMEOUT
            )
            if isinstance(raw, str):
                return json.loads(raw)

    async def _connect(self) -> None:
        await self._close()
        url = self.config.ws_url
        try:
            self._ws = await websockets.connect(
                url,
                max_size=None,
                ping_interval=None,
                open_timeout=WS_HANDSHAKE_TIMEOUT,
            )
            hello = await self._recv_json()
            if _event_type(hello) != "successful-connection":
                raise RuntimeError(f"Unexpected Blaze TTS greeting: {hello}")
            await self._ws.send(
                json.dumps(
                    {"token": self.config.api_key, "strategy": "request"}
                )
            )
            auth = await self._recv_json()
            if _event_type(auth) != "successful-authentication":
                raise BlazeTTSConnectionException(
                    status_code=401,
                    body=str(auth.get("message") or auth),
                )
            self.ten_env.log_debug(
                "vendor_status: connected to blaze tts",
                category=LOG_CATEGORY_VENDOR,
            )
        except BlazeTTSConnectionException:
            await self._close()
            raise
        except InvalidStatus as e:
            await self._close()
            raise BlazeTTSConnectionException(
                status_code=e.response.status_code, body=str(e)
            ) from e
        except ConnectionClosed as e:
            await self._close()
            if e.rcvd and e.rcvd.code == WS_CLOSE_POLICY_VIOLATION:
                raise BlazeTTSConnectionException(
                    status_code=401, body=e.rcvd.reason or str(e)
                ) from e
            raise
        except Exception:
            await self._close()
            raise

    async def _ensure_connection(self) -> None:
        if not self._ws:
            await self._connect()

    async def _close(self) -> None:
        ws = self._ws
        self._ws = None
        if ws is not None:
            try:
                await ws.close()
            except Exception:  # pylint: disable=broad-exception-caught
                pass
