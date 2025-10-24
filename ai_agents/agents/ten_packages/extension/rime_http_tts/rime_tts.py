from typing import Any, AsyncIterator, Tuple
from httpx import AsyncClient, Timeout, Limits

from .config import RimeTTSConfig
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient


BYTES_PER_SAMPLE = 2
NUMBER_OF_CHANNELS = 1


class RimeTTSClient(AsyncTTS2HttpClient):
    def __init__(
        self,
        config: RimeTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.api_key = config.api_key
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.endpoint = "https://users.rime.ai/v1/rime-tts"
        self.headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json",
            "Accept": "audio/pcm",
        }
        self.client = AsyncClient(
            timeout=Timeout(timeout=20.0),
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,  # 10 minutes keepalive
            ),
            http2=True,  # Enable HTTP/2 if server supports it
        )

    async def cancel(self):
        self.ten_env.log_debug("RimeTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """Process a single TTS request in serial manner"""
        self._is_cancelled = False
        if not self.client:
            self.ten_env.log_error(
                f"RimeTTS: client not initialized for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"RimeTTS: client not initialized for request_id: {request_id}."
            )

        try:
            async with self.client.stream(
                "POST",
                self.endpoint,
                headers=self.headers,
                json={
                    "text": text,
                    **self.config.params,
                },
            ) as response:
                async for chunk in response.aiter_bytes(chunk_size=4096):
                    if self._is_cancelled:
                        self.ten_env.log_debug(
                            f"Cancellation flag detected, sending flush event and stopping TTS stream of request_id: {request_id}."
                        )
                        yield None, TTS2HttpResponseEventType.FLUSH
                        break

                    self.ten_env.log_debug(
                        f"RimeTTS: sending EVENT_TTS_RESPONSE, length: {len(chunk)} of request_id: {request_id}."
                    )

                    if len(chunk) > 0:
                        yield bytes(chunk), TTS2HttpResponseEventType.RESPONSE
                    else:
                        yield None, TTS2HttpResponseEventType.END

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"RimeTTS: sending EVENT_TTS_END of request_id: {request_id}."
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
        self.ten_env.log_debug("RimeTTS: clean() called.")
        try:
            await self.client.aclose()
        finally:
            pass

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for TTFB metrics."""
        return {
            "speaker": self.config.params.get("speaker", ""),
            "modelId": self.config.params.get("modelId", ""),
        }
