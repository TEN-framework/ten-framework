import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

import pytest


def _config(**params):
    from breezeblue_tts_python.config import BreezeBlueTTSConfig

    config = BreezeBlueTTSConfig(params=params)
    config.update_params()
    return config


def test_default_endpoint_contains_voice():
    config = _config(api_key="k", voice_id="voc_abc")
    assert (
        config.url == "https://api.breeze.blue/v1/text-to-speech/voc_abc/stream"
    )
    assert "base_url" not in config.params


def test_custom_base_url_trailing_slash():
    config = _config(api_key="k", voice_id="voc_abc", base_url="http://h:1/")
    assert config.url == "http://h:1/v1/text-to-speech/voc_abc/stream"


def test_explicit_url_wins():
    from breezeblue_tts_python.config import BreezeBlueTTSConfig

    config = BreezeBlueTTSConfig(
        url="http://proxy/tts", params={"api_key": "k", "voice_id": "voc_abc"}
    )
    config.update_params()
    assert config.url == "http://proxy/tts"


def test_frontend_sample_rate_is_removed():
    config = _config(api_key="k", voice_id="v", sample_rate=16000)
    assert "sample_rate" not in config.params


def test_request_body_is_whitelisted():
    """The vendor rejects unknown fields (additionalProperties: false)."""
    config = _config(
        api_key="k",
        voice_id="v",
        model_id="breeze-tts-2",
        language_code="en",
        instructions="Speak warmly.",
        speed=1.2,
        volume=0.8,
        bogus="x",
        sample_rate=16000,
    )
    body = config.build_request_body("hello")
    assert body == {
        "text": "hello",
        "model_id": "breeze-tts-2",
        "language_code": "en",
        "instructions": "Speak warmly.",
        "voice_settings": {"speed": 1.2, "volume": 0.8},
    }
    # Credentials and routing never leak into the body.
    assert "api_key" not in body and "voice_id" not in body


def test_nested_voice_settings_flat_keys_win():
    config = _config(
        api_key="k",
        voice_id="v",
        voice_settings={"speed": 1.5, "guidance_scale": 3.0, "junk": 1},
        speed=1.1,
    )
    body = config.build_request_body("hi")
    assert body["voice_settings"] == {"speed": 1.1, "guidance_scale": 3.0}


def test_minimal_body_has_only_text():
    body = _config(api_key="k", voice_id="v").build_request_body("hi")
    assert body == {"text": "hi"}


def test_validate_requires_api_key():
    with pytest.raises(ValueError, match="API key"):
        _config(api_key="", voice_id="v").validate()


def test_validate_requires_voice_id():
    with pytest.raises(ValueError, match="voice_id"):
        _config(api_key="k").validate()


def test_validate_ok():
    _config(api_key="k", voice_id="v").validate()


def test_auth_header_satisfies_api_key_requirement():
    from breezeblue_tts_python.config import BreezeBlueTTSConfig

    config = BreezeBlueTTSConfig(
        headers={"xi-api-key": "k"}, params={"voice_id": "v"}
    )
    config.update_params()
    config.validate()


def test_to_str_masks_api_key():
    config = _config(api_key="brz_supersecretkey1234567890", voice_id="v")
    assert "brz_supersecretkey1234567890" not in config.to_str()
