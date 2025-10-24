import base64
from typing import Any, AsyncIterator, Tuple

# Only import the specific TTS modules we need to avoid PortAudio dependency
from hume import AsyncHumeClient
from hume.tts import (
    FormatPcm,
    PostedContextWithUtterances,
    PostedUtterance,
    PostedUtteranceVoiceWithId,
    PostedUtteranceVoiceWithName,
)
from ten_runtime import AsyncTenEnv
from .config import HumeAiTTSConfig
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient


class HumeAiTTS(AsyncTTS2HttpClient):
    config: HumeAiTTSConfig
    ten_env: AsyncTenEnv
    connection: AsyncHumeClient
    _is_cancelled: bool

    def __init__(self, config: HumeAiTTSConfig, ten_env: AsyncTenEnv):
        self.config = config
        self.ten_env = ten_env
        self.connection = AsyncHumeClient(api_key=config.key)
        self._is_cancelled = False

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """
        Get audio stream from Hume AI TTS.

        Args:
            text: The text to synthesize
            request_id: Optional request ID for backward compatibility with tests
        """
        self._is_cancelled = False

        self.ten_env.log_debug(
            f"send_text_to_tts_server: {text} of request_id: {request_id}",
            category=LOG_CATEGORY_VENDOR,
        )

        voice = None
        if self.config.voice_name:
            voice = PostedUtteranceVoiceWithName(
                name=self.config.voice_name, provider=self.config.provider
            )
        elif self.config.voice_id:
            voice = PostedUtteranceVoiceWithId(
                id=self.config.voice_id, provider=self.config.provider
            )

        try:
            self.ten_env.log_debug(
                f"vendor_status: starting TTS streaming for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Check cancellation before starting the stream
            if self._is_cancelled:
                self.ten_env.log_info(
                    "Request was cancelled before streaming started."
                )
                yield None, TTS2HttpResponseEventType.FLUSH
                return

            async for snippet in self.connection.tts.synthesize_json_streaming(
                context=PostedContextWithUtterances(
                    utterances=[
                        PostedUtterance(
                            text="How can people see beauty so differently?",
                            description="A curious student with a clear and respectful tone, seeking clarification on Hume's ideas with a straightforward question.",
                        )
                    ],
                ),
                utterances=[
                    PostedUtterance(
                        text=text,
                        voice=voice,
                        speed=self.config.speed,
                        trailing_silence=self.config.trailing_silence,
                    )
                ],
                format=FormatPcm(type="pcm"),
                instant_mode=True,
            ):
                # Check cancellation immediately upon receiving each snippet
                if self._is_cancelled:
                    self.ten_env.log_info(
                        "Cancellation flag detected, sending flush event and stopping TTS stream."
                    )
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return

                audio_bytes = base64.b64decode(snippet.audio)

                # Final check before yielding audio data
                if self._is_cancelled:
                    self.ten_env.log_info(
                        "Cancellation detected after decoding audio, discarding data."
                    )
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return

                yield audio_bytes, TTS2HttpResponseEventType.RESPONSE

                if snippet.is_last_chunk:
                    break

            # Only send EVENT_TTS_END if not cancelled
            if not self._is_cancelled:
                yield None, TTS2HttpResponseEventType.END
            else:
                self.ten_env.log_info(
                    "TTS stream was cancelled, not sending END event."
                )

            self.ten_env.log_debug(
                f"vendor_status: TTS streaming finished for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message}", category=LOG_CATEGORY_VENDOR
            )

            # Check if it's an API key authentication error
            if (
                ("401" in error_message and "Invalid ApiKey" in error_message)
                or ("Invalid ApiKey" in error_message)
                or ("oauth.v2.InvalidApiKey" in error_message)
            ):
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.ERROR

    async def cancel(self) -> None:
        """Cancel the current TTS request."""
        self.ten_env.log_debug("HumeAiTTS: cancel() called.")
        self._is_cancelled = True

    async def clean(self) -> None:
        """Clean up resources."""
        # In this new model, most cleanup is handled by the connection object's lifecycle.
        # This can be used for any additional cleanup if needed.
        self.ten_env.log_debug("HumeAiTTS: clean() called.")

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for metrics."""
        return {
            "voice_id": self.config.voice_id if self.config.voice_id else "",
            "voice_name": (
                self.config.voice_name if self.config.voice_name else ""
            ),
        }
