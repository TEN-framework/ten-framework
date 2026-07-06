#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any, AsyncIterator, Tuple

from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient
from ten_runtime import AsyncTenEnv
from typecast import (
    AsyncTypecast,
    PaymentRequiredError,
    RateLimitError,
    TypecastError,
    UnauthorizedError,
)
from typecast.models import TTSRequestStream

from .config import TypecastTTSConfig
from .pcm import StreamingWavToPcm16

TYPECAST_STREAM_SAMPLE_RATE = 32000


class TypecastTTSClient(AsyncTTS2HttpClient):
    """Typecast TTS client backed by the official Typecast Python SDK."""

    def __init__(self, config: TypecastTTSConfig, ten_env: AsyncTenEnv):
        super().__init__()
        self.config = config
        self.ten_env = ten_env
        self._is_cancelled = False
        self._client = AsyncTypecast(
            host=self.config.host,
            api_key=self.config.params["api_key"],
        )
        self._client_entered = False

        ten_env.log_info(
            f"TypecastTTS initialized with host: {self.config.host}"
        )

    async def _ensure_client(self) -> AsyncTypecast:
        if not self._client_entered:
            await self._client.__aenter__()
            self._client_entered = True
        return self._client

    async def cancel(self):
        self.ten_env.log_debug("TypecastTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        self._is_cancelled = False

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"TypecastTTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        try:
            payload = {**self.config.params}
            payload.pop("api_key", None)

            output = dict(payload.get("output") or {})
            output["audio_format"] = "wav"
            payload["output"] = output
            payload["text"] = text

            request = TTSRequestStream.model_validate(payload)
            converter = StreamingWavToPcm16()
            client = await self._ensure_client()

            self.ten_env.log_debug(
                f"TypecastTTS: sending request for request_id: {request_id}"
            )

            async for chunk in client.text_to_speech_stream(
                request, chunk_size=self.config.chunk_size
            ):
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        f"Cancellation detected, flushing TTS stream for request_id: {request_id}"
                    )
                    yield None, TTS2HttpResponseEventType.FLUSH
                    break

                pcm = converter.feed(chunk)
                if pcm:
                    yield pcm, TTS2HttpResponseEventType.RESPONSE

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"TypecastTTS: sending END event for request_id: {request_id}"
                )
                yield None, TTS2HttpResponseEventType.END

        except UnauthorizedError as e:
            yield self._error_bytes(e, request_id), (
                TTS2HttpResponseEventType.INVALID_KEY_ERROR
            )
        except (
            PaymentRequiredError,
            RateLimitError,
            TypecastError,
            ValueError,
        ) as e:
            yield self._error_bytes(
                e, request_id
            ), TTS2HttpResponseEventType.ERROR
        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message} for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )
            if "401" in error_message or "403" in error_message:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.ERROR

    def _error_bytes(self, error: Exception, request_id: str) -> bytes:
        error_message = str(error)
        self.ten_env.log_error(
            f"vendor_error: {error_message} for request_id: {request_id}",
            category=LOG_CATEGORY_VENDOR,
        )
        return error_message.encode("utf-8")

    async def clean(self):
        self.ten_env.log_debug("TypecastTTS: clean() called.")
        if self._client_entered:
            await self._client.__aexit__(None, None, None)
            self._client_entered = False

    def get_extra_metadata(self) -> dict[str, Any]:
        return {
            "vendor": "typecast",
            "model": self.config.params.get("model", ""),
            "voice_id": self.config.params.get("voice_id", ""),
        }
