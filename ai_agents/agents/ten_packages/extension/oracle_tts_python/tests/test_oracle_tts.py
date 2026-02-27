import struct

import pytest

from oracle_tts import OracleTTS


class TestStripWavHeader:
    """Tests for OracleTTS._strip_wav_header static method."""

    @staticmethod
    def _make_wav(pcm: bytes, *, fmt_extra: bytes = b"", trailing: bytes = b"") -> bytes:
        """Build a minimal WAV with a standard 16-byte fmt chunk."""
        fmt_chunk = (
            b"fmt "
            + struct.pack("<I", 16)
            + struct.pack("<H", 1)       # PCM format
            + struct.pack("<H", 1)       # mono
            + struct.pack("<I", 16000)   # sample rate
            + struct.pack("<I", 32000)   # byte rate
            + struct.pack("<H", 2)       # block align
            + struct.pack("<H", 16)      # bits per sample
            + fmt_extra
        )
        data_chunk = b"data" + struct.pack("<I", len(pcm)) + pcm
        body = b"WAVE" + fmt_chunk + data_chunk + trailing
        riff_header = b"RIFF" + struct.pack("<I", len(body))
        return riff_header + body

    def test_non_riff_passthrough(self) -> None:
        raw = b"\x01\x02\x03\x04\x05\x06"
        assert OracleTTS._strip_wav_header(raw) == raw

    def test_short_input_passthrough(self) -> None:
        raw = b"RIFF" + b"\x00" * 10
        assert OracleTTS._strip_wav_header(raw) == raw

    def test_wrong_wave_id_passthrough(self) -> None:
        raw = b"RIFF" + struct.pack("<I", 100) + b"AVI " + b"\x00" * 100
        assert OracleTTS._strip_wav_header(raw) == raw

    def test_standard_wav_extracts_pcm(self) -> None:
        pcm = b"\x01\x02" * 100
        wav = self._make_wav(pcm)
        assert OracleTTS._strip_wav_header(wav) == pcm

    def test_odd_length_pcm_trimmed(self) -> None:
        pcm = b"\x01\x02\x03"  # 3 bytes, odd
        wav = self._make_wav(pcm)
        result = OracleTTS._strip_wav_header(wav)
        assert len(result) % 2 == 0
        assert result == b"\x01\x02"

    def test_data_chunk_size_zero_uses_remaining(self) -> None:
        """When data chunk declares size=0 (streaming placeholder),
        all remaining bytes after the data header should be used."""
        riff = b"RIFF" + struct.pack("<I", 36) + b"WAVE"
        fmt_chunk = (
            b"fmt "
            + struct.pack("<I", 16)
            + struct.pack("<H", 1)
            + struct.pack("<H", 1)
            + struct.pack("<I", 16000)
            + struct.pack("<I", 32000)
            + struct.pack("<H", 2)
            + struct.pack("<H", 16)
        )
        data_chunk = b"data" + struct.pack("<I", 0)
        pcm = b"\x01\x02\x03\x04"
        wav = riff + fmt_chunk + data_chunk + pcm
        assert OracleTTS._strip_wav_header(wav) == pcm

    def test_trailing_metadata_chunks_excluded(self) -> None:
        """When valid trailing WAV chunks exist after the data chunk,
        the declared data chunk_size should be trusted to exclude
        trailing metadata (prevents audio pops)."""
        pcm = b"\xAA\xBB" * 50  # 100 bytes of PCM
        trailing = b"LIST" + struct.pack("<I", 4) + b"INFO"
        wav = self._make_wav(pcm, trailing=trailing)
        result = OracleTTS._strip_wav_header(wav)
        assert result == pcm
        assert trailing[0:4] not in result

    def test_empty_pcm_returns_empty(self) -> None:
        wav = self._make_wav(b"")
        assert OracleTTS._strip_wav_header(wav) == b""

    def test_large_declared_size_uses_remaining(self) -> None:
        """When chunk_size is larger than remaining data (no valid trailing chunk),
        all remaining bytes should be returned."""
        pcm = b"\x01\x02" * 20
        fmt_chunk = (
            b"fmt "
            + struct.pack("<I", 16)
            + struct.pack("<H", 1)
            + struct.pack("<H", 1)
            + struct.pack("<I", 16000)
            + struct.pack("<I", 32000)
            + struct.pack("<H", 2)
            + struct.pack("<H", 16)
        )
        fake_large_size = 99999
        data_chunk = b"data" + struct.pack("<I", fake_large_size) + pcm
        body = b"WAVE" + fmt_chunk + data_chunk
        riff_header = b"RIFF" + struct.pack("<I", len(body))
        wav = riff_header + body
        result = OracleTTS._strip_wav_header(wav)
        assert result == pcm
