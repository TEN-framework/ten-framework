"""Breeze TTS client for the TEN Framework extension.

Wraps Breeze's streaming text-to-speech endpoint and yields raw PCM chunks in
the shape ``AsyncTTS2HttpClient`` expects.
"""

from __future__ import annotations

import asyncio
import os
from typing import Any, AsyncIterator, Tuple

import aiohttp
from pydantic import BaseModel, Field

from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

BYTES_PER_SAMPLE = 2
NUMBER_OF_CHANNELS = 1
SAMPLE_RATE = 24000
MAX_ATTEMPTS = 4
RETRY_STATUSES = frozenset({429, 500, 502, 503, 504})


def _retry_delay(attempt: int, headers: Any) -> float:
    """Backoff for a transient Breeze failure, honoring Retry-After."""

    retry_after = headers.get("retry-after") if headers else None
    if retry_after:
        try:
            return min(max(float(retry_after), 0.2), 5.0)
        except ValueError:
            pass
    return min(0.4 * (2 ** (attempt - 1)), 3.0)


class BreezeTTSParams(BaseModel):
    """Connection and synthesis parameters for Breeze."""

    api_key: str = Field(
        default_factory=lambda: os.getenv("BREEZE_API_KEY", ""),
        description="Breeze API key (brz_...)",
    )
    base_url: str = Field(
        default_factory=lambda: os.getenv(
            "BREEZE_BASE_URL", "https://api.breeze.blue"
        ),
        description="Breeze API base URL",
    )
    voice_id: str = Field(
        default="voc_u3nxkd5578wc", description="Breeze voice id"
    )
    language: str = Field(default="zh", description="ISO 639-1 language code")
    speed: float = Field(default=1.0, description="Speaking rate, 0.5 - 2.0")
    instructions: str = Field(
        default="", description="Optional style / emotion instructions"
    )
    timeout: int = Field(
        default=60, description="HTTP timeout in seconds for one utterance"
    )


class BreezeTTSClient(AsyncTTS2HttpClient):
    """Streaming Breeze client.

    Breeze only produces 24 kHz mono 16-bit PCM (``output_format=pcm``); the
    extension declares the same rate so no resampling is required.
    """

    def __init__(self, config: Any, ten_env: Any) -> None:
        super().__init__()
        self.config = config
        self.ten_env = ten_env
        self._session: aiohttp.ClientSession | None = None
        self._response: aiohttp.ClientResponse | None = None
        self._cancelled = False

    async def _ensure_session(self) -> aiohttp.ClientSession:
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(total=self.config.params.timeout)
            )
        return self._session

    async def clean(self) -> None:
        await self._close_response()
        if self._session is not None and not self._session.closed:
            await self._session.close()
        self._session = None

    async def cancel(self) -> None:
        self._cancelled = True
        await self._close_response()

    async def _close_response(self) -> None:
        if self._response is not None and not self._response.closed:
            self._response.close()
        self._response = None

    def get_extra_metadata(self) -> dict[str, Any]:
        params = self.config.params
        return {
            "voice_id": params.voice_id,
            "language": params.language,
            "sample_rate": SAMPLE_RATE,
        }

    def _build_request(self, text: str) -> tuple[str, dict[str, Any], dict[str, Any]]:
        params = self.config.params
        url = f"{params.base_url}/v1/text-to-speech/{params.voice_id}/stream"
        body: dict[str, Any] = {"text": text, "language_code": params.language}
        if params.instructions:
            body["instructions"] = params.instructions
        if params.speed != 1.0:
            body["voice_settings"] = {"speed": params.speed}
        headers = {"xi-api-key": params.api_key}
        return url, body, headers

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        self._cancelled = False

        if not text.strip():
            yield None, TTS2HttpResponseEventType.END
            return

        url, body, headers = self._build_request(text)
        session = await self._ensure_session()
        response = None

        try:
            for attempt in range(1, MAX_ATTEMPTS + 1):
                response = await session.post(
                    url,
                    params={"output_format": "pcm"},
                    json=body,
                    headers=headers,
                )
                self._response = response

                if response.status == 200:
                    break

                detail = (await response.text())[:300]
                if response.status in RETRY_STATUSES and attempt < MAX_ATTEMPTS:
                    delay = _retry_delay(attempt, response.headers)
                    self.ten_env.log_warn(
                        f"Breeze TTS HTTP {response.status} for request "
                        f"{request_id}, retrying in {delay:.1f}s "
                        f"(attempt {attempt}/{MAX_ATTEMPTS})"
                    )
                    response.release()
                    response = None
                    self._response = None
                    await asyncio.sleep(delay)
                    continue

                self.ten_env.log_error(
                    f"Breeze TTS HTTP {response.status} for request "
                    f"{request_id}: {detail}"
                )
                response.release()
                self._response = None
                yield detail.encode(), (
                    TTS2HttpResponseEventType.INVALID_KEY_ERROR
                    if response.status in (401, 403)
                    else TTS2HttpResponseEventType.ERROR
                )
                return

            if response is None:
                yield b"Breeze TTS request failed", TTS2HttpResponseEventType.ERROR
                return

            async for chunk in response.content.iter_chunked(4096):
                if self._cancelled:
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return
                if chunk:
                    yield chunk, TTS2HttpResponseEventType.RESPONSE

            if self._cancelled:
                yield None, TTS2HttpResponseEventType.FLUSH
                return
            yield None, TTS2HttpResponseEventType.END

        except asyncio.CancelledError:
            yield None, TTS2HttpResponseEventType.FLUSH
            raise
        except Exception as error:  # noqa: BLE001 - surfaced to the pipeline
            self.ten_env.log_error(
                f"Breeze TTS request {request_id} failed: {error}"
            )
            yield str(error).encode(), TTS2HttpResponseEventType.ERROR
        finally:
            self._response = None
            if response is not None and not response.closed:
                response.release()
