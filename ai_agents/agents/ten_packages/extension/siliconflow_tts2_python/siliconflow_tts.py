#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
/* [INPUT]: 依赖 httpx 的流式 HTTP 客户端，依赖 config.py 与 wav_stream_parser.py
 * [OUTPUT]: 对外提供 SiliconFlowTTSClient，向 TEN 输出 PCM 音频块和错误事件
 * [POS]: siliconflow_tts2_python 的核心供应商适配器，负责请求 SiliconFlow /audio/speech
 * [PROTOCOL]: 变更时更新此头部，然后检查 AGENT.md
 */
"""

from typing import Any, AsyncIterator, Tuple

from httpx import AsyncClient, Limits, Timeout
import miniaudio

from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient
from ten_runtime import AsyncTenEnv

from .config import SiliconFlowTTSConfig
from .wav_stream_parser import WavStreamParser


BYTES_PER_SAMPLE = 2
NUMBER_OF_CHANNELS = 1
PCM_CHUNK_SIZE = 4096


class SiliconFlowTTSClient(AsyncTTS2HttpClient):
    def __init__(self, config: SiliconFlowTTSConfig, ten_env: AsyncTenEnv):
        super().__init__()
        self.config = config
        self.ten_env = ten_env
        self._is_cancelled = False
        base_url = str(self.config.params.get("base_url", "")).rstrip("/")
        self.endpoint = f"{base_url}/audio/speech"
        self.headers = {
            "Authorization": f"Bearer {self.config.params['api_key']}",
            "Content-Type": "application/json",
        }
        self.client = AsyncClient(
            timeout=Timeout(timeout=60.0, connect=10.0),
            limits=Limits(
                max_connections=100,
                max_keepalive_connections=20,
                keepalive_expiry=600.0,
            ),
            http2=True,
        )

    async def cancel(self) -> None:
        self.ten_env.log_debug("SiliconFlowTTS: cancel() called.")
        self._is_cancelled = True

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        self._is_cancelled = False

        if len(text.strip()) == 0:
            self.ten_env.log_warn(
                f"SiliconFlowTTS: empty text for request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            yield None, TTS2HttpResponseEventType.END
            return

        payload = {**self.config.params}
        payload.pop("api_key", None)
        payload.pop("base_url", None)
        payload["input"] = text
        payload["stream"] = True

        try:
            async with self.client.stream(
                "POST",
                self.endpoint,
                headers=self.headers,
                json=payload,
            ) as response:
                if response.status_code != 200:
                    error_message = (
                        f"HTTP {response.status_code}: {await response.aread()}"
                    )
                    self.ten_env.log_error(
                        f"vendor_error: {error_message} of request_id: {request_id}.",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    if response.status_code in (401, 403):
                        yield error_message.encode("utf-8"), (
                            TTS2HttpResponseEventType.INVALID_KEY_ERROR
                        )
                    else:
                        yield error_message.encode("utf-8"), (
                            TTS2HttpResponseEventType.ERROR
                        )
                    return

                content_type = response.headers.get("content-type", "").lower()
                sniffed_format = await self._sniff_response_format(
                    response.aiter_bytes()
                )
                self.ten_env.log_info(
                    "SiliconFlowTTS: "
                    f"request_id={request_id}, "
                    f"requested_format={payload.get('response_format', 'mp3')}, "
                    f"content_type={content_type}, "
                    f"sniffed_format={sniffed_format}",
                    category=LOG_CATEGORY_VENDOR,
                )

                if sniffed_format == "mpeg":
                    async for chunk in self._iter_mpeg_stream(
                        request_id=request_id
                    ):
                        if chunk is None:
                            yield None, TTS2HttpResponseEventType.FLUSH
                            return
                        yield chunk, TTS2HttpResponseEventType.RESPONSE
                elif sniffed_format == "wav":
                    async for chunk in self._iter_wav_stream(
                        request_id=request_id
                    ):
                        if chunk is None:
                            yield None, TTS2HttpResponseEventType.FLUSH
                            return
                        yield chunk, TTS2HttpResponseEventType.RESPONSE
                else:
                    async for chunk in self._iter_pcm_stream(
                        request_id=request_id
                    ):
                        if chunk is None:
                            yield None, TTS2HttpResponseEventType.FLUSH
                            return
                        yield chunk, TTS2HttpResponseEventType.RESPONSE

            if not self._is_cancelled:
                yield None, TTS2HttpResponseEventType.END

        except Exception as exc:
            error_message = str(exc)
            self.ten_env.log_error(
                f"vendor_error: {error_message} of request_id: {request_id}.",
                category=LOG_CATEGORY_VENDOR,
            )
            if "401" in error_message or "403" in error_message:
                yield error_message.encode("utf-8"), (
                    TTS2HttpResponseEventType.INVALID_KEY_ERROR
                )
            else:
                yield error_message.encode("utf-8"), (
                    TTS2HttpResponseEventType.ERROR
                )

    async def _sniff_response_format(
        self, byte_stream: AsyncIterator[bytes]
    ) -> str:
        self._stream_iterator = byte_stream
        self._prefetched_chunk = b""

        async for chunk in self._stream_iterator:
            if chunk:
                self._prefetched_chunk = chunk
                break

        prefix = self._prefetched_chunk[:4]
        if prefix == b"RIFF":
            return "wav"
        if prefix[:3] == b"ID3" or prefix[:2] == b"\xff\xfb":
            return "mpeg"
        return "pcm"

    async def _iter_wav_stream(
        self, request_id: str
    ) -> AsyncIterator[bytes | None]:
        stream_parser = WavStreamParser(self._iter_prefetched_stream())
        format_info = await stream_parser.get_format_info()
        self.config.sample_rate = int(
            format_info.get("framerate", self.config.sample_rate)
        )

        channels = int(format_info.get("channels", NUMBER_OF_CHANNELS))
        sample_width = int(
            format_info.get("sample_width_bytes", BYTES_PER_SAMPLE)
        )
        if channels != NUMBER_OF_CHANNELS or sample_width != BYTES_PER_SAMPLE:
            raise ValueError(
                "SiliconFlow WAV stream must be mono 16-bit PCM compatible, "
                f"got channels={channels}, sample_width={sample_width}"
            )

        async for chunk in stream_parser:
            if self._is_cancelled:
                self.ten_env.log_debug(
                    "Cancellation flag detected, stopping SiliconFlow WAV stream "
                    f"of request_id: {request_id}."
                )
                yield None
                return

            if len(chunk) > 0:
                yield chunk

    async def _iter_mpeg_stream(
        self, request_id: str
    ) -> AsyncIterator[bytes | None]:
        audio_bytes = bytearray()
        async for chunk in self._iter_prefetched_stream():
            if self._is_cancelled:
                self.ten_env.log_debug(
                    "Cancellation flag detected before MPEG decode, stopping "
                    f"SiliconFlow stream of request_id: {request_id}."
                )
                yield None
                return

            audio_bytes.extend(chunk)

        try:
            decoded = miniaudio.decode(
                bytes(audio_bytes),
                output_format=miniaudio.SampleFormat.SIGNED16,
                nchannels=NUMBER_OF_CHANNELS,
                sample_rate=self.config.sample_rate,
            )
        except Exception as exc:
            raise ValueError(f"Failed to decode MPEG audio: {exc}") from exc

        pcm_bytes = decoded.samples.tobytes()
        for offset in range(0, len(pcm_bytes), PCM_CHUNK_SIZE):
            if self._is_cancelled:
                self.ten_env.log_debug(
                    "Cancellation flag detected during MPEG playback, stopping "
                    f"SiliconFlow stream of request_id: {request_id}."
                )
                yield None
                return

            chunk = pcm_bytes[offset : offset + PCM_CHUNK_SIZE]
            if chunk:
                yield chunk

    async def _iter_pcm_stream(
        self, request_id: str
    ) -> AsyncIterator[bytes | None]:
        cache_audio_bytes = bytearray()
        async for chunk in self._iter_prefetched_stream():
            if self._is_cancelled:
                self.ten_env.log_debug(
                    "Cancellation flag detected, stopping SiliconFlow PCM stream "
                    f"of request_id: {request_id}."
                )
                yield None
                return

            if len(cache_audio_bytes) > 0:
                chunk = bytes(cache_audio_bytes) + chunk
                cache_audio_bytes = bytearray()

            left_size = len(chunk) % (BYTES_PER_SAMPLE * NUMBER_OF_CHANNELS)
            if left_size > 0:
                cache_audio_bytes = bytearray(chunk[-left_size:])
                chunk = chunk[:-left_size]

            if len(chunk) > 0:
                yield chunk

    async def _iter_prefetched_stream(self) -> AsyncIterator[bytes]:
        if self._prefetched_chunk:
            yield self._prefetched_chunk
            self._prefetched_chunk = b""

        async for chunk in self._stream_iterator:
            yield chunk

    async def clean(self) -> None:
        self.ten_env.log_debug("SiliconFlowTTS: clean() called.")
        await self.client.aclose()

    def get_extra_metadata(self) -> dict[str, Any]:
        return {
            "model": self.config.params.get("model", ""),
            "voice": self.config.params.get("voice", ""),
        }
