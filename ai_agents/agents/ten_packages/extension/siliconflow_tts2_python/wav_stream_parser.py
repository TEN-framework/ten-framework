#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
/* [INPUT]: 依赖标准库 io/wave，依赖上游 HTTP 字节异步迭代器
 * [OUTPUT]: 对外提供 WavStreamParser，用于把流式 WAV 头剥离为 PCM 数据流
 * [POS]: siliconflow_tts2_python 的格式桥接层，避免在主客户端里塞额外分支
 * [PROTOCOL]: 变更时更新此头部，然后检查 AGENT.md
 */
"""

import io
import wave
from typing import Any, AsyncGenerator, AsyncIterator


class WavStreamParser:
    def __init__(
        self,
        aiter_bytes: AsyncGenerator[bytes, None] | AsyncIterator[bytes],
        initial_buffer_size: int = 4096,
    ) -> None:
        self._stream_iterator = aiter_bytes
        self._initial_buffer_size = initial_buffer_size
        self._format_info: dict[str, Any] = {}
        self._header_parsed = False
        self._first_pcm_chunk: bytes | None = None

    async def _parse_header(self) -> None:
        if self._header_parsed:
            return

        header_buffer = bytearray()
        async for chunk in self._stream_iterator:
            header_buffer.extend(chunk)
            if len(header_buffer) >= self._initial_buffer_size:
                break

        with io.BytesIO(header_buffer) as in_memory_file:
            try:
                with wave.open(in_memory_file, "rb") as wav_reader:
                    self._format_info = {
                        "channels": wav_reader.getnchannels(),
                        "sample_width_bytes": wav_reader.getsampwidth(),
                        "framerate": wav_reader.getframerate(),
                    }
            except wave.Error as exc:
                raise ValueError(
                    f"Failed to parse WAV header: {exc}"
                ) from exc

        data_chunk_start = header_buffer.find(b"data")
        if data_chunk_start == -1:
            raise ValueError("The 'data' chunk was not found in the stream")

        pcm_start_offset = data_chunk_start + 8
        self._first_pcm_chunk = bytes(header_buffer[pcm_start_offset:])
        self._header_parsed = True

    async def get_format_info(self) -> dict[str, Any]:
        if not self._header_parsed:
            await self._parse_header()
        return self._format_info

    async def __aiter__(self) -> AsyncGenerator[bytes, None]:
        if not self._header_parsed:
            await self._parse_header()

        if self._first_pcm_chunk:
            yield self._first_pcm_chunk

        async for chunk in self._stream_iterator:
            yield chunk

