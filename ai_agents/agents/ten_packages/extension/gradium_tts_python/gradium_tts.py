#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#
import asyncio
import base64
import json
from typing import Any, AsyncIterator, Tuple

import websockets
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_runtime import AsyncTenEnv

from .config import GradiumTTSConfig

# Event codes used by the extension
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_FLUSH = 4


class GradiumTTSClient:
    """Lightweight Gradium TTS websocket client."""

    def __init__(
        self,
        config: GradiumTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        self.config = config
        self.ten_env = ten_env
        self.ws: Any | None = None
        self._is_cancelled = False

    async def start(self) -> None:
        """Preheat connection (optional)."""
        try:
            await self._connect()
        except Exception as e:
            # Preheat failures should be logged but not fatal
            self.ten_env.log_warn(
                f"Failed to preheat Gradium websocket: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
        finally:
            await self._disconnect()

    async def _connect(self) -> None:
        if self.ws and not self.ws.closed:
            return

        headers = {"x-api-key": self.config.api_key}
        url = self.config.websocket_url()
        self.ten_env.log_debug(
            f"Connecting to Gradium TTS websocket at {url}",
            category=LOG_CATEGORY_VENDOR,
        )
        self.ws = await websockets.connect(
            url,
            extra_headers=headers,
            max_size=None,
        )

    async def _disconnect(self) -> None:
        if self.ws:
            await self.ws.close()
            self.ws = None

    async def cancel(self) -> None:
        """Cancel current request by closing websocket."""
        self._is_cancelled = True
        await self._disconnect()

    async def clean(self) -> None:
        """Clean up resources."""
        await self._disconnect()

    async def get(
        self, text: str, request_id: str, _text_input_end: bool
    ) -> AsyncIterator[Tuple[bytes | None, int]]:
        """Send a TTS request and yield audio chunks."""
        if len(text.strip()) == 0:
            yield None, EVENT_TTS_END
            return

        self._is_cancelled = False
        await self._connect()

        try:
            await self._send_setup()
            await self._wait_for_ready()

            await self._send_text(text)
            # Always close the stream per request to keep lifecycle simple.
            await self._send_end_of_stream()

            async for chunk, status in self._iter_messages():
                yield chunk, status
                if status in (
                    EVENT_TTS_END,
                    EVENT_TTS_ERROR,
                    EVENT_TTS_FLUSH,
                ):
                    break
        except Exception as e:
            self.ten_env.log_error(
                f"vendor_error: Gradium get failed for request_id {request_id}, error: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            yield str(e).encode("utf-8"), EVENT_TTS_ERROR
        finally:
            await self._disconnect()

    async def _send_setup(self) -> None:
        assert self.ws is not None
        payload = {
            "type": "setup",
            "model_name": self.config.model_name,
            "voice_id": self.config.voice_id,
            "output_format": self.config.output_format,
        }
        await self._send_json(payload)

    async def _wait_for_ready(self) -> None:
        """Wait for the ready message before sending text."""
        assert self.ws is not None
        while True:
            raw_msg = await asyncio.wait_for(self.ws.recv(), timeout=10)
            message = self._parse_message(raw_msg)
            if message is None:
                continue

            msg_type = message.get("type")
            if msg_type == "ready":
                return
            if msg_type == "error":
                err_msg = message.get("message", "Unknown error")
                code = message.get("code")
                if code:
                    err_msg = f"{err_msg} (code: {code})"
                raise RuntimeError(err_msg)
            # Ignore other messages until ready is received

    async def _send_text(self, text: str) -> None:
        assert self.ws is not None
        await self._send_json({"type": "text", "text": text})

    async def _send_end_of_stream(self) -> None:
        assert self.ws is not None
        await self._send_json({"type": "end_of_stream"})

    async def _send_json(self, payload: dict) -> None:
        assert self.ws is not None
        await self.ws.send(json.dumps(payload))

    async def _iter_messages(self) -> AsyncIterator[Tuple[bytes | None, int]]:
        assert self.ws is not None
        async for raw_msg in self.ws:
            if self._is_cancelled:
                yield None, EVENT_TTS_FLUSH
                break

            message = self._parse_message(raw_msg)
            if message is None:
                continue

            msg_type = message.get("type")
            if msg_type == "audio":
                audio_b64 = message.get("audio")
                if not audio_b64:
                    continue
                try:
                    audio_bytes = base64.b64decode(audio_b64)
                except Exception as e:
                    self.ten_env.log_warn(
                        f"Failed to decode Gradium audio chunk: {e}",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    continue
                yield audio_bytes, EVENT_TTS_RESPONSE
            elif msg_type == "end_of_stream":
                yield None, EVENT_TTS_END
                break
            elif msg_type == "error":
                err_msg = message.get("message", "Unknown error")
                code = message.get("code")
                if code:
                    err_msg = f"{err_msg} (code: {code})"
                yield err_msg.encode("utf-8"), EVENT_TTS_ERROR
                break
            elif msg_type == "ready":
                # Ready could appear again; ignore.
                continue

    def _parse_message(self, raw_msg: str | bytes) -> dict | None:
        try:
            if isinstance(raw_msg, bytes):
                raw_msg = raw_msg.decode("utf-8")
            return json.loads(raw_msg)
        except Exception as e:
            self.ten_env.log_warn(
                f"Failed to parse Gradium websocket message: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            return None

    def get_extra_metadata(self) -> dict[str, str]:
        """Return metadata for metrics."""
        return {
            "model_name": self.config.model_name,
            "voice_id": self.config.voice_id,
            "region": self.config.region,
            "output_format": self.config.output_format,
        }
