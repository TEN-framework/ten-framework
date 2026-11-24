import asyncio
import base64
import inspect
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
        self._request_stream = None
        self._stream_read_task: asyncio.Task | None = None
        self._stream_lock = asyncio.Lock()

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

            request_task = asyncio.create_task(
                self.client.aio.interactions.create(
                    model=model,
                    store=False,
                    stream=True,
                    automatic_continuation=True,
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
            self._request_task = request_task
            try:
                stream = await request_task
            except asyncio.CancelledError:
                await self._close_completed_request()
                self.ten_env.log_debug("Request cancelled while waiting")
                return
            except Exception:
                if self._request_task is request_task:
                    self._request_task = None
                raise

            if self._request_task is not request_task:
                return
            self._request_task = None

            self._request_stream = stream
            emitted_audio = False
            terminal_status = None
            try:
                while True:
                    try:
                        event = await self._next_stream_event(stream)
                    except StopAsyncIteration:
                        break

                    if self._is_cancelled:
                        self.ten_env.log_debug(
                            "Request cancelled while receiving audio"
                        )
                        return

                    if getattr(event, "event_type", None) == "error":
                        error = getattr(event, "error", None)
                        raise RuntimeError(
                            getattr(error, "message", None)
                            or "Gemini Interactions stream failed"
                        )

                    if (
                        getattr(event, "event_type", None)
                        == "interaction.completed"
                    ):
                        interaction = getattr(event, "interaction", None)
                        status = getattr(interaction, "status", None)
                        terminal_status = getattr(status, "value", status)
                        if str(terminal_status).lower() != (
                            "requires_continuation"
                        ):
                            break

                    audio_data = self._audio_delta_data(event)
                    if not audio_data:
                        continue

                    audio_bytes = base64.b64decode(audio_data)
                    emitted_audio = True
                    self.ten_env.log_debug(
                        f"Received {len(audio_bytes)} bytes of audio data"
                    )
                    yield audio_bytes, TTS2HttpResponseEventType.RESPONSE
            finally:
                await self._release_stream(stream)

            if self._is_cancelled:
                self.ten_env.log_debug("Request cancelled after stream closed")
                return

            if str(terminal_status).lower() != "completed":
                if terminal_status is None:
                    raise RuntimeError(
                        "Gemini Interactions stream ended without completion"
                    )
                raise RuntimeError(
                    "Gemini Interactions stream ended with status "
                    f"'{terminal_status}'"
                )

            if emitted_audio:
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
        request_task = self._request_task
        if request_task:
            if not request_task.done():
                request_task.cancel()
            await asyncio.gather(
                request_task,
                return_exceptions=True,
            )
            await self._close_completed_request()
        if self._request_stream:
            await self._release_stream()

    async def clean(self):
        """Clean up resources"""
        self.ten_env.log_info("Cleaning up Gemini TTS client")
        request_task = self._request_task
        if request_task:
            if not request_task.done():
                request_task.cancel()
            await asyncio.gather(
                request_task,
                return_exceptions=True,
            )
            await self._close_completed_request()
        if self._request_stream:
            await self._release_stream()
        if self.client:
            await self.client.aio.aclose()
        self._is_cancelled = False
        self.client = None

    async def _next_stream_event(self, stream):
        task = asyncio.create_task(anext(stream))
        self._stream_read_task = task
        try:
            return await task
        finally:
            if self._stream_read_task is task and task.done():
                self._stream_read_task = None

    async def _close_completed_request(self) -> None:
        task = self._request_task
        if task is None or not task.done():
            return
        self._request_task = None
        if task.cancelled():
            return
        try:
            stream = task.result()
        except Exception:  # noqa: BLE001 - failed requests have no stream
            return
        await self._close_stream(stream)

    async def _release_stream(self, expected_stream=None) -> None:
        async with self._stream_lock:
            stream = self._request_stream
            if stream is None:
                return
            if expected_stream is not None and stream is not expected_stream:
                return

            self._request_stream = None
            read_task = self._stream_read_task
            if read_task is not None:
                if not read_task.done():
                    read_task.cancel()
                await asyncio.gather(read_task, return_exceptions=True)
                if self._stream_read_task is read_task:
                    self._stream_read_task = None
            await self._close_stream(stream)

    @staticmethod
    def _audio_delta_data(event) -> str | None:
        if getattr(event, "event_type", None) != "step.delta":
            return None
        delta = getattr(event, "delta", None)
        if getattr(delta, "type", None) != "audio":
            return None
        data = getattr(delta, "data", None)
        return data if isinstance(data, str) and data else None

    @staticmethod
    async def _close_stream(stream) -> None:
        close = getattr(stream, "aclose", None) or getattr(
            stream, "close", None
        )
        if close is None:
            return
        result = close()
        if inspect.isawaitable(result):
            await result

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return model/voice for TTFB metrics"""
        return {
            "model": self.config.params.get("model"),
            "voice": self.config.params.get("voice"),
        }
