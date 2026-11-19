from ten_ai_base.struct import TTS2HttpResponseEventType

from gmicloud_tts2_python.client import (
    build_payload,
    extract_audio_url,
    model_family,
    parse_wav,
    resample_s16,
    split_text,
    to_mono_s16,
)
from tests.helpers import make_wav

import struct


def test_model_family_mapping():
    assert model_family("inworld-tts-2") == "inworld"
    assert model_family("minimax-tts-speech-2.8-turbo") == "minimax"
    assert model_family("Chatterbox-tts") == "chatterbox"


def test_inworld_payload_shape():
    payload = build_payload(
        "hello", model="inworld-tts-2", voice="Dennis",
        audio_format="WAV", speed=1.25,
    )
    assert payload == {
        "text": "hello",
        "audio_encoding": "WAV",
        "speaking_rate": 1.25,
        "voice_id": "Dennis",
    }


def test_minimax_payload_shape():
    payload = build_payload(
        "hello", model="minimax-tts-speech-2.8-turbo", voice=None,
        audio_format="WAV", speed=1.0,
    )
    assert payload == {
        "text": "hello",
        "speed": "1.0",
        "format": "wav",
        "voice_id": "English_expressive_narrator",
    }


def test_chatterbox_requires_voice_uuid():
    import pytest

    with pytest.raises(ValueError, match="voice UUID"):
        build_payload(
            "hello", model="Chatterbox-tts", voice=None,
            audio_format="WAV", speed=1.0,
        )
    payload = build_payload(
        "hello", model="Chatterbox-tts", voice="1234-uuid",
        audio_format="WAV", speed=1.0,
    )
    assert payload["voice_uuid"] == "1234-uuid"
    assert payload["data"] == "hello"


def test_inworld_temperature_forwarded_only_when_set():
    with_t = build_payload(
        "hi", model="inworld-tts-2", voice="Dennis",
        audio_format="WAV", speed=1.0, temperature=0.5,
    )
    assert with_t["temperature"] == 0.5
    without = build_payload(
        "hi", model="inworld-tts-2", voice="Dennis",
        audio_format="WAV", speed=1.0,
    )
    assert "temperature" not in without
    minimax = build_payload(
        "hi", model="minimax-tts-speech-2.8-turbo", voice=None,
        audio_format="WAV", speed=1.0, temperature=0.5,
    )
    assert "temperature" not in minimax


def test_split_text_sentence_boundaries():
    text = " ".join(f"Sentence number {i}." for i in range(80))
    chunks = split_text(text, 200)
    assert all(len(c) <= 200 for c in chunks)
    assert " ".join(chunks).replace("  ", " ") == text


def test_split_text_empty():
    assert split_text("   ", 200) == []


def test_split_text_cjk_sentence_boundaries():
    text = "".join(f"这是第{i}句话。" for i in range(30))
    chunks = split_text(text, 40)
    assert all(len(c) <= 40 for c in chunks)
    assert all(c.endswith("。") for c in chunks)


def test_split_text_keeps_numbers_intact():
    chunks = split_text("The value is 3.14 units.", 200)
    assert chunks == ["The value is 3.14 units."]


def test_split_text_never_cuts_mid_word():
    sentence = " ".join(f"word{i}" for i in range(200))  # one long sentence
    chunks = split_text(sentence, 50)
    assert all(len(c) <= 50 for c in chunks)
    # a mid-word cut would surface as split tokens; the rebuilt word
    # sequence must be identical to the original
    assert " ".join(chunks).split() == sentence.split()
    rebuilt = " ".join(chunks)
    assert rebuilt.split() == sentence.split()


def test_split_text_hard_splits_only_oversized_tokens():
    chunks = split_text("a" * 60, 20)
    assert chunks == ["a" * 20, "a" * 20, "a" * 20]
    mixed = split_text("ok " + "b" * 25, 20)
    assert all(len(c) <= 20 for c in mixed)


def test_split_text_cjk_join_inserts_no_spaces():
    text = "第一句话。第二句话。第三句话。"
    chunks = split_text(text, 12)
    assert chunks == ["第一句话。第二句话。", "第三句话。"]
    assert "".join(chunks) == text


def test_parse_wav_roundtrip():
    pcm = struct.pack("<4h", 0, 100, -100, 32000)
    parsed = parse_wav(make_wav(pcm, 16000, 1))
    assert parsed is not None
    data, rate, channels, width = parsed
    assert (data, rate, channels, width) == (pcm, 16000, 1, 2)


def test_parse_wav_rejects_garbage():
    assert parse_wav(b"not a wav file") is None


def test_to_mono_downmixes_stereo():
    stereo = struct.pack("<4h", 100, 300, -100, -300)
    mono = struct.unpack("<2h", to_mono_s16(stereo, 2))
    assert mono == (200, -200)


def test_resample_changes_length():
    pcm = struct.pack("<100h", *([0] * 100))
    out = resample_s16(pcm, 48000, 24000)
    assert len(out) == 100  # 50 samples * 2 bytes


def test_extract_audio_url_variants():
    assert extract_audio_url({"outcome": {"audio_url": "u1"}}) == "u1"
    assert extract_audio_url({"outcome": {"media_urls": [{"url": "u2"}]}}) == "u2"
    assert extract_audio_url({"outcome": {}}) is None
    assert extract_audio_url({}) is None
