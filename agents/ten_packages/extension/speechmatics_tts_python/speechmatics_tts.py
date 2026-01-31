#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any, AsyncIterator, Tuple
import asyncio
import aiohttp
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

from .config import SpeechmaticsTTSConfig


class SpeechmaticsTTSClient(AsyncTTS2HttpClient):
    """Speechmatics TTS Client implementation"""

    def __init__(
        self,
        config: SpeechmaticsTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.session: aiohttp.ClientSession | None = None

        # Retry configuration
        self.max_retries = 3
        self.retry_delay = 0.1

        try:
            # Create aiohttp session
            self.session = aiohttp.ClientSession()

            self.ten_env.log_info(
                f"Speechmatics TTS client initialized with voice: {config.params.get('voice_id')}",
                category=LOG_CATEGORY_VENDOR,
            )
        except Exception as e:
            ten_env.log_error(
                f"Error when initializing Speechmatics TTS: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(f"Error when initializing Speechmatics TTS: {e}") from e

    async def cancel(self):
        """Cancel the current TTS request"""
        self.ten_env.log_debug("Speechmatics TTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """Process a single TTS request"""
        self._is_cancelled = False

        if not self.session:
            self.ten_env.log_error(
                f"Speechmatics TTS: session not initialized for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"Speechmatics TTS: session not initialized for request_id: {request_id}"
            )

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"Speechmatics TTS: empty text for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        try:
            # Synthesize audio
            async for chunk in self._synthesize_with_retry(text, request_id):
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        f"Cancellation detected, sending flush event for request_id: {request_id}"
                    )
                    yield None, TTS2HttpResponseEventType.FLUSH
                    break

                self.ten_env.log_debug(
                    f"Speechmatics TTS: sending audio chunk, length: {len(chunk)}, request_id: {request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )

                if len(chunk) > 0:
                    yield bytes(chunk), TTS2HttpResponseEventType.RESPONSE

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"Speechmatics TTS: synthesis completed for request_id: {request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )
                yield None, TTS2HttpResponseEventType.END

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"Speechmatics TTS error: {error_message}, request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Check for authentication errors
            if "401" in error_message or "authentication" in error_message.lower():
                yield error_message.encode("utf-8"), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                yield error_message.encode("utf-8"), TTS2HttpResponseEventType.ERROR

    async def _synthesize(self, text: str) -> AsyncIterator[bytes]:
        """Internal method to synthesize audio from text"""
        assert self.session is not None

        # Build API endpoint
        voice_id = self.config.params["voice_id"]
        output_format = self.config.params.get("output_format", "wav")
        base_url = self.config.params.get("base_url", "https://preview.tts.speechmatics.com")

        url = f"{base_url}/generate/{voice_id}"
        if output_format:
            url += f"?output_format={output_format}"

        # Prepare request
        headers = {
            "Authorization": f"Bearer {self.config.params['api_key']}",
            "Content-Type": "application/json",
        }

        payload = {"text": text}

        self.ten_env.log_debug(
            f"Speechmatics TTS: requesting synthesis, voice: {voice_id}, format: {output_format}",
            category=LOG_CATEGORY_VENDOR,
        )

        # Make HTTP request
        async with self.session.post(url, headers=headers, json=payload) as response:
            if response.status != 200:
                error_text = await response.text()
                raise RuntimeError(
                    f"Speechmatics TTS API error: {response.status} - {error_text}"
                )

            # Stream response chunks
            async for chunk in response.content.iter_chunked(4096):
                if chunk:
                    yield chunk

    async def _synthesize_with_retry(
        self, text: str, request_id: str
    ) -> AsyncIterator[bytes]:
        """Synthesize with retry logic"""
        retries = 0
        last_error = None

        while retries <= self.max_retries:
            try:
                async for chunk in self._synthesize(text):
                    yield chunk
                return  # Success, exit retry loop
            except Exception as e:
                last_error = e
                retries += 1

                if retries <= self.max_retries:
                    self.ten_env.log_warn(
                        f"Speechmatics TTS: retry {retries}/{self.max_retries} after error: {e}",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    await asyncio.sleep(self.retry_delay * (2 ** (retries - 1)))
                else:
                    raise last_error

    async def clean(self):
        """Clean up resources"""
        self.ten_env.log_debug("Speechmatics TTS: clean() called.")
        try:
            if self.session:
                await self.session.close()
        finally:
            pass

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for TTFB metrics."""
        return {
            "voice_id": self.config.params.get("voice_id", ""),
            "output_format": self.config.params.get("output_format", "wav"),
        }
