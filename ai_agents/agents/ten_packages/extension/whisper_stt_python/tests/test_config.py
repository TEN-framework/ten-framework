#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#

import pytest
from whisper_stt_python.config import WhisperSTTConfig


def test_config_default_values():
    """Test default configuration values"""
    config = WhisperSTTConfig()
    assert config.dump is False
    assert config.dump_path == "/tmp"
    assert config.finalize_mode == "disconnect"
    assert config.silence_duration_ms == 1000
    assert config.params == {}


def test_config_from_json():
    """Test configuration from JSON"""
    json_str = """{
        "dump": true,
        "dump_path": "/var/log",
        "finalize_mode": "silence",
        "params": {
            "model": "base",
            "device": "cpu",
            "language": "en"
        }
    }"""
    config = WhisperSTTConfig.model_validate_json(json_str)
    assert config.dump is True
    assert config.dump_path == "/var/log"
    assert config.finalize_mode == "silence"
    assert config.params["model"] == "base"
    assert config.params["device"] == "cpu"
    assert config.params["language"] == "en"


def test_config_update():
    """Test configuration update method"""
    config = WhisperSTTConfig()
    config.update({"dump": True, "dump_path": "/custom/path"})
    assert config.dump is True
    assert config.dump_path == "/custom/path"


def test_config_to_json_no_sensitive():
    """Test JSON serialization without sensitive handling"""
    config = WhisperSTTConfig(params={"api_key": "secret123", "model": "base"})
    json_str = config.to_json(sensitive_handling=False)
    assert "secret123" in json_str
    assert "model" in json_str


def test_config_to_json_with_sensitive():
    """Test JSON serialization with sensitive data masking"""
    config = WhisperSTTConfig(params={"api_key": "secret123", "model": "base"})
    json_str = config.to_json(sensitive_handling=True)
    assert "secret123" not in json_str
    assert "model" in json_str


def test_normalized_language_mapping():
    """Test language code normalization"""
    test_cases = [
        ("en", "en-US"),
        ("zh", "zh-CN"),
        ("ja", "ja-JP"),
        ("fr", "fr-FR"),
        ("de", "de-DE"),
        ("unknown", "unknown"),
    ]

    for lang_code, expected in test_cases:
        config = WhisperSTTConfig(params={"language": lang_code})
        assert config.normalized_language == expected


def test_normalized_language_empty():
    """Test normalized language with empty params"""
    config = WhisperSTTConfig()
    assert config.normalized_language == ""


def test_config_params_passthrough():
    """Test params pass-through design"""
    config = WhisperSTTConfig(
        params={
            "model": "large-v3",
            "device": "cuda",
            "compute_type": "float16",
            "custom_param": "custom_value",
        }
    )
    assert config.params["model"] == "large-v3"
    assert config.params["device"] == "cuda"
    assert config.params["compute_type"] == "float16"
    assert config.params["custom_param"] == "custom_value"


def test_finalize_modes():
    """Test valid finalize modes"""
    config1 = WhisperSTTConfig(finalize_mode="disconnect")
    assert config1.finalize_mode == "disconnect"

    config2 = WhisperSTTConfig(finalize_mode="silence")
    assert config2.finalize_mode == "silence"
