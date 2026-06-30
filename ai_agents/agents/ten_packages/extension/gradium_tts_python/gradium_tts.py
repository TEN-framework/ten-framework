#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#
from __future__ import annotations

import asyncio
import base64
import json
from datetime import datetime
from typing import Any, AsyncIterator

import websockets
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_runtime import AsyncTenEnv
from websockets.asyncio.client import ClientConnection
from websockets.exceptions import InvalidStatus
from websockets.protocol import State

from .config import GradiumTTSConfig

EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_TTFB_METRIC = 5

WS_READY_TIMEOUT = 10.0
WS_RECV_TIMEOUT = 10.0


class GradiumTTSConnectionException(Exception):
    """Raised when the Gradium websocket fails to open or initialize."""

    def __init__(self, status_code: int, body: str):
        self.status_code = status_code
        self.body = body
        super().__init__(
            f"Gradium TTS connection failed (code: {status_code}): {body}"
        )


class GradiumTTSClient:
    """Gradium TTS websocket client: one streaming session per request.

    Each text segment is forwarded to the vendor as it arrives (no local
    batching), matching Gradium's LLM-to-TTS streaming guidance and the
    behaviour of the other websocket TTS extensions. The caller opens a
    session, sends text segments, signals end_of_stream, and reads audio
    concurrently via audio_events().
    """

    def __init__(
        self,
        config: GradiumTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        self.config = config
        self.ten_env = ten_env
        self.ws: ClientConnection | None = None
        self._is_cancelled = False
        self._sent_ts: datetime | None = None
        self._ttfb_sent = False
        self._last_ready: dict[str, Any] = {}

    async def start(self) -> None:
        try:
            await self._connect()
        except Exception as exc:
            self.ten_env.log_warn(
                f"Gradium TTS preheat failed: {exc}",
                category=LOG_CATEGORY_VENDOR,
            )
        finally:
            await self._disconnect()

    async def clean(self) -> None:
        await self._disconnect()

    async def cancel(self) -> None:
        self._is_cancelled = True
        await self._disconnect()

    async def start_session(self) -> None:
        """Open a fresh websocket and complete setup for a new request."""
        self._is_cancelled = False
        self._last_ready = {}
        self._sent_ts = None
        self._ttfb_sent = False
        await self._disconnect()
        await self._connect()
        await self._send_setup()
        await self._wait_for_ready()

    async def send_text(self, text: str) -> None:
        """Forward a single text segment to the vendor immediately."""
        if self._sent_ts is None:
            self._sent_ts = datetime.now()
        await self._send_json({"type": "text", "text": text})

    async def end_input(self) -> None:
        """Signal that no more text is coming for the current request."""
        await self._send_json({"type": "end_of_stream"})

    def audio_events(
        self,
    ) -> AsyncIterator[tuple[bytes | int | None, int]]:
        """Yield audio/ttfb/end/error events until the session ends."""
        return self._iter_messages()

    def is_connected(self) -> bool:
        return self.ws is not None and self.ws.state == State.OPEN

    async def _connect(self) -> None:
        if self.is_connected():
            return

        url = self.config.websocket_url()
        headers = {"x-api-key": self.config.api_key}
        try:
            self.ws = await websockets.connect(
                url,
                additional_headers=headers,
                max_size=None,
            )
            self.ten_env.log_debug(
                "vendor_status: connected to gradium tts",
                category=LOG_CATEGORY_VENDOR,
            )
        except InvalidStatus as exc:
            raise GradiumTTSConnectionException(
                status_code=exc.response.status_code,
                body=str(exc),
            ) from exc
        except Exception as exc:
            message = str(exc)
            if "401" in message or "403" in message:
                raise GradiumTTSConnectionException(
                    status_code=401,
                    body=message,
                ) from exc
            raise

    async def _disconnect(self) -> None:
        if self.ws:
            try:
                await self.ws.close()
            except Exception:
                pass
            self.ws = None

    async def _send_setup(self) -> None:
        payload = {
            "type": "setup",
            "model_name": self.config.model_name,
            "output_format": self.config.output_format,
            "close_ws_on_eos": self.config.close_ws_on_eos,
        }
        if self.config.voice_id:
            payload["voice_id"] = self.config.voice_id
        elif self.config.voice:
            payload["voice"] = self.config.voice
        json_config = self._build_json_config()
        if json_config is not None:
            payload["json_config"] = json_config
        if self.config.retry_for_s is not None:
            payload["retry_for_s"] = self.config.retry_for_s
        if self.config.pronunciation_id:
            payload["pronunciation_id"] = self.config.pronunciation_id

        for key, value in self.config.params.items():
            if value is None:
                continue
            payload[key] = value

        await self._send_json(payload)

    def _build_json_config(self) -> Any:
        """Parse the configured json_config string into an object for the wire.

        json_config is a JSON string (manifest schema only allows a string).
        Gradium expects an object, so parse it here. If it is already a dict
        or is not valid JSON, send it through unchanged.
        """
        raw = self.config.json_config
        if not raw:
            return None
        if isinstance(raw, dict):
            return raw
        try:
            return json.loads(raw)
        except (TypeError, ValueError):
            self.ten_env.log_warn(
                "json_config is not valid JSON; sending it as-is",
                category=LOG_CATEGORY_VENDOR,
            )
            return raw

    async def _wait_for_ready(self) -> None:
        assert self.ws is not None
        while True:
            try:
                raw_msg = await asyncio.wait_for(
                    self.ws.recv(),
                    timeout=WS_READY_TIMEOUT,
                )
            except Exception as exc:
                status_code = 401 if self._closed_during_setup(exc) else 500
                raise GradiumTTSConnectionException(
                    status_code=status_code,
                    body=str(exc),
                ) from exc
            message = self._parse_message(raw_msg)
            if message is None:
                continue

            msg_type = message.get("type")
            if msg_type == "ready":
                self._last_ready = message
                return
            if msg_type == "error":
                raise self._message_to_exception(message)

    async def _send_json(self, payload: dict[str, Any]) -> None:
        assert self.ws is not None
        await self.ws.send(json.dumps(payload))

    async def _iter_messages(
        self,
    ) -> AsyncIterator[tuple[bytes | int | None, int]]:
        assert self.ws is not None
        while True:
            if self._is_cancelled:
                break

            try:
                raw_msg = await asyncio.wait_for(
                    self.ws.recv(),
                    timeout=WS_RECV_TIMEOUT,
                )
            except asyncio.TimeoutError:
                yield b"Timeout waiting for Gradium audio", EVENT_TTS_ERROR
                break
            except Exception as exc:
                if self._is_cancelled:
                    break
                # A clean close (code 1000) is a normal end-of-stream, not an
                # error. On websockets >= 14 the code lives on exc.rcvd/exc.sent,
                # so read it via _close_code rather than a top-level exc.code.
                if self._close_code(exc) == 1000:
                    yield None, EVENT_TTS_END
                    break
                yield str(exc).encode("utf-8"), EVENT_TTS_ERROR
                break

            message = self._parse_message(raw_msg)
            if message is None:
                continue

            msg_type = message.get("type")
            if msg_type == "audio":
                audio_b64 = message.get("audio", "")
                if not audio_b64:
                    continue
                try:
                    audio_bytes = base64.b64decode(audio_b64)
                except Exception as exc:
                    self.ten_env.log_warn(
                        f"Failed to decode Gradium audio chunk: {exc}",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    continue

                if self._sent_ts and not self._ttfb_sent:
                    ttfb_ms = int(
                        (datetime.now() - self._sent_ts).total_seconds() * 1000
                    )
                    self._ttfb_sent = True
                    yield ttfb_ms, EVENT_TTS_TTFB_METRIC

                yield audio_bytes, EVENT_TTS_RESPONSE
            elif msg_type == "end_of_stream":
                yield None, EVENT_TTS_END
                break
            elif msg_type == "error":
                error = self._message_to_exception(message)
                yield error.body.encode("utf-8"), EVENT_TTS_ERROR
                break

    def _message_to_exception(
        self, message: dict[str, Any]
    ) -> GradiumTTSConnectionException:
        error_message = message.get("message", "Unknown error")
        code = str(message.get("code", ""))
        body = error_message if not code else f"{error_message} (code: {code})"
        status_code = (
            401 if code == "1008" or self._looks_like_auth_error(body) else 500
        )
        return GradiumTTSConnectionException(status_code=status_code, body=body)

    def _parse_message(self, raw_msg: str | bytes) -> dict[str, Any] | None:
        try:
            if isinstance(raw_msg, bytes):
                raw_msg = raw_msg.decode("utf-8")
            return json.loads(raw_msg)
        except Exception as exc:
            self.ten_env.log_warn(
                f"Failed to parse Gradium websocket message: {exc}",
                category=LOG_CATEGORY_VENDOR,
            )
            return None

    @staticmethod
    def _looks_like_auth_error(message: str) -> bool:
        lowered = message.lower()
        return any(
            token in lowered
            for token in (
                "401",
                "403",
                "1008",
                "unauthorized",
                "forbidden",
                "invalid api key",
                "invalid or expired api key",
                "expired api key",
                "authentication",
            )
        )

    @staticmethod
    def _close_code(exc: Exception) -> int | None:
        """Extract the websocket close code across websockets versions.

        websockets >= 14 exposes the close frame via ``exc.rcvd`` / ``exc.sent``
        (each a ``Close`` carrying a ``.code``) and no longer has a top-level
        ``exc.code``. Fall back to ``exc.code`` for older versions and for the
        hand-built exceptions used in tests.
        """
        for attr in ("rcvd", "sent"):
            code = getattr(getattr(exc, attr, None), "code", None)
            if isinstance(code, int):
                return code
        code = getattr(exc, "code", None)
        return code if isinstance(code, int) else None

    @classmethod
    def _closed_during_setup(cls, exc: Exception) -> bool:
        if cls._close_code(exc) in {1000, 1008, 1011}:
            return True
        return cls._looks_like_auth_error(str(exc))

    def get_extra_metadata(self) -> dict[str, str]:
        metadata = {
            "model_name": self.config.model_name,
            "output_format": self.config.output_format,
        }
        if self.config.voice_id:
            metadata["voice_id"] = self.config.voice_id
        if "sample_rate" in self._last_ready:
            metadata["sample_rate"] = str(self._last_ready["sample_rate"])
        return metadata

    def get_ready_sample_rate(self) -> int | None:
        sample_rate = self._last_ready.get("sample_rate")
        if isinstance(sample_rate, int):
            return sample_rate
        return None
