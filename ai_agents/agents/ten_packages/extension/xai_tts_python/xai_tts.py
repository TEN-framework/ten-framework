import asyncio
import base64
import json
import random
from datetime import datetime
from typing import AsyncIterator
from urllib.parse import urlencode

import websockets
from websockets.asyncio.client import ClientConnection
from websockets.exceptions import InvalidStatus
from websockets.protocol import State

from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_runtime import AsyncTenEnv

from .config import XAITTSConfig, _is_sensitive_key


EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_TTFB_METRIC = 5

WS_RECV_TIMEOUT = 8.0
MAX_CONNECT_ATTEMPTS = 5
BASE_BACKOFF_SECONDS = 0.5


class XAITTSConnectionException(Exception):
    def __init__(self, status_code: int, body: str):
        self.status_code = status_code
        self.body = body
        super().__init__(
            f"xAI TTS connection failed (code: {status_code}): {body}"
        )


class XAITTSClient:
    def __init__(self, config: XAITTSConfig, ten_env: AsyncTenEnv):
        self.config = config
        self.ten_env = ten_env
        self._ws: ClientConnection | None = None
        self._is_cancelled = False
        self._needs_reconnect = False
        self._sent_ts: datetime | None = None
        self._ttfb_sent = False
        self._ws_url = self._build_ws_url()
        self._connect_exp_cnt = 0

    def _build_ws_url(self) -> str:
        def _encode_query_value(value: str | int | bool) -> str | int:
            if isinstance(value, bool):
                return str(value).lower()
            return value

        query_params: dict[str, str | int | bool] = {
            "voice": self.config.voice_id,
            "language": self.config.language,
            "codec": self.config.codec,
            "sample_rate": self.config.sample_rate,
        }
        if self.config.codec == "mp3":
            query_params["bit_rate"] = self.config.bit_rate
        query_params["optimize_streaming_latency"] = (
            self.config.optimize_streaming_latency
        )
        query_params["text_normalization"] = self.config.text_normalization
        for key, value in self.config.params.items():
            if key in {
                "api_key",
                "base_url",
                "voice_id",
                "language",
                "codec",
                "sample_rate",
                "bit_rate",
                "optimize_streaming_latency",
                "text_normalization",
            }:
                continue
            # Defensive: never forward a key whose name looks sensitive into
            # the URL where it could land in proxy logs.
            if _is_sensitive_key(key):
                self.ten_env.log_warn(
                    f"dropping sensitive-looking param {key!r} from ws url"
                )
                continue
            if value is not None:
                query_params[key] = value
        encoded_query_params = {
            key: _encode_query_value(value)
            for key, value in query_params.items()
        }
        return (
            f"{self.config.base_url}?"
            f"{urlencode(encoded_query_params, doseq=True)}"
        )

    async def start(self) -> None:
        await self._connect_with_backoff("preheat")

    async def stop(self) -> None:
        self._is_cancelled = True
        if self._ws:
            try:
                self.ten_env.log_info(
                    "vendor_status_changed: closing xai tts websocket",
                    category=LOG_CATEGORY_VENDOR,
                )
                await self._ws.close()
            finally:
                self._ws = None

    async def cancel(self) -> None:
        self._is_cancelled = True
        self.reset_ttfb()
        self._needs_reconnect = True
        if self._ws:
            try:
                self.ten_env.log_info(
                    "vendor_status_changed: cancelling xai tts websocket",
                    category=LOG_CATEGORY_VENDOR,
                )
                await self._ws.close()
            finally:
                self._ws = None

    def reset_ttfb(self) -> None:
        self._sent_ts = None
        self._ttfb_sent = False

    def is_connected(self) -> bool:
        return self._ws is not None and self._ws.state == State.OPEN

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | int | None, int]]:
        if len(text.strip()) == 0:
            yield None, EVENT_TTS_END
            return

        # A previous cancel()/stop() may have set _is_cancelled = True. A
        # fresh request must clear it before reaching _reconnect /
        # _ensure_connection, otherwise the backoff loop short-circuits
        # with a 499 and the next TTS request after a barge-in fails.
        self._is_cancelled = False

        if self._needs_reconnect:
            await self._reconnect()
            self._needs_reconnect = False

        await self._ensure_connection()
        if not self._ttfb_sent:
            self._sent_ts = datetime.now()

        await self._ws.send(json.dumps({"type": "text.delta", "delta": text}))
        await self._ws.send(json.dumps({"type": "text.done"}))

        try:
            while True:
                if self._is_cancelled:
                    break

                try:
                    message = await asyncio.wait_for(
                        self._ws.recv(), timeout=WS_RECV_TIMEOUT
                    )
                except asyncio.TimeoutError:
                    self._needs_reconnect = True
                    yield b"Timeout waiting for xAI audio", EVENT_TTS_ERROR
                    break

                if isinstance(message, bytes):
                    self.ten_env.log_warn(
                        "Unexpected binary frame from xAI TTS; ignoring"
                    )
                    continue

                try:
                    event = json.loads(message)
                except json.JSONDecodeError:
                    self.ten_env.log_warn(
                        f"Failed to parse xAI TTS frame: {message}"
                    )
                    continue

                event_type = event.get("type", "")
                if event_type == "audio.delta":
                    if self._sent_ts and not self._ttfb_sent:
                        yield (
                            int(
                                (datetime.now() - self._sent_ts).total_seconds()
                                * 1000
                            ),
                            EVENT_TTS_TTFB_METRIC,
                        )
                        self._ttfb_sent = True
                    audio_chunk = base64.b64decode(event.get("delta", ""))
                    yield audio_chunk, EVENT_TTS_RESPONSE
                elif event_type == "audio.done":
                    yield None, EVENT_TTS_END
                    break
                elif event_type == "error":
                    self._needs_reconnect = True
                    yield (
                        str(event.get("message", "Unknown error")).encode(
                            "utf-8"
                        ),
                        EVENT_TTS_ERROR,
                    )
                    break

        except Exception as e:
            self.ten_env.log_error(
                f"vendor_error: {e}", category=LOG_CATEGORY_VENDOR
            )
            self._needs_reconnect = True
            yield str(e).encode("utf-8"), EVENT_TTS_ERROR

    async def _connect(self) -> None:
        try:
            self.ten_env.log_info(
                "vendor_status_changed: connecting to xai tts",
                category=LOG_CATEGORY_VENDOR,
            )
            self._ws = await websockets.connect(
                self._ws_url,
                additional_headers={
                    "Authorization": f"Bearer {self.config.api_key}"
                },
            )
            self._connect_exp_cnt = 0
            self.ten_env.log_info(
                "vendor_status: connected to xai tts",
                category=LOG_CATEGORY_VENDOR,
            )
        except InvalidStatus as e:
            response = getattr(e, "response", None)
            status_code = getattr(response, "status_code", None) or getattr(
                e, "status_code", 0
            )
            raise XAITTSConnectionException(
                status_code=int(status_code or 0),
                body=str(e),
            ) from e
        except Exception as e:
            error_message = str(e)
            if "401" in error_message or "Unauthorized" in error_message:
                raise XAITTSConnectionException(
                    status_code=401, body=error_message
                ) from e
            raise

    async def _connect_with_backoff(self, reason: str) -> None:
        last_error: Exception | None = None
        attempt = 0
        while attempt < MAX_CONNECT_ATTEMPTS:
            if self._is_cancelled:
                raise XAITTSConnectionException(
                    status_code=499,
                    body="xAI TTS connection cancelled",
                )
            try:
                await self._connect()
                return
            except XAITTSConnectionException as e:
                if e.status_code in {401, 403}:
                    raise
                last_error = e
            except Exception as e:
                last_error = e

            attempt += 1
            self._connect_exp_cnt = attempt
            if attempt >= MAX_CONNECT_ATTEMPTS:
                break

            base = min(BASE_BACKOFF_SECONDS * (2 ** (attempt - 1)), 4.0)
            # Add ±20% jitter so multi-replica deployments do not all
            # reconnect at the same millisecond after a shared outage.
            backoff_seconds = base * (0.8 + random.random() * 0.4)
            self.ten_env.log_info(
                f"vendor_status_changed: retrying xai tts websocket "
                f"after {reason} failure in {backoff_seconds:.2f}s "
                f"(attempt {attempt}/{MAX_CONNECT_ATTEMPTS})",
                category=LOG_CATEGORY_VENDOR,
            )
            await asyncio.sleep(backoff_seconds)
            if self._is_cancelled:
                raise XAITTSConnectionException(
                    status_code=499,
                    body="xAI TTS connection cancelled",
                )

        message = (
            str(last_error)
            if last_error is not None
            else "xAI TTS connection failed"
        )
        raise XAITTSConnectionException(status_code=503, body=message)

    async def _ensure_connection(self) -> None:
        if not self.is_connected():
            await self._connect_with_backoff("connect")

    async def _reconnect(self) -> None:
        if self._ws:
            try:
                await self._ws.close()
            except Exception:
                pass
            self._ws = None
        await self._connect_with_backoff("reconnect")
