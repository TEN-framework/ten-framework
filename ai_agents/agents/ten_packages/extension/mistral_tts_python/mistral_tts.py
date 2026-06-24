"""
Mistral (Voxtral) TTS Client Implementation using httpx.

Mistral exposes an OpenAI-compatible Text-to-Speech endpoint
(`POST {base_url}/audio/speech`, default base_url `https://api.mistral.ai/v1`).
We talk to it with httpx directly (no vendor SDK) so we can stream the audio.

Why WAV (and not raw `pcm`): Mistral's `response_format="pcm"` is documented as
"raw float32 LE samples", which is NOT the PCM16 the TEN `pcm_frame` contract
expects. Instead we request `response_format="wav"` and convert the
self-describing WAV stream to PCM16 mono on the fly (see WavToPcm16), so the
extension is correct whether the vendor's WAV payload is int16, int24/32, or
IEEE float.
"""

from typing import Any, AsyncIterator, Tuple
import json
import ssl
import struct
import certifi
import time

# ============================================================================
# Performance Optimization: Module-level pre-import of httpx
# ============================================================================
import httpcore  # noqa: F401  # pylint: disable=unused-import  # Note: This import cannot be removed, otherwise it will affect http client initialization time
import httpx  # noqa: F401  # pylint: disable=unused-import
from httpx import AsyncClient, Timeout, Limits

from ten_runtime import AsyncTenEnv
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

from .config import MistralTTSConfig


# ============================================================================
# Performance Optimization: Module-level pre-creation of SSL context
# ============================================================================
# Pre-create a global SSL context at module import time so every
# httpx.AsyncClient reuses it instead of re-loading CA certificates.
_GLOBAL_SSL_CONTEXT = ssl.create_default_context(cafile=certifi.where())


# WAV format tags (from the `fmt ` chunk's audioFormat field).
WAVE_FORMAT_PCM = 0x0001
WAVE_FORMAT_IEEE_FLOAT = 0x0003
WAVE_FORMAT_EXTENSIBLE = 0xFFFE

PCM16_MAX = 32767


class WavToPcm16:
    """Streaming converter: WAV bytes in, PCM16 mono little-endian bytes out.

    The vendor sends a WAV container whose `fmt ` chunk declares the real sample
    format. We buffer just enough of the head to parse that chunk, then convert
    every subsequent `data` byte to signed 16-bit mono. Partial input frames are
    held back between calls so we never split a sample.
    """

    def __init__(self) -> None:
        self._header_parsed = False
        self._head = bytearray()
        self._remainder = bytearray()  # leftover bytes < one input frame
        self.audio_format = WAVE_FORMAT_PCM
        self.channels = 1
        self.bits_per_sample = 16
        self.sample_rate = 0

    @property
    def passthrough(self) -> bool:
        """True when the source is already PCM16 mono (no conversion needed)."""
        return (
            self.audio_format == WAVE_FORMAT_PCM
            and self.bits_per_sample == 16
            and self.channels == 1
        )

    def feed(self, chunk: bytes) -> bytes:
        """Feed a chunk of the WAV stream; return PCM16 bytes now available."""
        if not self._header_parsed:
            self._head.extend(chunk)
            if not self._try_parse_header():
                return b""  # still waiting for the full header
            # _try_parse_header set self._remainder to the post-header PCM bytes
            chunk = bytes(self._remainder)
            self._remainder = bytearray()

        return self._convert(chunk)

    def _try_parse_header(self) -> bool:
        buf = self._head
        if len(buf) < 12 or buf[0:4] != b"RIFF" or buf[8:12] != b"WAVE":
            # Not (yet) a recognizable RIFF/WAVE head. Keep buffering up to a
            # sane cap before giving up.
            if len(buf) > 65536:
                raise ValueError("Mistral TTS: stream is not a valid WAV")
            return False

        fmt = buf.find(b"fmt ")
        data = buf.find(b"data")
        if fmt == -1 or data == -1 or len(buf) < data + 8:
            if len(buf) > 65536:
                raise ValueError("Mistral TTS: WAV header not found in stream")
            return False

        # `body` points at the fmt chunk data (after the 8-byte chunk header).
        # Layout: audioFormat(2) numChannels(2) sampleRate(4) byteRate(4)
        #         blockAlign(2) bitsPerSample(2) ...
        body = fmt + 8  # skip "fmt " id (4) + chunk size (4)
        self.audio_format = int.from_bytes(buf[body : body + 2], "little")
        self.channels = max(
            1, int.from_bytes(buf[body + 2 : body + 4], "little")
        )
        self.sample_rate = int.from_bytes(buf[body + 4 : body + 8], "little")
        self.bits_per_sample = int.from_bytes(
            buf[body + 14 : body + 16], "little"
        )

        if self.audio_format == WAVE_FORMAT_EXTENSIBLE:
            # For WAVE_FORMAT_EXTENSIBLE the real format tag is the first 2
            # bytes of the SubFormat GUID at body+24.
            sub = body + 24
            if len(buf) >= sub + 2:
                self.audio_format = int.from_bytes(buf[sub : sub + 2], "little")

        self._remainder = bytearray(buf[data + 8 :])
        self._head = bytearray()
        self._header_parsed = True
        return True

    def _convert(self, chunk: bytes) -> bytes:
        if not chunk:
            return b""

        data = bytes(self._remainder) + chunk
        self._remainder = bytearray()

        if self.passthrough:
            return data

        frame = self.channels * (self.bits_per_sample // 8)
        if frame <= 0:
            raise ValueError(
                f"Mistral TTS: unsupported WAV format "
                f"(format={self.audio_format}, bits={self.bits_per_sample})"
            )

        usable = (len(data) // frame) * frame
        if usable < len(data):
            self._remainder = bytearray(data[usable:])
            data = data[:usable]
        if not data:
            return b""

        return self._frames_to_pcm16(data, frame)

    def _frames_to_pcm16(self, data: bytes, frame: int) -> bytes:
        ch = self.channels
        bits = self.bits_per_sample
        fmt = self.audio_format
        out = bytearray()

        for i in range(0, len(data), frame):
            acc = 0.0
            for c in range(ch):
                off = i + c * (bits // 8)
                acc += self._sample_to_float(data, off, fmt, bits)
            value = acc / ch  # downmix to mono, normalized to [-1, 1]
            s = int(max(-1.0, min(1.0, value)) * PCM16_MAX)
            out += struct.pack("<h", s)
        return bytes(out)

    @staticmethod
    def _sample_to_float(data: bytes, off: int, fmt: int, bits: int) -> float:
        """Decode one sample at `off` into a float in [-1, 1]."""
        if fmt == WAVE_FORMAT_IEEE_FLOAT:
            if bits == 32:
                return struct.unpack_from("<f", data, off)[0]
            if bits == 64:
                return struct.unpack_from("<d", data, off)[0]
        elif fmt == WAVE_FORMAT_PCM:
            if bits == 8:  # unsigned
                return (data[off] - 128) / 128.0
            if bits == 16:
                return struct.unpack_from("<h", data, off)[0] / 32768.0
            if bits == 24:
                b = data[off : off + 3]
                val = b[0] | (b[1] << 8) | (b[2] << 16)
                if val & 0x800000:
                    val -= 0x1000000
                return val / 8388608.0
            if bits == 32:
                return struct.unpack_from("<i", data, off)[0] / 2147483648.0
        raise ValueError(
            f"Mistral TTS: unsupported WAV sample (format={fmt}, bits={bits})"
        )


class MistralTTSClient(AsyncTTS2HttpClient):
    """
    Mistral (Voxtral) TTS Client using httpx.

    Features:
    - OpenAI-compatible `/v1/audio/speech` request shape
    - Parameter passthrough (all params except api_key and base_url)
    - WAV -> PCM16 mono conversion (handles int and IEEE-float WAV)
    - Comprehensive error handling and cancellation support
    """

    def __init__(
        self,
        config: MistralTTSConfig,
        ten_env: AsyncTenEnv,
    ):
        super().__init__()
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self._is_cancelled = False

        # Build headers - merge user-provided headers with defaults
        api_key = self.config.params.get("api_key", "")
        default_headers = {
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        }
        # Merge: user headers override defaults
        self.headers = {**default_headers, **self.config.headers}

        # Create httpx client reusing the module-level SSL context.
        _start_time = time.time()
        self.client = AsyncClient(
            timeout=Timeout(timeout=60.0),  # TTS may take longer
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,
            ),
            http2=True,
            verify=_GLOBAL_SSL_CONTEXT,
        )
        _elapsed_ms = (time.time() - _start_time) * 1000
        ten_env.log_debug(f"http client initialized in {_elapsed_ms:.2f}ms")

        ten_env.log_info(
            f"MistralTTS initialized with endpoint: {self.config.url}"
        )

    async def cancel(self):
        """Cancel the current TTS request."""
        self.ten_env.log_debug("MistralTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        """
        Process a single TTS request.

        Yields:
            Tuple of (audio_bytes, event_type):
            - (bytes, RESPONSE): PCM16 mono audio chunk
            - (None, END): Successful completion
            - (None, FLUSH): Cancelled
            - (bytes, ERROR): Error message
            - (bytes, INVALID_KEY_ERROR): Authentication error
        """
        self._is_cancelled = False

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"MistralTTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        try:
            # Build request payload - pass through all params
            # (except api_key and base_url)
            payload = {**self.config.params}
            payload.pop("api_key", None)  # api_key is sent via the header
            payload.pop("base_url", None)  # base_url was folded into the url

            # Set input to the text to be synthesized
            payload["input"] = text

            self.ten_env.log_debug(
                f"MistralTTS: sending request for request_id: {request_id}"
            )

            converter = WavToPcm16()

            # Send streaming request
            async with self.client.stream(
                "POST", self.config.url, headers=self.headers, json=payload
            ) as response:
                if self._is_cancelled:
                    self.ten_env.log_debug(
                        f"Cancellation detected before processing response for request_id: {request_id}"
                    )
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return

                # Handle non-200 status code
                if response.status_code != 200:
                    error_body = await response.aread()
                    try:
                        error_data = json.loads(error_body)
                        error_info = error_data.get("error", error_data)
                        if isinstance(error_info, dict):
                            error_msg = error_info.get(
                                "message", str(error_data)
                            )
                            error_code = error_info.get("code")
                        else:
                            error_msg = str(error_info)
                            error_code = None
                    except Exception:
                        error_msg = error_body.decode("utf-8", errors="replace")
                        error_code = None

                    self.ten_env.log_error(
                        f"vendor_error: HTTP {response.status_code}: {error_msg} for request_id: {request_id}",
                        category=LOG_CATEGORY_VENDOR,
                    )

                    # 401/403 -> auth/permission (Mistral also returns 403 when
                    # the input is rejected by content moderation).
                    if (
                        response.status_code in (401, 403)
                        or error_code == "invalid_api_key"
                    ):
                        yield error_msg.encode(
                            "utf-8"
                        ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
                    else:
                        yield error_msg.encode(
                            "utf-8"
                        ), TTS2HttpResponseEventType.ERROR
                    return

                # Stream audio data, converting WAV -> PCM16 mono on the fly.
                async for chunk in response.aiter_bytes():
                    if self._is_cancelled:
                        self.ten_env.log_debug(
                            f"Cancellation detected, flushing TTS stream for request_id: {request_id}"
                        )
                        yield None, TTS2HttpResponseEventType.FLUSH
                        break

                    pcm = converter.feed(chunk)
                    if pcm:
                        yield pcm, TTS2HttpResponseEventType.RESPONSE

                # Send END event
                if not self._is_cancelled:
                    self.ten_env.log_debug(
                        f"MistralTTS: sending END event for request_id: {request_id}"
                    )
                    yield None, TTS2HttpResponseEventType.END

        except Exception as e:
            error_message = str(e)
            self.ten_env.log_error(
                f"vendor_error: {error_message} for request_id: {request_id}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Check if it's an authentication error
            if (
                "401" in error_message
                or "403" in error_message
                or "invalid_api_key" in error_message
            ):
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                # All other errors are treated as general errors
                # (including network errors: ConnectionRefusedError, TimeoutError, etc.)
                yield error_message.encode(
                    "utf-8"
                ), TTS2HttpResponseEventType.ERROR

    async def clean(self):
        """Clean up resources."""
        self.ten_env.log_debug("MistralTTS: clean() called.")
        try:
            if self.client:
                await self.client.aclose()
        finally:
            pass

    def get_extra_metadata(self) -> dict[str, Any]:
        """Return extra metadata for TTFB metrics."""
        return {
            "model": self.config.params.get("model", ""),
            "voice": self.config.params.get(
                "voice_id", self.config.params.get("voice", "")
            ),
        }
