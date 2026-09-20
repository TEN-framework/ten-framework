#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import pytest
from speechmatics_tts_python.config import SpeechmaticsTTSConfig


def test_config_creation():
    """Test basic config creation"""
    config = SpeechmaticsTTSConfig(
        params={
            "api_key": "test_key",
            "voice_id": "sarah",
        }
    )
    assert config.params["api_key"] == "test_key"
    assert config.params["voice_id"] == "sarah"


def test_config_validation_missing_api_key():
    """Test validation fails without API key"""
    config = SpeechmaticsTTSConfig(params={"voice_id": "sarah"})
    with pytest.raises(ValueError, match="API key is required"):
        config.validate()


def test_config_validation_missing_voice_id():
    """Test validation fails without voice ID"""
    config = SpeechmaticsTTSConfig(params={"api_key": "test_key"})
    with pytest.raises(ValueError, match="Voice ID is required"):
        config.validate()


def test_config_validation_success():
    """Test validation succeeds with required fields"""
    config = SpeechmaticsTTSConfig(
        params={
            "api_key": "test_key",
            "voice_id": "sarah",
        }
    )
    config.validate()  # Should not raise


def test_config_to_str_sensitive_handling():
    """Test sensitive data handling in to_str"""
    config = SpeechmaticsTTSConfig(
        params={
            "api_key": "secret_key_12345",
            "voice_id": "sarah",
        }
    )
    config_str = config.to_str(sensitive_handling=True)
    assert "secret_key_12345" not in config_str


def test_config_default_values():
    """Test default configuration values"""
    config = SpeechmaticsTTSConfig(
        params={
            "api_key": "test_key",
            "voice_id": "sarah",
        }
    )
    assert config.dump is False
    assert "speechmatics_tts_in.pcm" in config.dump_path


def test_config_with_optional_params():
    """Test configuration with optional parameters"""
    config = SpeechmaticsTTSConfig(
        params={
            "api_key": "test_key",
            "voice_id": "megan",
            "output_format": "mp3",
            "sample_rate": 24000,
            "base_url": "https://custom.api.com",
        }
    )
    assert config.params["output_format"] == "mp3"
    assert config.params["sample_rate"] == 24000
    assert config.params["base_url"] == "https://custom.api.com"


def test_config_voice_options():
    """Test different voice configurations"""
    voices = ["sarah", "theo", "megan", "jack"]
    for voice in voices:
        config = SpeechmaticsTTSConfig(
            params={
                "api_key": "test_key",
                "voice_id": voice,
            }
        )
        assert config.params["voice_id"] == voice


def test_config_output_formats():
    """Test different output format configurations"""
    formats = ["wav", "mp3"]
    for fmt in formats:
        config = SpeechmaticsTTSConfig(
            params={
                "api_key": "test_key",
                "voice_id": "sarah",
                "output_format": fmt,
            }
        )
        assert config.params["output_format"] == fmt
