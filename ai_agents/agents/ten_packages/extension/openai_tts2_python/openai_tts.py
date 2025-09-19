from typing import AsyncIterator
from openai import AsyncOpenAI

from .config import OpenaiTTSConfig
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


class OpenaiTTSClient:
    def __init__(
        self,
        config: OpenaiTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        self.config = config
        self.api_key = config.api_key
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.client = AsyncOpenAI(
            api_key=self.config.api_key,
        )

    async def stop(self):
        # Stop the client if it exists
        if self.client:
            await self.client.close()
            self.client = None

    def cancel(self):
        self.ten_env.log_debug("OpenaiTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[bytes | None, int | None]]:
        """Process a single TTS request in serial manner"""
        self._is_cancelled = False
        if not self.client:
            return

        try:
            async with self.client.audio.speech.with_streaming_response.create(
                input=text, **self.config.params
            ) as response:
                cache_audio_bytes = bytearray()
                async for chunk in response.iter_bytes():
                    if self._is_cancelled:
                        self.ten_env.log_debug(
                            "Cancellation flag detected, sending flush event and stopping TTS stream."
                        )
                        yield None, EVENT_TTS_FLUSH
                        break

                    self.ten_env.log_debug(
                        f"OpenaiTTS: sending EVENT_TTS_RESPONSE, length: {len(chunk)}"
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
                        yield bytes(chunk), EVENT_TTS_RESPONSE
                    else:
                        yield None, EVENT_TTS_END

            if not self._is_cancelled:
                self.ten_env.log_debug("OpenaiTTS: sending EVENT_TTS_END")
                yield None, EVENT_TTS_END

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Check if it's an API key authentication error
            if (
                "401" in error_message and "invalid_api_key" in error_message
            ) or ("invalid_api_key" in error_message):
                yield error_message.encode("utf-8"), EVENT_TTS_INVALID_KEY_ERROR
            else:
                yield error_message.encode("utf-8"), EVENT_TTS_ERROR

    def clean(self):
        # In this new model, most cleanup is handled by the connection object's lifecycle.
        # This can be used for any additional cleanup if needed.
        self.ten_env.log_debug("OpenaiTTS: clean() called.")
