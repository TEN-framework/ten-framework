import asyncio
import base64
from collections.abc import AsyncIterator
from typing import Any

from google import genai
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient
from ten_runtime import AsyncTenEnv

from .config import DEFAULT_MODEL, GeminiTTSConfig


class GeminiTTSClient(AsyncTTS2HttpClient):
    def __init__(
        self,
        config: GeminiTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config: GeminiTTSConfig = config
        self.ten_env: AsyncTenEnv = ten_env
        self.client = None
        self._is_cancelled = False
        self._request_task: asyncio.Task | None = None

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[tuple[bytes | None, TTS2HttpResponseEventType]]:
        """Generate TTS audio for the given text using Gemini API"""

        self._is_cancelled = False

        try:
            # Initialize Gemini client
            if not self.client:
                api_key = self.config.params.get("api_key")
                self.client = genai.Client(
                    api_key=api_key,
                    http_options={"api_version": "v1beta"},
                )
                self.ten_env.log_debug("Gemini TTS client initialized")

            # Prepare request parameters
            model = self.config.params.get("model", DEFAULT_MODEL)
            voice = self.config.params.get("voice", "Kore")
            language_code = self.config.params.get("language_code", "en-US")
            style_prompt = self.config.params.get("style_prompt", "")

            self.ten_env.log_debug(
                f"Generating speech for text: '{text[:100]}...' "
                f"(model: {model}, voice: {voice}, "
                f"language: {language_code})"
            )

            # Prepare contents
            text_content: dict[str, Any] = {"type": "text", "text": text}
            if style_prompt:
                text_content["annotations"] = [
                    {
                        "type": "speech_metadata",
                        "style": style_prompt,
                    }
                ]

            self._request_task = asyncio.create_task(
                self.client.aio.interactions.create(
                    model=model,
                    input=[
                        {
                            "type": "user_input",
                            "content": [text_content],
                        }
                    ],
                    response_format={
                        "type": "audio",
                        "mime_type": "audio/l16",
                        "sample_rate": 24000,
                    },
                    generation_config={
                        "speech_config": [
                            {"voice": voice, "language": language_code}
                        ]
                    },
                )
            )
            try:
                response = await self._request_task
            except asyncio.CancelledError:
                self.ten_env.log_debug("Request cancelled while waiting")
                return
            finally:
                self._request_task = None

            output_audio = getattr(response, "output_audio", None)
            audio_data = getattr(output_audio, "data", None)
            if audio_data:
                audio_bytes = base64.b64decode(audio_data)

                if self._is_cancelled:
                    self.ten_env.log_debug(
                        "Request cancelled after receiving response"
                    )
                    return

                self.ten_env.log_debug(
                    f"Received {len(audio_bytes)} bytes of audio data"
                )

                yield audio_bytes, TTS2HttpResponseEventType.RESPONSE
                yield None, TTS2HttpResponseEventType.END
            else:
                error_msg = "No audio data in response from Gemini TTS"
                self.ten_env.log_error(error_msg)
                yield error_msg.encode("utf-8"), TTS2HttpResponseEventType.ERROR

        except Exception as e:  # noqa: BLE001 - classify all provider failures
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: reason: {error_message}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Check if it's an authentication error
            if (
                "401" in error_message
                or "403" in error_message
                or "authentication" in error_message.lower()
                or "api_key" in error_message.lower()
                or "unauthorized" in error_message.lower()
            ):
                yield (
                    error_message.encode("utf-8"),
                    TTS2HttpResponseEventType.INVALID_KEY_ERROR,
                )
            else:
                yield (
                    error_message.encode("utf-8"),
                    TTS2HttpResponseEventType.ERROR,
                )

    async def cancel(self):
        """Cancel ongoing request"""
        self.ten_env.log_info("Cancelling Gemini TTS request")
        self._is_cancelled = True
        if self._request_task and not self._request_task.done():
            self._request_task.cancel()
            await asyncio.gather(
                self._request_task,
                return_exceptions=True,
            )

    async def clean(self):
        """Clean up resources"""
        self.ten_env.log_info("Cleaning up Gemini TTS client")
        self._is_cancelled = False
        self.client = None

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return model/voice for TTFB metrics"""
        return {
            "model": self.config.params.get("model"),
            "voice": self.config.params.get("voice"),
        }
