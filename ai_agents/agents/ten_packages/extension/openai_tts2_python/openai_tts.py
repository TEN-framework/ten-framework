from typing import Any, AsyncIterator, Tuple
from openai import AsyncOpenAI

from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

from .config import OpenAITTSConfig


BYTES_PER_SAMPLE = 2
NUMBER_OF_CHANNELS = 1


class OpenAITTSClient(AsyncTTS2HttpClient):
    def __init__(
        self,
        config: OpenAITTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.client: AsyncOpenAI | None = None

        try:
            self.client = AsyncOpenAI(
                api_key=config.params["api_key"],
            )
        except Exception as e:
            ten_env.log_error(
                f"error when initializing OpenAITTS: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(f"error when initializing OpenAITTS: {e}") from e

    async def cancel(self):
        self.ten_env.log_debug("OpenAITTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """Process a single TTS request"""
        self._is_cancelled = False
        if not self.client:
            self.ten_env.log_error(
                f"OpenAITTS: client not initialized for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"OpenAITTS: client not initialized for request_id: {request_id}."
            )

        if len(text.strip()) == 0:
            self.ten_env.log_warning(
                f"OpenAITTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        try:
            async with self.client.audio.speech.with_streaming_response.create(
                input=text, **self.config.params
            ) as response:
                cache_audio_bytes = bytearray()
                async for chunk in response.iter_bytes():
                    if self._is_cancelled:
                        self.ten_env.log_debug(
                            f"Cancellation flag detected, sending flush event and stopping TTS stream of request_id: {request_id}."
                        )
                        yield None, TTS2HttpResponseEventType.FLUSH
                        break

                    self.ten_env.log_debug(
                        f"OpenAITTS: sending EVENT_TTS_RESPONSE, length: {len(chunk)} of request_id: {request_id}."
                    )
                    if len(cache_audio_bytes) > 0:
                        chunk = cache_audio_bytes + chunk
                        cache_audio_bytes = bytearray()

                    left_size = len(chunk) % (
                        BYTES_PER_SAMPLE * NUMBER_OF_CHANNELS
                    )

                    if left_size > 0:
                        cache_audio_bytes = chunk[-left_size:]
                        chunk = chunk[:-left_size]

                    if len(chunk) > 0:
                        yield bytes(chunk), TTS2HttpResponseEventType.RESPONSE

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"OpenAITTS: sending EVENT_TTS_END of request_id: {request_id}."
                )
                yield None, TTS2HttpResponseEventType.END

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message} of request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )

            # Check if it's an API key authentication error
            if (
                "401" in error_message and "invalid_api_key" in error_message
            ) or ("invalid_api_key" in error_message):
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.ERROR

    async def clean(self):
        """Clean up resources"""
        self.ten_env.log_debug("OpenAITTS: clean() called.")
        try:
            if self.client:
                await self.client.close()
        finally:
            pass

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for TTFB metrics."""
        return {
            "model": self.config.params.get("model", ""),
            "voice": self.config.params.get("voice", ""),
        }
