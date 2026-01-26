"""
OpenAI TTS Client Implementation using httpx

This implementation replaces the OpenAI SDK with httpx for better compatibility
with third-party TTS servers while maintaining full backward compatibility.
"""

from typing import Any, AsyncIterator, Tuple
import json
from httpx import AsyncClient, Timeout, Limits

from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

from .config import OpenAITTSConfig


BYTES_PER_SAMPLE = 2
NUMBER_OF_CHANNELS = 1


class OpenAITTSClient(AsyncTTS2HttpClient):
    """
    OpenAI TTS Client using httpx.

    Features:
    - Full OpenAI TTS API compatibility
    - Support for third-party TTS servers via base_url
    - Parameter passthrough (all params except api_key and base_url)
    - Comprehensive error handling
    - Audio frame alignment
    - Cancellation support
    """

    def __init__(
        self,
        config: OpenAITTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False

        # Set endpoint URL
        if config.url:
            self.endpoint = config.url
        else:
            base_url = config.params.get(
                "base_url", "https://api.openai.com/v1"
            )
            # Remove trailing slash
            base_url = base_url.rstrip("/")
            self.endpoint = f"{base_url}/audio/speech"

        # Build headers
        api_key = config.params.get("api_key", "")
        self.headers = {
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        }

        # Create httpx client
        self.client = AsyncClient(
            timeout=Timeout(timeout=60.0),  # TTS may take longer
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,
            ),
            http2=True,
        )

        ten_env.log_info(
            f"OpenAITTS initialized with endpoint: {self.endpoint}"
        )

    async def cancel(self):
        """Cancel the current TTS request."""
        self.ten_env.log_debug("OpenAITTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """
        Process a single TTS request.

        Args:
            text: Text to synthesize
            request_id: Unique request identifier for logging

        Yields:
            Tuple of (audio_bytes, event_type):
            - (bytes, RESPONSE): Audio chunk
            - (None, END): Successful completion
            - (None, FLUSH): Cancelled
            - (bytes, ERROR): Error message
            - (bytes, INVALID_KEY_ERROR): Authentication error
        """
        self._is_cancelled = False

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"OpenAITTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        try:
            # Build request payload - pass through all params (except api_key and base_url)
            payload = {**self.config.params}
            payload.pop("api_key", None)  # Remove api_key from headers
            payload.pop("base_url", None)  # Remove base_url from payload

            # Set input to the text to be synthesized
            payload["input"] = text

            self.ten_env.log_debug(
                f"OpenAITTS: sending request for request_id: {request_id}"
            )

            # Send streaming request
            async with self.client.stream(
                "POST", self.endpoint, headers=self.headers, json=payload
            ) as response:
                # Check cancellation flag
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        f"Cancellation detected before processing response for request_id: {request_id}"
                    )
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return

                # Handle non-200 status code
                if response.status_code != 200:
                    error_body = await response.aread()
                    try:
                        error_data = json.loads(error_body)
                        error_info = error_data.get("error", {})
                        error_msg = error_info.get("message", str(error_data))
                        error_code = error_info.get("code")
                    except Exception:
                        error_msg = error_body.decode("utf-8", errors="replace")
                        error_code = None

                    self.ten_env.log_error(
                        f"vendor_error: HTTP {response.status_code}: {error_msg} for request_id: {request_id}",
                        category=LOG_CATEGORY_VENDOR,
                    )

                    # Classify by status code and error code
                    if (
                        response.status_code in (401, 403)
                        or error_code == "invalid_api_key"
                    ):
                        yield error_msg.encode(
                            "utf-8"
                        ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
                    else:
                        yield error_msg.encode(
                            "utf-8"
                        ), TTS2HttpResponseEventType.ERROR
                    return

                # Stream audio data
                cache_audio_bytes = bytearray()
                async for chunk in response.aiter_bytes():
                    if self._is_cancelled:
                        self.ten_env.log_debug(
                            f"Cancellation detected, flushing TTS stream for request_id: {request_id}"
                        )
                        yield None, TTS2HttpResponseEventType.FLUSH
                        break

                    self.ten_env.log_debug(
                        f"OpenAITTS: received chunk, length: {len(chunk)} for request_id: {request_id}"
                    )

                    # Process audio alignment (ensure it's a complete audio frame)
                    # This is important for PCM format, ensure each chunk is a complete sample point
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

                # Send END event
                if not self._is_cancelled:
                    self.ten_env.log_debug(
                        f"OpenAITTS: sending END event for request_id: {request_id}"
                    )
                    yield None, TTS2HttpResponseEventType.END

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message} for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Check if it's an authentication error
            if (
                "401" in error_message
                or "403" in error_message
                or "invalid_api_key" in error_message
            ):
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                # All other errors are treated as general errors (NON_FATAL_ERROR)
                # Including network connection errors (ConnectionRefusedError, TimeoutError, etc.)
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.ERROR

    async def clean(self):
        """Clean up resources."""
        self.ten_env.log_debug("OpenAITTS: clean() called.")
        try:
            if self.client:
                await self.client.aclose()
        finally:
            pass

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for TTFB metrics."""
        return {
            "model": self.config.params.get("model", ""),
            "voice": self.config.params.get("voice", ""),
        }
