import json
from pathlib import Path

import pytest

from gmicloud_tts2_python.config import GmicloudTTS2Config

CONFIGS = Path(__file__).parent / "configs"


def load_config(name: str) -> GmicloudTTS2Config:
    raw = json.loads((CONFIGS / name).read_text())
    config = GmicloudTTS2Config.model_validate(raw)
    config.update_params()
    return config


def test_basic_config_loads_and_validates():
    config = load_config("property_basic_audio_setting1.json")
    config.validate()
    assert config.model == "inworld-tts-2"
    assert config.sample_rate == 16000
    assert config.channels == 1


def test_alt_model_config():
    config = load_config("property_basic_audio_setting2.json")
    config.validate()
    assert config.model == "inworld-tts-2"
    # the rate-comparison harness requires the two fixtures to differ
    assert config.sample_rate == 32000


def test_dump_config():
    config = load_config("property_dump.json")
    config.validate()
    assert config.dump is True


def test_missing_api_key_rejected():
    config = load_config("property_miss_required.json")
    with pytest.raises(ValueError, match="api_key is required"):
        config.validate()


def test_invalid_config_rejected():
    with pytest.raises(ValueError):
        load_config("property_invalid.json")


def test_chatterbox_requires_voice():
    config = GmicloudTTS2Config(params={
        "api_key": "k", "model": "Chatterbox-tts", "voice": None,
    })
    config.update_params()
    with pytest.raises(ValueError, match="voice UUID"):
        config.validate()


def test_channels_must_be_mono():
    config = GmicloudTTS2Config(params={"api_key": "k", "channels": 2})
    with pytest.raises(ValueError, match="channels must be 1"):
        config.update_params()


def test_temperature_bounds():
    config = GmicloudTTS2Config(params={"api_key": "k", "temperature": 0.5})
    config.update_params()
    config.validate()
    assert config.temperature == 0.5
    bad = GmicloudTTS2Config(params={"api_key": "k", "temperature": 3.0})
    with pytest.raises(ValueError, match="temperature"):
        bad.update_params()


def test_text_chunk_chars_bounds():
    for bad in (0, -5, 2001):
        config = GmicloudTTS2Config(params={"api_key": "k", "text_chunk_chars": bad})
        with pytest.raises(ValueError):
            config.update_params()
    for ok in (1, 1000, 2000):
        config = GmicloudTTS2Config(params={"api_key": "k", "text_chunk_chars": ok})
        config.update_params()
        config.validate()
        assert config.text_chunk_chars == ok


def test_config_masks_api_key():
    config = GmicloudTTS2Config(params={"api_key": "secret-key-123"})
    config.update_params()
    config.validate()
    assert "secret-key-123" not in config.to_str(sensitive_handling=True)
    assert config.to_str(sensitive_handling=True).endswith("api_key=secr***)")
