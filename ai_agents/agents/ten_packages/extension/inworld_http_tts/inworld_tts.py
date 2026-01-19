import base64
import json
from typing import Any, AsyncIterator, Tuple
from httpx import AsyncClient, Timeout, Limits

from .config import InworldTTSConfig
from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient


class InworldTTSClient(AsyncTTS2HttpClient):
    def __init__(
        self,
        config: InworldTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.api_key = config.params.get("api_key", "")
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False
        self.endpoint = config.params.get(
            "endpoint", "https://api.inworld.ai/tts/v1/voice:stream"
        )
        # Inworld uses Basic auth instead of Bearer
        self.headers = {
            "Authorization": f"Basic {self.api_key}",
            "Content-Type": "application/json",
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
        # Flag to track if RIFF header has been stripped for current request
        self._header_stripped = False

    async def cancel(self):
        self.ten_env.log_debug("InworldTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """Process a single TTS request in serial manner"""
        self._is_cancelled = False
        # Reset header stripped flag for each new request
        self._header_stripped = False

        if not self.client:
            self.ten_env.log_error(
                f"InworldTTS: client not initialized for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            raise RuntimeError(
                f"InworldTTS: client not initialized for request_id: {request_id}."
            )

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"InworldTTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        try:
            # Build Inworld request body with correct parameter names
            payload = {
                "text": text,
                "voiceId": self.config.params.get("voice", ""),
                "modelId": self.config.params.get("modelId", "inworld-tts-1"),
                "audioConfig": {
                    "sampleRateHertz": self.config.params.get("sampleRate", 16000),
                    "audioEncoding": "LINEAR16",
                },
            }

            # Add optional params to audioConfig if present
            if "speakingRate" in self.config.params:
                payload["audioConfig"]["speakingRate"] = self.config.params[
                    "speakingRate"
                ]

            self.ten_env.log_debug(
                f"InworldTTS: sending request to {self.endpoint} for request_id: {request_id}."
            )

            async with self.client.stream(
                "POST",
                self.endpoint,
                headers=self.headers,
                json=payload,
            ) as response:
                # Log response status for debugging
                self.ten_env.log_info(
                    f"InworldTTS: HTTP {response.status_code} from {self.endpoint} for request_id: {request_id}."
                )
                if response.status_code != 200:
                    error_body = await response.aread()
                    self.ten_env.log_error(
                        f"InworldTTS: Error response: {error_body.decode('utf-8', errors='ignore')} for request_id: {request_id}."
                    )
                    yield error_body, TTS2HttpResponseEventType.ERROR
                    return

                # Inworld returns newline-delimited JSON objects for longer text
                # Each line is a separate JSON object with result.audioContent
                response_text = await response.aread()
                response_str = response_text.decode("utf-8")

                if self._is_cancelled:
                    self.ten_env.log_debug(
                        f"Cancellation flag detected, sending flush event for request_id: {request_id}."
                    )
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return

                # Parse each line as a separate JSON object
                all_audio_data = bytearray()
                for line in response_str.split("\n"):
                    line = line.strip()
                    if not line:
                        continue

                    try:
                        data = json.loads(line)
                        # Extract audioContent from result object
                        audio_content_b64 = data.get("result", {}).get("audioContent")
                        if audio_content_b64:
                            # Base64 decode the audio content
                            chunk_data = base64.b64decode(audio_content_b64)

                            # Strip 44-byte RIFF/WAV header if present (only first chunk)
                            if chunk_data[:4] == b"RIFF":
                                if not self._header_stripped:
                                    self.ten_env.log_debug(
                                        f"InworldTTS: stripping 44-byte RIFF header for request_id: {request_id}."
                                    )
                                    self._header_stripped = True
                                chunk_data = chunk_data[44:]

                            all_audio_data.extend(chunk_data)

                    except json.JSONDecodeError as e:
                        self.ten_env.log_warn(
                            f"InworldTTS: failed to parse JSON line: {e} for request_id: {request_id}.",
                            category=LOG_CATEGORY_VENDOR,
                        )
                        continue

                if len(all_audio_data) > 0:
                    self.ten_env.log_info(
                        f"InworldTTS: sending {len(all_audio_data)} bytes PCM audio for request_id: {request_id}."
                    )
                    yield bytes(all_audio_data), TTS2HttpResponseEventType.RESPONSE
                else:
                    self.ten_env.log_warn(
                        f"InworldTTS: no audio data extracted for request_id: {request_id}.",
                        category=LOG_CATEGORY_VENDOR,
                    )

            if not self._is_cancelled:
                self.ten_env.log_debug(
                    f"InworldTTS: sending EVENT_TTS_END of request_id: {request_id}."
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
                yield error_message.encode("utf-8"), TTS2HttpResponseEventType.ERROR

    async def clean(self):
        """Clean up resources."""
        self.ten_env.log_debug("InworldTTS: clean() called.")
        await self.client.aclose()

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for TTFB metrics."""
        return {
            "voice": self.config.params.get("voice", ""),
            "sampleRate": self.config.params.get("sampleRate", 16000),
        }
