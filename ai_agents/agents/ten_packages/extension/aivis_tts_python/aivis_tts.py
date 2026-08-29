#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Aivis Cloud TTS HTTP client — streams WAV, yields PCM chunks.

The base class ``AsyncTTS2HttpClient`` owns ``metrics_add_recv_audio_chunks``
and calculates TTFB from the first PCM chunk we emit.  We therefore must
yield the first PCM chunk as soon as the WAV header is parsed, before
iterating the rest of the stream.
"""

from typing import Any, AsyncIterator, Tuple

from httpx import AsyncClient, Timeout, Limits

from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

from .config import AivisTTSConfig
from .wav_stream_parser import WavStreamParser


class AivisTTSClient(AsyncTTS2HttpClient):
    def __init__(
        self,
        config: AivisTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.api_key = config.params.get("api_key", "")
        self.client = AsyncClient(
            timeout=Timeout(timeout=60.0),
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,
            ),
            http2=True,
        )

    def _headers(self) -> dict[str, str]:
        return {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
            "Accept": "audio/wav, application/octet-stream, */*",
        }

    async def cancel(self):
        self.ten_env.log_debug("AivisTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        self._is_cancelled = False

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"AivisTTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        try:
            url = self.config.synthesize_url()
            body = self.config.request_body(text)
            self.ten_env.log_debug(
                f"AivisTTS: POST {url} request_id={request_id} "
                f"chars={len(text)} model_uuid={body.get('model_uuid')}",
                category=LOG_CATEGORY_VENDOR,
            )

            async with self.client.stream(
                "POST",
                url,
                headers=self._headers(),
                json=body,
            ) as response:
                if response.status_code in (401, 403):
                    error_message = (
                        f"Aivis auth error HTTP {response.status_code}: "
                        f"{await response.aread()}"
                    )
                    self.ten_env.log_error(
                        f"vendor_error: {error_message} of request_id: {request_id}.",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    yield error_message.encode(
                        "utf-8"
                    ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
                    return

                if response.status_code >= 400:
                    error_body = await response.aread()
                    error_message = (
                        f"Aivis HTTP {response.status_code}: "
                        f"{error_body.decode('utf-8', errors='replace')}"
                    )
                    self.ten_env.log_error(
                        f"vendor_error: {error_message} of request_id: {request_id}.",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    yield error_message.encode(
                        "utf-8"
                    ), TTS2HttpResponseEventType.ERROR
                    return

                async def byte_stream() -> AsyncIterator[bytes]:
                    async for chunk in response.aiter_bytes(chunk_size=4096):
                        if chunk:
                            yield chunk

                parser = WavStreamParser(byte_stream())
                # Parse header (consumes the first chunk from the stream and
                # buffers the leading PCM bytes internally).
                await parser.get_format_info()

                # Pull the first PCM chunk off the parser and yield it
                # immediately so the base class can mark TTFB against the
                # first audio byte. We iterate via anext rather than reaching
                # into the parser's private _first_pcm_chunk attribute.
                first_chunk = await parser.__anext__()
                if len(first_chunk) > 0:
                    self.ten_env.log_debug(
                        f"AivisTTS: first chunk len={len(first_chunk)} "
                        f"request_id={request_id}",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    yield bytes(first_chunk), TTS2HttpResponseEventType.RESPONSE

                # Continue with the rest of the stream. Empty chunks are
                # filtered before yielding.
                async for pcm_chunk in parser:
                    if self._is_cancelled:
                        self.ten_env.log_debug(
                            f"AivisTTS: cancelled, flushing request_id: {request_id}."
                        )
                        yield None, TTS2HttpResponseEventType.FLUSH
                        return

                    if len(pcm_chunk) > 0:
                        yield bytes(pcm_chunk), TTS2HttpResponseEventType.RESPONSE

            if not self._is_cancelled:
                yield None, TTS2HttpResponseEventType.END

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"AivisTTS: vendor_error: {error_message} of request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            if "401" in error_message or "403" in error_message:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.ERROR

    async def clean(self):
        """Release the underlying httpx client."""
        self.ten_env.log_debug("AivisTTS: clean() called.")
        try:
            await self.client.aclose()
        except Exception as exc:
            self.ten_env.log_warn(
                f"AivisTTS: client.aclose() raised: {exc}.",
                category=LOG_CATEGORY_VENDOR,
            )

    def get_extra_metadata(self) -> dict[str, Any]:
        return {
            "model_uuid": self.config.params.get("model_uuid", ""),
            "language": self.config.params.get("language", "ja"),
            "output_sampling_rate": self.config.params.get(
                "output_sampling_rate", 16000
            ),
        }
