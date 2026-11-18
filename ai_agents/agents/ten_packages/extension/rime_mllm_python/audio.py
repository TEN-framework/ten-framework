"""Bounded input buffering for a continuous 16 kHz PCM stream."""

from .config import INPUT_FRAME_BYTES, INPUT_SAMPLE_RATE, SAMPLE_WIDTH


class AudioBufferOverflow(ValueError):
    """Live input fell too far behind to preserve conversation timing."""


class InputAudio:
    """A short live-audio queue, padded with silence at the sender's clock."""

    def __init__(self):
        self.buffer = bytearray()

    def append(self, audio: bytes) -> None:
        """Reject partial PCM samples and excessive input latency."""
        if len(audio) % SAMPLE_WIDTH:
            raise ValueError("input must contain whole PCM16 samples")
        if len(self.buffer) + len(audio) > INPUT_SAMPLE_RATE * SAMPLE_WIDTH:
            self.buffer.clear()
            raise AudioBufferOverflow("input audio exceeded one second of buffering")
        self.buffer.extend(audio)

    def frame(self) -> bytes:
        """Consume one 20 ms frame, filling missing live samples with zeros."""
        result = bytes(self.buffer[:INPUT_FRAME_BYTES])
        del self.buffer[:INPUT_FRAME_BYTES]
        return result.ljust(INPUT_FRAME_BYTES, b"\0")
