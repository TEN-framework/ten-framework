"""Configuration tests for the Breeze TTS extension."""

from __future__ import annotations

import pytest

from breeze_tts import SAMPLE_RATE, BreezeTTSParams
from config import MAX_INSTRUCTIONS, BreezeTTSConfig


def make_config(**params):
    return BreezeTTSConfig(params=BreezeTTSParams(**params))


def test_param_defaults():
    params = BreezeTTSParams(api_key="brz_test")
    assert params.base_url == "https://api.breeze.blue"
    assert params.voice_id == "voc_u3nxkd5578wc"
    assert params.speed == 1.0


def test_update_params_normalises_values():
    config = make_config(
        api_key="  brz_test  ",
        base_url="https://api.breeze.blue/",
        speed=5.0,
        instructions="x" * (MAX_INSTRUCTIONS + 40),
    )
    config.update_params()

    assert config.params.api_key == "brz_test"
    assert config.params.base_url == "https://api.breeze.blue"
    assert config.params.speed == 2.0
    assert len(config.params.instructions) == MAX_INSTRUCTIONS


def test_speed_is_clamped_below():
    config = make_config(api_key="brz_test", speed=0.1)
    config.update_params()
    assert config.params.speed == 0.5


def test_validate_rejects_missing_key():
    config = make_config(api_key="")
    with pytest.raises(ValueError, match="api_key"):
        config.validate()


def test_validate_rejects_missing_voice():
    config = make_config(api_key="brz_test", voice_id="")
    with pytest.raises(ValueError, match="voice_id"):
        config.validate()


def test_validate_accepts_complete_config():
    config = make_config(api_key="brz_test")
    config.update_params()
    config.validate()


def test_to_str_masks_api_key():
    config = make_config(api_key="brz_secret")
    assert "brz_secret" not in config.to_str()


def test_sample_rate_is_fixed():
    assert SAMPLE_RATE == 24000
