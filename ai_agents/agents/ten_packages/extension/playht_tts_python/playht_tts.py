from typing import Any, AsyncIterator, Tuple
from pyht.client import TTSOptions, Language, Format
from pyht import AsyncClient

from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

from .config import PlayHTTTSConfig


class PlayHTTTSClient(AsyncTTS2HttpClient):
    def __init__(
        self,
        config: PlayHTTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.client: AsyncClient | None = None

        try:
            self.client = AsyncClient(
                api_key=config.params["api_key"],
                user_id=config.params["user_id"],
            )
        except Exception as e:
            ten_env.log_error(
                f"error when initializing PlayHTTTS: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(f"error when initializing PlayHTTTS: {e}") from e

    async def cancel(self):
        self.ten_env.log_debug("PlayHTTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """Process a single TTS request"""
        self._is_cancelled = False
        if not self.client:
            self.ten_env.log_error(
                f"PlayHTTTS: client not initialized for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"PlayHTTTS: client not initialized for request_id: {request_id}."
            )

        if len(text.strip()) == 0:
            self.ten_env.log_warning(
                f"PlayHTTTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        try:
            # Build TTSOptions from params
            tts_options_dict = {}

            # Add language if present
            if "language" in self.config.params:
                language_value = self.config.params["language"]
                if isinstance(language_value, str) and hasattr(
                    Language, language_value
                ):
                    tts_options_dict["language"] = getattr(
                        Language, language_value
                    )
                elif isinstance(language_value, Language):
                    tts_options_dict["language"] = language_value

            # Add format if present
            if "format" in self.config.params:
                format_value = self.config.params["format"]
                if isinstance(format_value, str) and hasattr(
                    Format, format_value
                ):
                    tts_options_dict["format"] = getattr(Format, format_value)
                elif isinstance(format_value, Format):
                    tts_options_dict["format"] = format_value
            else:
                tts_options_dict["format"] = Format.FORMAT_PCM

            # Add sample_rate if present
            if "sample_rate" in self.config.params:
                tts_options_dict["sample_rate"] = self.config.params[
                    "sample_rate"
                ]
            else:
                tts_options_dict["sample_rate"] = 16000

            # Add any other extra params (like voice, speed, etc.)
            excluded_keys = {
                "api_key",
                "user_id",
                "voice_engine",
                "protocol",
                "language",
                "format",
                "sample_rate",
            }
            for key, value in self.config.params.items():
                if key not in excluded_keys and value is not None:
                    tts_options_dict[key] = value

            tts_options = TTSOptions(**tts_options_dict)

            async for chunk in self.client.tts(
                text,
                tts_options,
                voice_engine=self.config.params.get("voice_engine"),
                protocol=self.config.params.get("protocol", "ws"),
            ):
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        f"Cancellation flag detected, sending flush event and stopping TTS stream of request_id: {request_id}."
                    )
                    yield None, TTS2HttpResponseEventType.FLUSH
                    break

                self.ten_env.log_debug(
                    f"PlayHTTTS: sending EVENT_TTS_RESPONSE, length: {len(chunk)} of request_id: {request_id}."
                )

                if len(chunk) > 0:
                    yield bytes(chunk), TTS2HttpResponseEventType.RESPONSE

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"PlayHTTTS: sending EVENT_TTS_END of request_id: {request_id}."
                )
                yield None, TTS2HttpResponseEventType.END

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message} of request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )

            # Check for authentication errors
            if (
                "401" in error_message
                or "unauthorized" in error_message.lower()
                or "authentication" in error_message.lower()
            ):
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.ERROR

    async def clean(self):
        """Clean up resources"""
        self.ten_env.log_debug("PlayHTTTS: clean() called.")
        try:
            # Release client - PlayHT SDK has known issues with cleanup
            # but we can safely set to None
            self.client = None
        except Exception:
            pass

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for TTFB metrics."""
        return {
            "voice_engine": self.config.params.get("voice_engine", ""),
            "voice": self.config.params.get("voice", ""),
        }
