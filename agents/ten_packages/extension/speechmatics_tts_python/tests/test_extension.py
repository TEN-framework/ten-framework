#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import pytest
from speechmatics_tts_python.extension import SpeechmaticsTTSExtension
from speechmatics_tts_python.config import SpeechmaticsTTSConfig


def test_extension_creation():
    """Test extension can be created"""
    extension = SpeechmaticsTTSExtension("test_extension")
    assert extension is not None
    assert extension.vendor() == "speechmatics"


def test_extension_vendor():
    """Test vendor name is correct"""
    extension = SpeechmaticsTTSExtension("test_extension")
    assert extension.vendor() == "speechmatics"


def test_extension_sample_rate_default():
    """Test default sample rate"""
    extension = SpeechmaticsTTSExtension("test_extension")
    extension.config = SpeechmaticsTTSConfig(
        params={
            "api_key": "test_key",
            "voice_id": "sarah",
        }
    )
    assert extension.synthesize_audio_sample_rate() == 16000


def test_extension_sample_rate_custom():
    """Test custom sample rate"""
    extension = SpeechmaticsTTSExtension("test_extension")
    extension.config = SpeechmaticsTTSConfig(
        params={
            "api_key": "test_key",
            "voice_id": "sarah",
            "sample_rate": 24000,
        }
    )
    assert extension.synthesize_audio_sample_rate() == 24000


@pytest.mark.asyncio
async def test_create_config():
    """Test config creation from JSON"""
    extension = SpeechmaticsTTSExtension("test_extension")
    config_json = '{"params": {"api_key": "test_key", "voice_id": "sarah"}}'
    config = await extension.create_config(config_json)
    assert isinstance(config, SpeechmaticsTTSConfig)
    assert config.params["api_key"] == "test_key"
    assert config.params["voice_id"] == "sarah"


def test_extension_inheritance():
    """Test extension inherits from correct base class"""
    from ten_ai_base.tts2_http import AsyncTTS2HttpExtension

    extension = SpeechmaticsTTSExtension("test_extension")
    assert isinstance(extension, AsyncTTS2HttpExtension)


def test_extension_config_types():
    """Test extension handles different config types"""
    extension = SpeechmaticsTTSExtension("test_extension")

    # Test with different voices
    for voice in ["sarah", "theo", "megan", "jack"]:
        extension.config = SpeechmaticsTTSConfig(
            params={
                "api_key": "test_key",
                "voice_id": voice,
            }
        )
        assert extension.config.params["voice_id"] == voice


def test_extension_config_validation():
    """Test extension config validation"""
    extension = SpeechmaticsTTSExtension("test_extension")
    extension.config = SpeechmaticsTTSConfig(
        params={
            "api_key": "test_key",
            "voice_id": "sarah",
        }
    )
    # Should not raise
    extension.config.validate()


def test_extension_with_all_params():
    """Test extension with all configuration parameters"""
    extension = SpeechmaticsTTSExtension("test_extension")
    extension.config = SpeechmaticsTTSConfig(
        params={
            "api_key": "test_key",
            "voice_id": "megan",
            "output_format": "mp3",
            "sample_rate": 24000,
            "base_url": "https://custom.api.com",
        }
    )
    assert extension.config.params["voice_id"] == "megan"
    assert extension.config.params["output_format"] == "mp3"
    assert extension.synthesize_audio_sample_rate() == 24000
