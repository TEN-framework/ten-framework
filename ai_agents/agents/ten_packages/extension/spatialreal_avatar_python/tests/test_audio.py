#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Unit tests for the audio helpers: resampling and silence padding.

Resampling keeps ``audioop.ratecv`` state across chunks so a request that
arrives in 20 ms pieces converts like one that arrives whole; a trailing odd
byte is dropped instead of raising; silence is generated in whole samples.
"""

import struct

import pytest

from spatialreal_avatar_python.audio import (
    ACCEPTED_RATES,
    Resampler,
    duration_ms,
    silence,
)


def _tone(rate: int, ms: int, step: int = 7) -> bytes:
    """Deterministic non-silent int16 PCM (a sawtooth) of ``ms`` at ``rate``."""
    frames = rate * ms // 1000
    return b"".join(
        struct.pack("<h", ((i * step) % 2000) - 1000) for i in range(frames)
    )


def test_accepted_rates():
    assert ACCEPTED_RATES == (8000, 16000, 22050, 24000, 32000, 44100, 48000)


def test_same_rate_passthrough():
    pcm = _tone(24000, 20)
    assert Resampler(24000, 24000).convert(pcm) == pcm


def test_odd_byte_dropped_and_no_raise():
    pcm = _tone(24000, 20) + b"\x01"
    # Same rate: the odd byte goes, the rest passes through.
    assert Resampler(24000, 24000).convert(pcm) == pcm[:-1]
    # Different rate: ratecv would raise on an odd length; we must not.
    out = Resampler(16000, 24000).convert(_tone(16000, 20) + b"\x01")
    assert len(out) % 2 == 0
    # A lone odd byte converts to nothing.
    assert Resampler(16000, 24000).convert(b"\x01") == b""


def test_empty_input_returns_empty():
    assert Resampler(24000, 24000).convert(b"") == b""
    assert Resampler(16000, 24000).convert(b"") == b""


def test_resample_16k_to_24k_length_ratio():
    out = Resampler(16000, 24000).convert(_tone(16000, 1000))
    frames = len(out) // 2
    assert abs(frames - 24000) <= 2


def test_state_carries_across_chunks():
    src = _tone(16000, 200)
    whole = Resampler(16000, 24000).convert(src)

    chunked = Resampler(16000, 24000)
    chunk_bytes = 16000 * 20 // 1000 * 2
    pieces = [src[i : i + chunk_bytes] for i in range(0, len(src), chunk_bytes)]
    assert len(pieces) == 10
    joined = b"".join(chunked.convert(p) for p in pieces)

    assert abs(len(joined) // 2 - len(whole) // 2) <= 4
    # Carrying state means the chunked output tracks the whole-buffer
    # output sample for sample well after the first chunk; without state
    # every chunk boundary would restart the filter and the two diverge.
    common = min(len(joined), len(whole)) // 2 * 2
    diffs = [
        abs(a - b)
        for a, b in zip(
            struct.unpack(f"<{common // 2}h", joined[:common]),
            struct.unpack(f"<{common // 2}h", whole[:common]),
        )
    ]
    assert max(diffs[1000:]) <= 2


def test_reset_restarts_state():
    r = Resampler(16000, 24000)
    first = r.convert(_tone(16000, 20))
    r.reset()
    again = r.convert(_tone(16000, 20))
    assert again == first


def test_silence_sample_count():
    out = silence(100, 24000)
    assert len(out) == 4800
    assert out == b"\x00" * 4800
    assert len(silence(100, 44100)) == 4410 * 2
    assert silence(0, 24000) == b""


def test_silence_rejects_fractional():
    # 22050 * 150 / 1000 = 3307.5 frames: not whole samples.
    with pytest.raises(ValueError):
        silence(150, 22050)
    # 44100 * 1 / 1000 = 44.1 frames.
    with pytest.raises(ValueError):
        silence(1, 44100)
    # 8000 * 1 / 1000 = 8 frames: whole, so this one is accepted.
    assert len(silence(1, 8000)) == 16


def test_duration_ms():
    assert duration_ms(silence(100, 24000), 24000) == 100.0
    assert duration_ms(b"", 24000) == 0.0
    assert duration_ms(_tone(16000, 20), 16000) == 20.0
    # An odd trailing byte is not a sample.
    assert duration_ms(b"\x00" * 3, 1000) == 1.0
