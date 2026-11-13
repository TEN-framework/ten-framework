#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""PCM helpers: resampling to the session rate and silence generation.

Pure: no TEN or SpatialReal imports. Everything is 16-bit little-endian
mono, the only format the graph's TTS extensions emit and the only one
``SessionConfig.sample_rate`` describes.

Frames arrive at ``AudioFrame.get_sample_rate()`` and are converted to the
session rate with ``audioop.ratecv``; the filter state is kept across chunks
of one request and reset at close or flush so a request that arrives in
20 ms pieces converts like one that arrives whole. A trailing odd byte (half
a sample) is dropped first: ``ratecv`` raises on it, and a raise in the
frame handler would take the whole extension down.

Silence is generated in whole samples at the session rate so the virtual
playhead's ``sent_ms`` stays exact.
"""

import audioop  # pylint: disable=deprecated-module

ACCEPTED_RATES: tuple[int, ...] = (
    8000,
    16000,
    22050,
    24000,
    32000,
    44100,
    48000,
)

SAMPLE_WIDTH = 2
CHANNELS = 1


class Resampler:
    """Streaming int16 mono resampler from ``src_rate`` to ``dst_rate``.

    ``convert`` may be called once per chunk; ``reset`` forgets the filter
    state so the next chunk is treated as the start of a new request.
    """

    def __init__(self, src_rate: int, dst_rate: int):
        if src_rate <= 0 or dst_rate <= 0:
            raise ValueError("sample rates must be positive")
        self.src_rate = src_rate
        self.dst_rate = dst_rate
        self._state = None

    def convert(self, pcm: bytes) -> bytes:
        """Return ``pcm`` resampled to ``dst_rate``; never raises on length."""
        if len(pcm) % SAMPLE_WIDTH:
            pcm = pcm[: len(pcm) - len(pcm) % SAMPLE_WIDTH]
        if self.src_rate == self.dst_rate or not pcm:
            return pcm
        out, self._state = audioop.ratecv(
            pcm,
            SAMPLE_WIDTH,
            CHANNELS,
            self.src_rate,
            self.dst_rate,
            self._state,
        )
        return out

    def reset(self) -> None:
        """Forget the filter state (request closed or flushed)."""
        self._state = None


def silence(ms: int, rate: int) -> bytes:
    """Return ``ms`` of int16 zero at ``rate``; must be whole samples."""
    if rate * ms % 1000:
        raise ValueError(
            f"{ms} ms at {rate} Hz is not a whole number of samples"
        )
    return bytes(rate * ms // 1000 * SAMPLE_WIDTH)


def duration_ms(pcm: bytes, rate: int) -> float:
    """Playout time of ``pcm`` at ``rate``; an odd trailing byte is ignored."""
    return len(pcm) // SAMPLE_WIDTH * 1000.0 / rate
