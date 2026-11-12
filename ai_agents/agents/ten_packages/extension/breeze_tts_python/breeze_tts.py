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


AUDIO_CONTENT_TYPES = ("audio/pcm", "audio/x-pcm", "audio/l16", "application/octet-stream")


def _is_audio_type(content_type: str) -> bool:
    if not content_type:
        return True  # a missing header is not proof of an error page
    return content_type.startswith("audio/") or any(
        content_type.startswith(kind) for kind in AUDIO_CONTENT_TYPES
    )


CREDENTIAL_HINTS = ("api key", "api_key", "credential", "unauthorized", "invalid key")


def _is_credential_failure(status: int, detail: str) -> bool:
    """Auth failures are fatal; request validation errors stay recoverable."""

    if status in (401, 403):
        return True
    if status in (400, 422):
        lowered = (detail or "").lower()
        return any(hint in lowered for hint in CREDENTIAL_HINTS)
    return False


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
        self._responses: dict[str, aiohttp.ClientResponse] = {}
        self._pending_posts: dict[str, asyncio.Task] = {}
        self._cancel_epoch = 0
        self._resampler: LinearResampler | None = None
        self._carry = b""

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
        self._cancel_epoch += 1
        for task in list(self._pending_posts.values()):
            if not task.done():
                task.cancel()
        self._pending_posts.clear()
        await self._close_response()

    async def _close_response(self) -> None:
        for response in self._responses.values():
            if not response.closed:
                response.close()
        self._responses.clear()

    def target_sample_rate(self) -> int:
        rate = getattr(self.config.params, "sample_rate", SAMPLE_RATE)
        return rate if rate in SUPPORTED_SAMPLE_RATES else SAMPLE_RATE

    def get_extra_metadata(self) -> dict[str, Any]:
        params = self.config.params
        return {
            "voice_id": params.voice_id,
            "sample_rate": self.target_sample_rate(),
            "language": params.language,
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
        epoch = self._cancel_epoch
        self._carry = b""
        target_rate = self.target_sample_rate()
        self._resampler = (
            None if target_rate == SAMPLE_RATE else LinearResampler(SAMPLE_RATE, target_rate)
        )

        if not text.strip():
            yield None, TTS2HttpResponseEventType.END
            return

        url, body, headers = self._build_request(text)
        session = await self._ensure_session()
        emitted = 0

        for attempt in range(1, MAX_ATTEMPTS + 1):
            response = None
            try:
                post_task = asyncio.ensure_future(
                    session.post(
                        url,
                        params={"output_format": "pcm"},
                        json=body,
                        headers=headers,
                    )
                )
                self._pending_posts[request_id] = post_task
                try:
                    response = await post_task
                except asyncio.CancelledError:
                    if epoch != self._cancel_epoch:
                        yield None, TTS2HttpResponseEventType.FLUSH
                        return
                    raise
                finally:
                    self._pending_posts.pop(request_id, None)

                if epoch != self._cancel_epoch:
                    response.release()
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return
                self._responses[request_id] = response

                content_type = (response.headers.get("Content-Type") or "").lower()
                if response.status == 200 and not _is_audio_type(content_type):
                    detail = (await response.text())[:300]
                    self.ten_env.log_error(
                        f"Breeze TTS returned {content_type or 'no content type'} "
                        f"for request {request_id}: {detail}"
                    )
                    response.release()
                    self._responses.pop(request_id, None)
                    yield detail.encode(), TTS2HttpResponseEventType.ERROR
                    return

                if response.status != 200:
                    detail = (await response.text())[:300]
                    retryable = (
                        response.status in RETRY_STATUSES
                        and attempt < MAX_ATTEMPTS
                    )
                    if retryable:
                        delay = _retry_delay(attempt, response.headers)
                        self.ten_env.log_warn(
                            f"Breeze TTS HTTP {response.status} for request "
                            f"{request_id}, retrying in {delay:.1f}s "
                            f"(attempt {attempt}/{MAX_ATTEMPTS})"
                        )
                        response.release()
                        self._responses.pop(request_id, None)
                        response = None
                        await asyncio.sleep(delay)
                        if epoch != self._cancel_epoch:
                            yield None, TTS2HttpResponseEventType.FLUSH
                            return
                        continue

                    self.ten_env.log_error(
                        f"Breeze TTS HTTP {response.status} for request "
                        f"{request_id}: {detail}"
                    )
                    response.release()
                    self._responses.pop(request_id, None)
                    yield detail.encode(), (
                        TTS2HttpResponseEventType.INVALID_KEY_ERROR
                        if _is_credential_failure(response.status, detail)
                        else TTS2HttpResponseEventType.ERROR
                    )
                    return

                async for chunk in response.content.iter_chunked(4096):
                    if epoch != self._cancel_epoch:
                        yield None, TTS2HttpResponseEventType.FLUSH
                        return
                    chunk = self._carry + chunk
                    # keep a trailing odd byte so 16-bit samples stay aligned
                    self._carry = chunk[-1:] if len(chunk) % 2 else b""
                    chunk = chunk[: len(chunk) - len(self._carry)]
                    if self._resampler is not None:
                        chunk = self._resampler.process(chunk)
                    if chunk:
                        emitted += len(chunk)
                        yield chunk, TTS2HttpResponseEventType.RESPONSE

                if epoch != self._cancel_epoch:
                    self._carry = b""
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return

                if self._carry:
                    # the vendor ended mid-sample: report it rather than
                    # fabricating the missing half sample
                    self._carry = b""
                    message = (
                        f"Breeze TTS response for request {request_id} ended "
                        "with a truncated PCM sample"
                    )
                    self.ten_env.log_error(message)
                    yield message.encode(), TTS2HttpResponseEventType.ERROR
                    return
                yield None, TTS2HttpResponseEventType.END
                return

            except asyncio.CancelledError:
                yield None, TTS2HttpResponseEventType.FLUSH
                raise
            except aiohttp.ClientError as error:
                if epoch != self._cancel_epoch:
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return
                if emitted == 0 and attempt < MAX_ATTEMPTS:
                    self.ten_env.log_warn(
                        f"Breeze TTS request {request_id} dropped before audio "
                        f"({error}), retrying (attempt {attempt}/{MAX_ATTEMPTS})"
                    )
                    await asyncio.sleep(min(0.4 * attempt, 2.0))
                    if epoch != self._cancel_epoch:
                        yield None, TTS2HttpResponseEventType.FLUSH
                        return
                    continue
                self.ten_env.log_error(
                    f"Breeze TTS request {request_id} failed: {error}"
                )
                yield str(error).encode(), TTS2HttpResponseEventType.ERROR
                return
            except Exception as error:  # noqa: BLE001 - surfaced to the pipeline
                if epoch != self._cancel_epoch:
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return
                self.ten_env.log_error(
                    f"Breeze TTS request {request_id} failed: {error}"
                )
                yield str(error).encode(), TTS2HttpResponseEventType.ERROR
                return
            finally:
                self._responses.pop(request_id, None)
                if response is not None and not response.closed:
                    response.release()

        yield b"Breeze TTS request failed", TTS2HttpResponseEventType.ERROR
