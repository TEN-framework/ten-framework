from typing import AsyncIterator
from httpx import AsyncClient, Timeout, Limits

from .config import RimeTTSConfig
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR

# Custom event types to communicate status back to the extension
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_INVALID_KEY_ERROR = 4
EVENT_TTS_FLUSH = 5


BYTES_PER_SAMPLE = 2
NUMBER_OF_CHANNELS = 1


class RimeTTSClient:
    def __init__(
        self,
        config: RimeTTSConfig,
        ten_env: AsyncTenEnv,
    ):
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
            timeout=Timeout(timeout=5.0),
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,  # 10 minutes keepalive
            ),
            http2=True,  # Enable HTTP/2 if server supports it
            follow_redirects=True,
        )

    async def stop(self):
        # Stop the client if it exists
        if self.client:
            self.ten_env.log_debug("stop the client")
            self.client = None

    def cancel(self):
        self.ten_env.log_debug("RimeTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | None, int | None]]:
        """Process a single TTS request in serial manner"""
        self._is_cancelled = False
        if not self.client:
            return

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
                            "Cancellation flag detected, sending flush event and stopping TTS stream."
                        )
                        yield None, EVENT_TTS_FLUSH
                        break

                    self.ten_env.log_debug(
                        f"RimeTTS: sending EVENT_TTS_RESPONSE, length: {len(chunk)}"
                    )

                    if len(chunk) > 0:
                        yield bytes(chunk), EVENT_TTS_RESPONSE
                    else:
                        yield None, EVENT_TTS_END

            if not self._is_cancelled:
                self.ten_env.log_debug("RimeTTS: sending EVENT_TTS_END")
                yield None, EVENT_TTS_END

        except Exception as e:
            # Check if it's an API key authentication error
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message}",
                category=LOG_CATEGORY_VENDOR,
            )
            if "401" in error_message:
                yield error_message.encode("utf-8"), EVENT_TTS_INVALID_KEY_ERROR
            else:
                yield error_message.encode("utf-8"), EVENT_TTS_ERROR

    def clean(self):
        # In this new model, most cleanup is handled by the connection object's lifecycle.
        # This can be used for any additional cleanup if needed.
        self.ten_env.log_debug("RimeTTS: clean() called.")
