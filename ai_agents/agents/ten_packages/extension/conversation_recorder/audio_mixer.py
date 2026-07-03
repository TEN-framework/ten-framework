import numpy as np
import threading
from typing import Dict, Deque
from collections import deque


class AudioMixer:
    def __init__(self, sample_rate=24000, channels=1, chunk_duration_ms=40):
        self.sample_rate = sample_rate
        self.channels = channels
        # Calculate chunk size in samples.
        # e.g., 24000Hz * 0.04s = 960 samples
        self.chunk_size = int(self.sample_rate * (chunk_duration_ms / 1000.0))

        # Buffers: map string(stream_id) -> Deque[float] (samples)
        # We use deque for efficient pop from left.
        self.buffers: Dict[str, Deque[float]] = {}
        self.lock = threading.Lock()

    def _resample(
        self, audio: np.ndarray, src_rate: int, dst_rate: int
    ) -> np.ndarray:
        """Resample audio from src_rate to dst_rate using linear interpolation."""
        if src_rate == dst_rate:
            return audio

        # Calculate the resampling ratio
        ratio = dst_rate / src_rate

        # Calculate output length
        output_length = int(len(audio) * ratio)
        if output_length == 0:
            return np.array([], dtype=np.float32)

        # Use linear interpolation for resampling
        x_old = np.linspace(0, 1, len(audio))
        x_new = np.linspace(0, 1, output_length)
        resampled = np.interp(x_new, x_old, audio)

        return resampled.astype(np.float32)

    def push_audio(
        self, source_id: str, pcm_data: bytes, source_sample_rate: int = None
    ):
        """
        Push raw PCM bytes (int16) into the source's buffer.
        If source_sample_rate differs from mixer sample_rate, audio will be resampled.

        Args:
            source_id: Unique identifier for the audio source
            pcm_data: Raw PCM bytes in int16 format
            source_sample_rate: Sample rate of the input audio (defaults to mixer rate if not specified)
        """
        # Convert to float32 for mixing headroom
        audio_array = np.frombuffer(pcm_data, dtype=np.int16).astype(np.float32)

        # Resample if needed
        if (
            source_sample_rate is not None
            and source_sample_rate != self.sample_rate
        ):
            audio_array = self._resample(
                audio_array, source_sample_rate, self.sample_rate
            )

        with self.lock:
            if source_id not in self.buffers:
                self.buffers[source_id] = deque()

            # Extend deque with samples
            self.buffers[source_id].extend(audio_array)

    def mix_next_chunk(self) -> bytes:
        """
        Extracts `chunk_size` samples from all buffers, mixes them, and returns bytes.
        If a buffer has insufficient data, it contributes 0 (silence) for the missing part.
        If ALL buffers are empty, returns empty bytes (indicating no data to write).
        """
        with self.lock:
            # Check if any buffer has data
            has_data = any(len(buf) > 0 for buf in self.buffers.values())
            if not has_data:
                return b""

            # Initialize mixer buffer
            mixed_chunk = np.zeros(self.chunk_size, dtype=np.float32)

            # Mix each source
            for buf in self.buffers.values():
                # Extract up to chunk_size samples
                count = min(len(buf), self.chunk_size)
                if count > 0:
                    # Create temporary array from deque slice
                    # Iterating deque is fast enough for 960 items?
                    # Ideally we'd chunk this better, but for python MVP this is readable.
                    # Optimization: slice deque to list then array.
                    samples = [buf.popleft() for _ in range(count)]
                    samples_arr = np.array(samples, dtype=np.float32)

                    # Add to mix
                    mixed_chunk[:count] += samples_arr

            # Clip and convert to int16
            mixed_chunk = np.clip(mixed_chunk, -32768, 32767)
            return mixed_chunk.astype(np.int16).tobytes()
