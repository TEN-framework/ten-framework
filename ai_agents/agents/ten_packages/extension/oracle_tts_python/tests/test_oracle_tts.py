from oracle_tts import OracleTTS


def test_strip_wav_header_returns_pcm_when_data_chunk_size_zero() -> None:
    riff = b"RIFF" + (36).to_bytes(4, "little") + b"WAVE"
    fmt_chunk = (
        b"fmt "
        + (16).to_bytes(4, "little")
        + (1).to_bytes(2, "little")
        + (1).to_bytes(2, "little")
        + (16000).to_bytes(4, "little")
        + (32000).to_bytes(4, "little")
        + (2).to_bytes(2, "little")
        + (16).to_bytes(2, "little")
    )
    data_chunk = b"data" + (0).to_bytes(4, "little")
    pcm = b"\x01\x02\x03\x04"

    wav = riff + fmt_chunk + data_chunk + pcm
    stripped = OracleTTS._strip_wav_header(wav)
    assert stripped == pcm
