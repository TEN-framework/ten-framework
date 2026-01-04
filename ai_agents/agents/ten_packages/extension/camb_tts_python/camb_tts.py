#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any, AsyncIterator, Tuple
from httpx import AsyncClient, Timeout, Limits

from .config import CambTTSConfig
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient


BYTES_PER_SAMPLE = 2
NUMBER_OF_CHANNELS = 1
SAMPLE_RATE = 24000


class CambTTSClient(AsyncTTS2HttpClient):
    def __init__(
        self,
        config: CambTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.api_key = config.params.get("api_key", "")
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.endpoint = config.params.get(
            "endpoint", "https://client.camb.ai/apis/tts-stream"
        )
        self.headers = {
            "x-api-key": self.api_key,
            "Content-Type": "application/json",
            "Accept": "application/json",
        }
        # Camb.ai TTS requires longer timeout (minimum 60s recommended)
        self.client = AsyncClient(
            timeout=Timeout(timeout=60.0),
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,  # 10 minutes keepalive
            ),
            http2=True,  # Enable HTTP/2 if server supports it
        )

    async def cancel(self):
        self.ten_env.log_debug("CambTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """Process a single TTS request in serial manner"""
        self._is_cancelled = False
        if not self.client:
            self.ten_env.log_error(
                f"CambTTS: client not initialized for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"CambTTS: client not initialized for request_id: {request_id}."
            )

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"CambTTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        # Validate text length (Camb.ai requires 3-3000 characters)
        text_len = len(text.strip())
        if text_len < 3:
            self.ten_env.log_warn(
                f"CambTTS: text too short ({text_len} chars, min 3) for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        if text_len > 3000:
            self.ten_env.log_warn(
                f"CambTTS: text too long ({text_len} chars, max 3000), truncating for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            text = text[:3000]

        try:
            # Build payload with Camb.ai's nested structure
            payload = {
                "text": text,
                "voice_id": self.config.params.get("voice_id", 2681),
                "language": self.config.params.get("language", "en-us"),
                "speech_model": self.config.params.get("speech_model", "mars-8-flash"),
                "output_configuration": {
                    "format": self.config.params.get("format", "pcm_s16le"),
                },
                "voice_settings": {
                    "speed": self.config.params.get("speed", 1.0),
                },
            }

            async with self.client.stream(
                "POST",
                self.endpoint,
                headers=self.headers,
                json=payload,
            ) as response:
                # Check for HTTP errors before streaming
                if response.status_code == 401:
                    error_message = "Invalid Camb.ai API key. Set CAMB_API_KEY environment variable with your API key from https://camb.ai"
                    self.ten_env.log_error(
                        f"CambTTS: {error_message} for request_id: {request_id}.",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    yield error_message.encode(
                        "utf-8"
                    ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
                    return

                if response.status_code == 403:
                    voice_id = self.config.params.get("voice_id", 2681)
                    error_message = f"Voice ID {voice_id} is not accessible with your API key. Use list_voices() to see available voices."
                    self.ten_env.log_error(
                        f"CambTTS: {error_message} for request_id: {request_id}.",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    yield error_message.encode(
                        "utf-8"
                    ), TTS2HttpResponseEventType.ERROR
                    return

                if response.status_code == 429:
                    error_message = "Rate limit exceeded. Please wait before making more requests."
                    self.ten_env.log_error(
                        f"CambTTS: {error_message} for request_id: {request_id}.",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    yield error_message.encode(
                        "utf-8"
                    ), TTS2HttpResponseEventType.ERROR
                    return

                if response.status_code >= 400:
                    error_body = await response.aread()
                    error_message = f"API Error {response.status_code}: {error_body.decode('utf-8', errors='replace')}"
                    self.ten_env.log_error(
                        f"CambTTS: {error_message} for request_id: {request_id}.",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    yield error_message.encode(
                        "utf-8"
                    ), TTS2HttpResponseEventType.ERROR
                    return

                async for chunk in response.aiter_bytes(chunk_size=8192):
                    if self._is_cancelled:
                        self.ten_env.log_debug(
                            f"Cancellation flag detected, sending flush event and stopping TTS stream of request_id: {request_id}."
                        )
                        yield None, TTS2HttpResponseEventType.FLUSH
                        break

                    self.ten_env.log_debug(
                        f"CambTTS: sending EVENT_TTS_RESPONSE, length: {len(chunk)} of request_id: {request_id}."
                    )

                    if len(chunk) > 0:
                        yield bytes(chunk), TTS2HttpResponseEventType.RESPONSE
                    else:
                        yield None, TTS2HttpResponseEventType.END

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"CambTTS: sending EVENT_TTS_END of request_id: {request_id}."
                )
                yield None, TTS2HttpResponseEventType.END

        except Exception as e:
            # Check if it's an API key authentication error
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message} of request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            if "401" in error_message:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.ERROR

    async def clean(self):
        # In this new model, most cleanup is handled by the connection object's lifecycle.
        # This can be used for any additional cleanup if needed.
        self.ten_env.log_debug("CambTTS: clean() called.")
        try:
            await self.client.aclose()
        finally:
            pass

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for TTFB metrics."""
        return {
            "voice_id": self.config.params.get("voice_id", 2681),
            "speech_model": self.config.params.get("speech_model", "mars-8-flash"),
        }
