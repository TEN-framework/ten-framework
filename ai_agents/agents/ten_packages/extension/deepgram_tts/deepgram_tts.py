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

# Custom event types to communicate status back to the extension
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_FLUSH = 4
EVENT_TTS_TTFB_METRIC = 5


class DeepgramTTSConnectionException(Exception):
    """Exception raised when Deepgram TTS connection fails"""

    def __init__(self, status_code: int, body: str):
        self.status_code = status_code
        self.body = body
        super().__init__(
            f"Deepgram TTS connection failed (code: {status_code}): {body}"
        )


class DeepgramTTSClient:
    def __init__(
        self,
        config: DeepgramTTSConfig,
        ten_env: AsyncTenEnv,
        send_fatal_tts_error: Callable[[str], asyncio.Future] | None = None,
        send_non_fatal_tts_error: Callable[[str], asyncio.Future] | None = None,
    ):
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.ws: ClientConnection | None = None
        self.send_fatal_tts_error = send_fatal_tts_error
        self.send_non_fatal_tts_error = send_non_fatal_tts_error

        self.sent_ts: datetime | None = None
        self.ttfb_sent: bool = False

        # Build WebSocket URL with query parameters
        self.ws_url = self._build_ws_url()

    def _build_ws_url(self) -> str:
        """Build the WebSocket URL with query parameters"""
        base = self.config.base_url
        params = f"model={self.config.model}&encoding={self.config.encoding}&sample_rate={self.config.sample_rate}"
        return f"{base}?{params}"

    async def start(self) -> None:
        """Preheating: establish websocket connection during initialization"""
        try:
            await self._connect()

        except Exception as e:
            self.ten_env.log_error(f"Deepgram TTS preheat failed: {e}")

    async def _connect(self) -> None:
        """Connect to the websocket"""
        try:
            extra_headers = {
                "Authorization": f"Token {self.config.api_key}",
            }
            self.ws = await websockets.connect(
                self.ws_url,
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
                self.ten_env.log_error(
                    f"Deepgram TTS preheat failed, unexpected error: {e}"
                )
                if self.send_non_fatal_tts_error:
                    await self.send_non_fatal_tts_error(
                        error_message=error_message
                    )
                raise

    async def stop(self):
        # Stop the websocket connection if it exists
        if self.ws:
            try:
                # Send close message
                await self.ws.send(json.dumps({"type": "Close"}))
            except Exception:
                pass
            await self.ws.close()
            self.ws = None

    async def cancel(self):
        """
        Cancel the current TTS task.
        """
        self.ten_env.log_debug("Cancelling current TTS task.")
        self._is_cancelled = True
        if self.ws:
            self.reset_ttfb()
            # Send flush to clear any pending audio
            try:
                await self.ws.send(json.dumps({"type": "Flush"}))
            except Exception:
                pass

    def reset_ttfb(self):
        self.sent_ts = None
        self.ttfb_sent = False

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | int | None, int | None]]:
        """Generate TTS audio for the given text"""

        self._is_cancelled = False
        try:
            await self._ensure_connection()
            async for audio_chunk, event_status in self._process_single_tts(
                text
            ):
                if event_status == EVENT_TTS_FLUSH:
                    # Reconnect after flush
                    await self.ws.close()
                    self.ws = None
                    await self._ensure_connection()

                yield audio_chunk, event_status

        except Exception as e:
            self.ten_env.log_error(
                f"vendor_error: {e}", category=LOG_CATEGORY_VENDOR
            )
            raise

    async def _ensure_connection(self) -> None:
        """Ensure websocket connection is established"""
        if not self.ws:
            await self._connect()

    async def _process_single_tts(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | int | None, int | None]]:
        """Process a single TTS request"""
        if not self.ws:
            self.ten_env.log_error("Deepgram websocket not connected")
            return

        self.ten_env.log_debug(f"process_single_tts, text: {text}")

        if not self.ttfb_sent:
            self.sent_ts = datetime.now()

        # Send the text to Deepgram
        speak_msg = {
            "type": "Speak",
            "text": text,
        }
        await self.ws.send(json.dumps(speak_msg))

        # Send flush to get audio immediately
        await self.ws.send(json.dumps({"type": "Flush"}))

        try:
            # Receive audio data
            while True:
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        "Cancellation flag detected, stopping TTS stream."
                    )
                    yield None, EVENT_TTS_FLUSH
                    break

                try:
                    message = await asyncio.wait_for(
                        self.ws.recv(), timeout=10.0
                    )
                except asyncio.TimeoutError:
                    self.ten_env.log_error(
                        "Timeout waiting for Deepgram audio - yielding error"
                    )
                    yield b"Timeout waiting for Deepgram audio", EVENT_TTS_ERROR
                    break

                # Binary message = audio data
                if isinstance(message, bytes):
                    # First audio chunk, calculate TTFB
                    if self.sent_ts and not self.ttfb_sent:
                        ttfb_ms = int(
                            (datetime.now() - self.sent_ts).total_seconds()
                            * 1000
                        )
                        yield ttfb_ms, EVENT_TTS_TTFB_METRIC
                        self.ttfb_sent = True

                    self.ten_env.log_debug(
                        f"DeepgramTTS: sending EVENT_TTS_RESPONSE, "
                        f"length: {len(message)}"
                    )
                    yield message, EVENT_TTS_RESPONSE

                # Text message = JSON metadata
                else:
                    try:
                        data = json.loads(message)
                        msg_type = data.get("type", "")

                        if msg_type == "Flushed":
                            # All audio for this text has been sent
                            self.ten_env.log_debug(
                                "DeepgramTTS: received Flushed, "
                                "sending EVENT_TTS_END"
                            )
                            yield None, EVENT_TTS_END
                            break

                        elif msg_type == "Warning":
                            self.ten_env.log_warn(
                                f"Deepgram warning: {data.get('warn_msg', '')}"
                            )

                        elif msg_type == "Error":
                            error_msg = data.get("err_msg", "Unknown error")
                            self.ten_env.log_error(
                                f"Deepgram error: {error_msg}"
                            )
                            yield error_msg.encode("utf-8"), EVENT_TTS_ERROR
                            break

                    except json.JSONDecodeError:
                        self.ten_env.log_warn(
                            f"Failed to parse Deepgram message: {message}"
                        )

            if not self._is_cancelled:
                self.ten_env.log_debug("DeepgramTTS: TTS complete")

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message}",
                category=LOG_CATEGORY_VENDOR,
            )
            yield error_message.encode("utf-8"), EVENT_TTS_ERROR
