import time
from typing import AsyncIterator

# Only import the specific TTS modules we need to avoid PortAudio dependency
from fish_audio_sdk import AsyncWebSocketSession, TTSRequest
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from .config import FishAudioTTSConfig

# Custom event types to communicate status back to the extension
EVENT_TTS_RESPONSE = 1
EVENT_TTS_END = 2
EVENT_TTS_ERROR = 3
EVENT_TTS_INVALID_KEY_ERROR = 4
EVENT_TTS_FLUSH = 5


class FishAudioTTSClient:
    def __init__(self, config: FishAudioTTSConfig, ten_env: AsyncTenEnv):
        self.config = config
        self.ten_env = ten_env
        self.client = AsyncWebSocketSession(config.api_key)
        self._is_cancelled = False

    async def _text_stream(self, text: str) -> AsyncIterator[str]:
        yield text

    async def get(self, text: str) -> AsyncIterator[tuple[bytes | None, int]]:
        """Process a single TTS request in serial manner"""
        self._is_cancelled = False
        if not self.client:
            return

        tts_request = TTSRequest(
            text="", chunk_length=200, **self.config.params
        )

        start_time = time.time()

        gen: AsyncIterator[bytes] | None = None

        try:
            gen = self.client.tts(
                request=tts_request,
                text_stream=self._text_stream(text),
                backend=self.config.backend,
            )
            async for chunk in gen:
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        "Cancellation flag detected, sending flush event and stopping TTS stream."
                    )
                    yield None, EVENT_TTS_FLUSH
                    await gen.aclose()
                    gen = None
                    return

                self.ten_env.log_debug(
                    f"FishAudioTTS: sending EVENT_TTS_RESPONSE, length: {len(chunk)}"
                )
                if len(chunk) > 0:
                    yield chunk, EVENT_TTS_RESPONSE

                # Only send EVENT_TTS_END if not cancelled (flush event already sent)

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"FishAudioTTS: sending EVENT_TTS_END, total time: {time.time() - start_time}"
                )
                yield None, EVENT_TTS_END

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Check if it's an API key authentication error
            if (
                "402" in error_message and "Payment Required" in error_message
            ) or ("Payment Required" in error_message):
                yield error_message.encode("utf-8"), EVENT_TTS_INVALID_KEY_ERROR
            else:
                yield error_message.encode("utf-8"), EVENT_TTS_ERROR
        finally:
            if gen is not None:
                try:
                    await gen.aclose()
                except Exception as close_error:
                    self.ten_env.log_warn(
                        f"FishAudioTTS: failed to close generator cleanly: {close_error}"
                    )

    def cancel(self):
        self.ten_env.log_debug("FishAudioTTS: cancel() called.")
        self._is_cancelled = True

    def clean(self):
        # In this new model, most cleanup is handled by the connection object's lifecycle.
        # This can be used for any additional cleanup if needed.
        self.ten_env.log_debug("FishAudioTTS: clean() called.")
