from typing import Any, AsyncIterator, Tuple
from groq import (
    AsyncGroq,
    APIConnectionError,
    APITimeoutError,
    RateLimitError,
    InternalServerError,
)
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

from .config import GroqTTSConfig
from .utils import with_retry_context
from .wav_stream_parser import WavStreamParser


class GroqTTSClient(AsyncTTS2HttpClient):
    def __init__(
        self,
        config: GroqTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.client: AsyncGroq | None = None

        # Retry configuration
        self.max_retries = 3
        self.retry_delay = 0.1

        try:
            self.client = AsyncGroq(api_key=config.params["api_key"])
        except Exception as e:
            ten_env.log_error(
                f"error when initializing GroqTTS: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(f"error when initializing GroqTTS: {e}") from e

    async def cancel(self):
        self.ten_env.log_debug("GroqTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """Process a single TTS request"""
        self._is_cancelled = False
        if not self.client:
            self.ten_env.log_error(
                f"GroqTTS: client not initialized for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"GroqTTS: client not initialized for request_id: {request_id}."
            )

        if len(text.strip()) == 0:
            self.ten_env.log_warning(
                f"GroqTTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        try:
            # Use retry mechanism for robust connection
            async for chunk in self._synthesize_with_retry(text, request_id):
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        f"Cancellation flag detected, sending flush event and stopping TTS stream of request_id: {request_id}."
                    )
                    yield None, TTS2HttpResponseEventType.FLUSH
                    break

                self.ten_env.log_debug(
                    f"GroqTTS: sending EVENT_TTS_RESPONSE, length: {len(chunk)} of request_id: {request_id}."
                )

                if len(chunk) > 0:
                    yield bytes(chunk), TTS2HttpResponseEventType.RESPONSE

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"GroqTTS: sending EVENT_TTS_END of request_id: {request_id}."
                )
                yield None, TTS2HttpResponseEventType.END

        except Exception as e:
            # Check if it's an API key authentication error
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message} of request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            if (
                "401" in error_message
                or "authentication" in error_message.lower()
            ):
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.ERROR

    async def _synthesize(self, text: str) -> AsyncIterator[bytes]:
        """Internal method to synthesize audio from text"""
        assert self.client is not None

        # Build request params from config.params dict
        request_params = {
            "model": self.config.params.get("model", "playai-tts"),
            "voice": self.config.params["voice"],
            "response_format": self.config.params.get("response_format", "wav"),
        }
        if "sample_rate" in self.config.params:
            request_params["sample_rate"] = self.config.params["sample_rate"]
        if "speed" in self.config.params:
            request_params["speed"] = self.config.params["speed"]

        response = self.client.with_streaming_response.audio.speech.create(
            input=text, **request_params
        )
        async with response as stream:
            stream_parser = WavStreamParser(stream.iter_bytes())
            _format_info = await stream_parser.get_format_info()
            async for chunk in stream_parser:
                yield chunk

    async def _synthesize_with_retry(
        self, text: str, request_id: str  # pylint: disable=unused-argument
    ) -> AsyncIterator[bytes]:
        """Synthesize with retry logic"""
        assert self.client is not None

        response = with_retry_context(
            max_retries=self.max_retries,
            retry_delay=self.retry_delay,
            backoff_factor=2.0,
            exceptions=(
                APIConnectionError,
                APITimeoutError,
                RateLimitError,
                InternalServerError,
            ),
        )(self._synthesize)(text)

        async for chunk in response:
            yield chunk

    async def clean(self):
        """Clean up resources"""
        self.ten_env.log_debug("GroqTTS: clean() called.")
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
