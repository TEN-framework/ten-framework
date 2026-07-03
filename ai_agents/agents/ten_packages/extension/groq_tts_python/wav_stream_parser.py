import wave
import io
from typing import AsyncGenerator, AsyncIterator, Dict, Any


class WavStreamParser:
    """
    A async parser for WAV stream data.

    It first buffers and parses the WAV header, then streams the raw PCM data.
    """

    def __init__(
        self,
        aiter_bytes: AsyncGenerator[bytes, None] | AsyncIterator[bytes],
        initial_buffer_size: int = 4096,
    ):
        """
        Initialize the parser.

        Args:
            aiter_bytes: An async generator of bytes.
            initial_buffer_size: The initial buffer size for finding and parsing the header.
                                 For most WAV files, 4KB is more than enough.
        """
        self._stream_iterator = aiter_bytes
        self._initial_buffer_size = initial_buffer_size
        self._format_info: Dict[str, Any] = {}
        self._header_parsed = False
        self._pcm_start_offset = 0
        self._first_pcm_chunk = None

    async def _parse_header(self) -> None:
        """
        Read data from the beginning of the stream until the 'fmt ' and 'data' chunks are found and parsed.
        """
        if self._header_parsed:
            return

        # 1. Buffer the initial data block, which should be enough to contain the entire header
        header_buffer = bytearray()
        async for chunk in self._stream_iterator:
            header_buffer.extend(chunk)
            if len(header_buffer) >= self._initial_buffer_size:
                break

        # 2. Open this buffer in memory, like a file
        with io.BytesIO(header_buffer) as in_memory_file:
            try:
                with wave.open(in_memory_file, "rb") as wav_reader:
                    # 3. Use the wave module to extract the format information
                    self._format_info = {
                        "channels": wav_reader.getnchannels(),
                        "sample_width_bytes": wav_reader.getsampwidth(),
                        "framerate": wav_reader.getframerate(),
                        "bits_per_sample": wav_reader.getsampwidth() * 8,
                    }
            except wave.Error as e:
                raise ValueError(
                    f"Failed to parse WAV header: {e}. "
                    f"Ensure the stream is a valid WAV format."
                ) from e

        # 4. Find the start of the 'data' chunk, so we know where the PCM data starts
        # The header of the 'data' chunk contains the 4-byte ID 'data' and the 4-byte size
        data_chunk_start = header_buffer.find(b"data")
        if data_chunk_start == -1:
            raise ValueError("The 'data' chunk was not found in the stream.")

        # PCM data starts after the 'data' identifier and the 4-byte length field
        self._pcm_start_offset = data_chunk_start + 8

        # 5. Save the remaining PCM data after the header in the buffer
        self._first_pcm_chunk = bytes(header_buffer[self._pcm_start_offset :])
        self._header_parsed = True

    async def get_format_info(self) -> Dict[str, Any]:
        """
        Return the format information of the WAV file. If needed, the header will be parsed first.
        """
        if not self._header_parsed:
            await self._parse_header()
        return self._format_info

    async def __aiter__(self) -> AsyncGenerator[bytes, None]:
        """
        Make this class an async iterator, to yield raw PCM data blocks.
        """
        if not self._header_parsed:
            await self._parse_header()

        # 1. First yield the first PCM data block parsed from the initial buffer
        if self._first_pcm_chunk:
            yield self._first_pcm_chunk

        # 2. Then continue to yield the remaining data blocks from the HTTP stream
        async for chunk in self._stream_iterator:
            yield chunk
