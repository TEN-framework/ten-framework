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
"""

import asyncio
import json
from abc import ABC, abstractmethod
from typing import Any

import websockets
from websockets.protocol import State

from ten_ai_base.const import LOG_CATEGORY_TRANSCRIPTS, LOG_CATEGORY_VENDOR
from ten_ai_base.timeline import AudioTimeline
from ten_runtime import AsyncTenEnv

from .config import BlazeSTTConfig
from .const import READY_TIMEOUT_SECONDS, WS_CLOSE_POLICY_VIOLATION


class BlazeSTTAuthError(Exception):
    """Raised when Blaze rejects the token."""

    status_code = 401


class BlazeSTTRecognitionCallback(ABC):
    @abstractmethod
    async def on_open(self) -> None:
        pass

    @abstractmethod
    async def on_partial_result(self, message_data: dict[str, Any]) -> None:
        pass

    @abstractmethod
    async def on_final_result(self, message_data: dict[str, Any]) -> None:
        pass

    @abstractmethod
    async def on_error(
        self, error_msg: str, error_code: int | str | None = None
    ) -> None:
        pass

    @abstractmethod
    async def on_close(self) -> None:
        pass


class BlazeSTTRecognition:
    def __init__(
        self,
        config: BlazeSTTConfig,
        audio_timeline: AudioTimeline,
        ten_env: AsyncTenEnv,
        callback: BlazeSTTRecognitionCallback,
    ):
        self.config = config
        self.audio_timeline = audio_timeline
        self.ten_env = ten_env
        self.callback = callback
        self.websocket = None
        self.is_started = False
        self.final_event = asyncio.Event()
        self._message_task: asyncio.Task | None = None
        self._bytes_per_ms = config.sample_rate * 2 / 1000

    async def start(self, timeout: float = READY_TIMEOUT_SECONDS) -> None:
        url = self.config.ws_url()
        self.ten_env.log_info(
            f"vendor_status: connecting to Blaze STT {url}",
            category=LOG_CATEGORY_VENDOR,
        )
        self.websocket = await websockets.connect(
            url, open_timeout=timeout, max_size=None
        )
        try:
            await self.websocket.send(json.dumps(self.config.init_message()))
            while True:
                raw = await asyncio.wait_for(
                    self.websocket.recv(), timeout=timeout
                )
                if isinstance(raw, bytes):
                    continue
                event = json.loads(raw)
                event_type = event.get("type")
                self.ten_env.log_debug(
                    f"vendor_result: startup: {event_type}",
                    category=LOG_CATEGORY_VENDOR,
                )
                if event_type == "ready":
                    break
                if event_type == "failed-authentication":
                    raise BlazeSTTAuthError(
                        event.get("message")
                        or "Blaze STT authentication failed"
                    )
                if event_type == "error":
                    raise RuntimeError(
                        f"Blaze STT startup error: {event.get('text') or event}"
                    )
            self.is_started = True
            self.final_event.clear()
            await self.callback.on_open()
            self._message_task = asyncio.create_task(self._message_handler())
        except websockets.exceptions.ConnectionClosed as e:
            await self._close_socket()
            if e.rcvd and e.rcvd.code == WS_CLOSE_POLICY_VIOLATION:
                raise BlazeSTTAuthError(
                    e.rcvd.reason or "Blaze STT authentication failed"
                ) from e
            raise
        except Exception:
            await self._close_socket()
            raise

    async def _message_handler(self) -> None:
        try:
            async for message in self.websocket:
                if isinstance(message, bytes):
                    continue
                event = json.loads(message)
                event_type = event.get("type", "")
                if event_type == "partial":
                    await self.callback.on_partial_result(event)
                elif event_type == "final":
                    self.ten_env.log_debug(
                        f"vendor_result: final: {message}",
                        category=LOG_CATEGORY_TRANSCRIPTS,
                    )
                    self.final_event.set()
                    await self.callback.on_final_result(event)
                elif event_type == "error":
                    await self.callback.on_error(
                        str(event.get("text") or "Unknown Blaze STT error"),
                        event.get("code"),
                    )
                elif event_type == "failed-authentication":
                    await self.callback.on_error(
                        str(event.get("message") or "authentication failed"),
                        BlazeSTTAuthError.status_code,
                    )
        except websockets.exceptions.ConnectionClosed as e:
            self.ten_env.log_info(
                f"vendor_status: Blaze STT websocket closed: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
        except Exception as e:  # pylint: disable=broad-exception-caught
            await self.callback.on_error(
                f"WebSocket message handler error: {e}"
            )
        finally:
            self.is_started = False
            await self.callback.on_close()

    async def send_audio_frame(self, audio_data: bytes) -> None:
        if not self.is_connected():
            raise RuntimeError("WebSocket not connected")
        self.audio_timeline.add_user_audio(
            int(len(audio_data) / self._bytes_per_ms)
        )
        await self.websocket.send(audio_data)

    async def send_silence(self, duration_ms: int) -> None:
        """Send PCM silence so server-side endpointing ends the utterance."""
        if not self.is_connected():
            return
        self.audio_timeline.add_silence_audio(duration_ms)
        await self.websocket.send(
            b"\x00" * int(duration_ms * self._bytes_per_ms)
        )

    async def wait_for_final(self, timeout_ms: int) -> bool:
        try:
            await asyncio.wait_for(self.final_event.wait(), timeout_ms / 1000)
        except asyncio.TimeoutError:
            return False
        return True

    async def close(self) -> None:
        self.is_started = False
        await self._close_socket()
        if self._message_task and not self._message_task.done():
            self._message_task.cancel()
            try:
                await self._message_task
            except asyncio.CancelledError:
                pass
        self._message_task = None

    async def _close_socket(self) -> None:
        if self.websocket is None:
            return
        try:
            if self.websocket.state == State.OPEN:
                await self.websocket.close()
        except Exception as e:  # pylint: disable=broad-exception-caught
            self.ten_env.log_info(f"Error closing websocket: {e}")

    def is_connected(self) -> bool:
        return (
            self.is_started
            and self.websocket is not None
            and self.websocket.state == State.OPEN
        )
