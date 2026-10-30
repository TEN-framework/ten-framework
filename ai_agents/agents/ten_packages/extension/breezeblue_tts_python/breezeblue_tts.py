"""
BreezeBlue TTS Client Implementation using httpx.

Endpoint: `POST {base_url}/v1/text-to-speech/{voice_id}/stream?output_format=pcm`
(default base_url `https://api.breeze.blue`), authenticated with the
`xi-api-key` header. The response body is a raw stream of 16-bit little-endian
PCM, 24 kHz mono, which is what TEN expects, so chunks are forwarded as-is
except that a trailing odd byte is held back so a sample is never split across
two frames.
"""

from collections.abc import Mapping
from typing import Any, AsyncIterator, Tuple
import asyncio
import json
import re
import ssl
import certifi
import time

# ============================================================================
# Performance Optimization: Module-level pre-import of httpx
# ============================================================================
import httpcore  # noqa: F401  # pylint: disable=unused-import  # Note: This import cannot be removed, otherwise it will affect http client initialization time
import httpx  # noqa: F401  # pylint: disable=unused-import
from httpx import AsyncClient, Timeout, Limits

from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

from .config import BreezeBlueTTSConfig, DEFAULT_OUTPUT_FORMAT

# Pre-create a global SSL context so every httpx.AsyncClient reuses it instead
# of re-loading CA certificates.
_GLOBAL_SSL_CONTEXT = ssl.create_default_context(cafile=certifi.where())

_PCM16_SAMPLE_BYTES = 2

# Transient upstream failures worth retrying, but only before any audio has
# been produced (an interrupted stream can't be resumed without repeating it).
_RETRY_STATUS = (502, 503, 504)
_RETRY_DELAYS_S = (0.3, 0.8)

_SENTENCE_END = re.compile(r"(?<=[.!?。！？;；\n])")


def split_text(text: str, limit: int) -> list[str]:
    """Split text into chunks of at most `limit` characters.

    The vendor rejects (HTTP 422) text over its per-request cap instead of
    truncating, and AsyncTTS2HttpExtension passes LLM chunks through unsplit.
    Split at sentence boundaries first, packing sentences greedily, and fall
    back to whitespace, then a hard cut, for a single over-long sentence.
    """
    if len(text) <= limit:
        return [text]

    pieces: list[str] = []
    for sentence in _SENTENCE_END.split(text):
        while len(sentence) > limit:
            cut = sentence.rfind(" ", 0, limit)
            cut = cut if cut > 0 else limit
            pieces.append(sentence[:cut])
            sentence = sentence[cut:]
        if sentence:
            pieces.append(sentence)

    chunks: list[str] = []
    current = ""
    for piece in pieces:
        if current and len(current) + len(piece) > limit:
            chunks.append(current)
            current = ""
        current += piece
    if current:
        chunks.append(current)
    return [c for c in chunks if c.strip()]


class BreezeBlueTTSClient(AsyncTTS2HttpClient):
    """
    BreezeBlue TTS Client using httpx.

    Features:
    - HTTP streaming with raw PCM16 24 kHz mono output
    - Whitelisted request body (the vendor rejects unknown fields)
    - Error mapping from the `{ok, code, detail}` envelope
    - Cancellation support
    """

    def __init__(
        self,
        config: BreezeBlueTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        # Response currently being streamed, so cancel() can close it instead
        # of waiting for the next chunk boundary.
        self._response: httpx.Response | None = None

        api_key = self.config.params.get("api_key", "")
        default_headers = {
            "xi-api-key": api_key,
            "Content-Type": "application/json",
        }
        # Merge: user headers override defaults
        self.headers = {**default_headers, **self.config.headers}

        _start_time = time.time()
        self.client = AsyncClient(
            timeout=Timeout(timeout=60.0),
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,
            ),
            http2=True,
            verify=_GLOBAL_SSL_CONTEXT,
        )
        _elapsed_ms = (time.time() - _start_time) * 1000
        ten_env.log_debug(f"http client initialized in {_elapsed_ms:.2f}ms")

        ten_env.log_info(
            f"BreezeBlueTTS initialized with endpoint: {self.config.url}"
        )

    async def cancel(self):
        """Cancel the current TTS request."""
        self.ten_env.log_debug("BreezeBlueTTS: cancel() called.")
        self._is_cancelled = True
        response = self._response
        if response is not None:
            # Stop the transfer now; the streaming loop sees the flag and
            # flushes. Closing is idempotent and may race with normal
            # completion, so errors here are not interesting.
            try:
                await response.aclose()
            except Exception:  # pylint: disable=broad-except
                pass

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """
        Process a single TTS request.

        Yields:
            Tuple of (audio_bytes, event_type):
            - (bytes, RESPONSE): PCM16 mono audio chunk
            - (None, END): Successful completion
            - (None, FLUSH): Cancelled
            - (bytes, ERROR): Error message
            - (bytes, INVALID_KEY_ERROR): Authentication error
        """
        self._is_cancelled = False

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"BreezeBlueTTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        chunks = split_text(text, self.config.max_text_length)
        if len(chunks) > 1:
            self.ten_env.log_info(
                f"BreezeBlueTTS: split {len(text)} chars into {len(chunks)} "
                f"requests for request_id: {request_id}"
            )

        for index, chunk in enumerate(chunks):
            is_last = index == len(chunks) - 1
            async for audio, event in self._request(chunk, request_id):
                if event == TTS2HttpResponseEventType.END:
                    if is_last:
                        yield audio, event
                    continue
                yield audio, event
                if event != TTS2HttpResponseEventType.RESPONSE:
                    # ERROR / INVALID_KEY_ERROR / FLUSH end the whole request.
                    return

    async def _request(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """Synthesize one chunk (<= max_text_length), retrying transient errors."""
        try:
            payload = self.config.build_request_body(text)

            for attempt in range(len(_RETRY_DELAYS_S) + 1):
                self.ten_env.log_debug(
                    f"BreezeBlueTTS: sending request for request_id: {request_id}"
                )
                leftover = b""
                sent_audio = False
                async with self.client.stream(
                    "POST",
                    self.config.url,
                    params={"output_format": DEFAULT_OUTPUT_FORMAT},
                    headers=self.headers,
                    json=payload,
                ) as response:
                    self._response = response
                    if self._is_cancelled:
                        yield None, TTS2HttpResponseEventType.FLUSH
                        return

                    if response.status_code != 200:
                        error_body = await response.aread()
                        error_msg, error_code = self._parse_error(error_body)

                        if (
                            response.status_code in _RETRY_STATUS
                            and attempt < len(_RETRY_DELAYS_S)
                        ):
                            self.ten_env.log_warn(
                                f"BreezeBlueTTS: HTTP {response.status_code} "
                                f"{error_code or ''}, retrying "
                                f"({attempt + 1}/{len(_RETRY_DELAYS_S)}) "
                                f"for request_id: {request_id}",
                                category=LOG_CATEGORY_VENDOR,
                            )
                            await asyncio.sleep(_RETRY_DELAYS_S[attempt])
                            continue

                        self.ten_env.log_error(
                            f"vendor_error: HTTP {response.status_code} "
                            f"{error_code or ''}: {error_msg} "
                            f"for request_id: {request_id}",
                            category=LOG_CATEGORY_VENDOR,
                        )
                        if self._is_invalid_key(response.status_code):
                            yield error_msg.encode(
                                "utf-8"
                            ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
                        else:
                            yield error_msg.encode(
                                "utf-8"
                            ), TTS2HttpResponseEventType.ERROR
                        return

                    # A 200 that is not audio (JSON/HTML from a proxy or an
                    # error page) must never be played as PCM.
                    content_type = ""
                    if isinstance(response.headers, Mapping):
                        content_type = response.headers.get(
                            "content-type", ""
                        ).lower()
                    if content_type.startswith(("application/json", "text/")):
                        body = await response.aread()
                        error_msg, error_code = self._parse_error(body)
                        self.ten_env.log_error(
                            f"vendor_error: unexpected content-type "
                            f"{content_type!r} {error_code or ''}: {error_msg} "
                            f"for request_id: {request_id}",
                            category=LOG_CATEGORY_VENDOR,
                        )
                        yield error_msg.encode(
                            "utf-8"
                        ), TTS2HttpResponseEventType.ERROR
                        return

                    async for chunk in response.aiter_bytes():
                        if self._is_cancelled:
                            self.ten_env.log_debug(
                                f"Cancellation detected, flushing TTS stream for request_id: {request_id}"
                            )
                            yield None, TTS2HttpResponseEventType.FLUSH
                            return

                        data = leftover + chunk
                        usable = len(data) - (len(data) % _PCM16_SAMPLE_BYTES)
                        leftover = data[usable:]
                        if usable:
                            sent_audio = True
                            yield data[
                                :usable
                            ], TTS2HttpResponseEventType.RESPONSE

                if not self._is_cancelled:
                    self.ten_env.log_debug(
                        f"BreezeBlueTTS: sending END event for request_id: {request_id} (audio: {sent_audio})"
                    )
                    yield None, TTS2HttpResponseEventType.END
                return

        except Exception as e:  # pylint: disable=broad-except
            if self._is_cancelled:
                # cancel() closed the response under us; this is the flush
                # boundary, not a vendor failure.
                yield None, TTS2HttpResponseEventType.FLUSH
                return
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message} for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )
            # Network errors, timeouts, etc. are all general errors.
            yield error_message.encode("utf-8"), TTS2HttpResponseEventType.ERROR
        finally:
            self._response = None

    def _is_invalid_key(self, status_code: int) -> bool:
        """401/403 are auth failures. The API also answers a malformed key
        (no `brz_` prefix) with a generic 422 "Request validation failed",
        which would otherwise hide the real problem."""
        if status_code in (401, 403):
            return True
        api_key = self.config.params.get("api_key", "")
        return (
            status_code == 422
            and bool(api_key)
            and not api_key.startswith("brz_")
        )

    @staticmethod
    def _parse_error(body: bytes) -> Tuple[str, str | None]:
        """Parse the `{ok, code, detail, error}` envelope into (message, code)."""
        try:
            data = json.loads(body)
            if isinstance(data, dict):
                msg = data.get("detail") or data.get("error") or str(data)
                if not isinstance(msg, str):
                    msg = json.dumps(msg, ensure_ascii=False)
                return msg, data.get("code")
        except Exception:  # pylint: disable=broad-except
            pass
        return body.decode("utf-8", errors="replace"), None

    async def clean(self):
        """Clean up resources."""
        self.ten_env.log_debug("BreezeBlueTTS: clean() called.")
        if self.client:
            await self.client.aclose()

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for TTFB metrics."""
        return {
            "model": self.config.params.get("model_id", ""),
            "voice": self.config.params.get("voice_id", ""),
        }
