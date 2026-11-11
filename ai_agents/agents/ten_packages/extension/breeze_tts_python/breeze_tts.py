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
SUPPORTED_SAMPLE_RATES = (16000, 22050, 24000, 44100)


class LinearResampler:
    """Streaming linear resampler for 16-bit mono PCM.

    Breeze always synthesises 24 kHz; when a graph asks for another rate we
    convert chunk by chunk so audio can still start before the stream ends.
    """

    def __init__(self, in_rate: int, out_rate: int) -> None:
        self.step = in_rate / out_rate
        self.pos = 0.0
        self.prev: float | None = None

    def process(self, pcm: bytes) -> bytes:
        import array

        if self.step == 1.0 or not pcm:
            return pcm

        samples = array.array("h")
        samples.frombytes(pcm[: len(pcm) - len(pcm) % 2])
        if not samples:
            return b""

        data = [samples[0] if self.prev is None else self.prev, *samples]
        out = array.array("h")
        pos = self.pos
        last = len(data) - 1
        while pos < last:
            idx = int(pos)
            frac = pos - idx
            value = data[idx] * (1.0 - frac) + data[idx + 1] * frac
            out.append(max(-32768, min(32767, int(value))))
            pos += self.step
        self.pos = pos - last
        self.prev = float(data[-1])
        return out.tobytes()


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
    sample_rate: int = Field(
        default=24000,
        description="Requested output rate; Breeze renders 24 kHz and the "
        "extension resamples to 16k / 22.05k / 44.1k when configured",
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
        self._resampler: LinearResampler | None = None

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

    def target_sample_rate(self) -> int:
        rate = getattr(self.config.params, "sample_rate", SAMPLE_RATE)
        return rate if rate in SUPPORTED_SAMPLE_RATES else SAMPLE_RATE

    def get_extra_metadata(self) -> dict[str, Any]:
        params = self.config.params
        return {
            "voice_id": params.voice_id,
            "sample_rate": self.target_sample_rate(),
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
        target_rate = self.target_sample_rate()
        self._resampler = (
            None if target_rate == SAMPLE_RATE else LinearResampler(SAMPLE_RATE, target_rate)
        )

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
                    if response.status in (400, 401, 403, 422)
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
                if self._resampler is not None:
                    chunk = self._resampler.process(chunk)
                if chunk:
                    yield chunk, TTS2HttpResponseEventType.RESPONSE

            if self._cancelled:
                yield None, TTS2HttpResponseEventType.FLUSH
                return
            yield None, TTS2HttpResponseEventType.END

        except asyncio.CancelledError:
            yield None, TTS2HttpResponseEventType.FLUSH
            raise
        except aiohttp.ClientError as error:
            if self._cancelled:
                yield None, TTS2HttpResponseEventType.FLUSH
                return
            self.ten_env.log_error(
                f"Breeze TTS request {request_id} failed: {error}"
            )
            yield str(error).encode(), TTS2HttpResponseEventType.ERROR
        except Exception as error:  # noqa: BLE001 - surfaced to the pipeline
            if self._cancelled:
                yield None, TTS2HttpResponseEventType.FLUSH
                return
            self.ten_env.log_error(
                f"Breeze TTS request {request_id} failed: {error}"
            )
            yield str(error).encode(), TTS2HttpResponseEventType.ERROR
        finally:
            self._response = None
            if response is not None and not response.closed:
                response.release()
