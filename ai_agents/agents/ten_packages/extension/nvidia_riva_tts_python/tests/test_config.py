#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#
import pytest
from nvidia_riva_tts_python.config import NvidiaRivaTTSConfig


def test_config_validation():
    """Test configuration validation"""
    # Valid config
    config = NvidiaRivaTTSConfig(
        params={
            "server": "localhost:50051",
            "language_code": "en-US",
            "voice_name": "English-US.Female-1",
            "sample_rate": 16000,
        }
    )
    config.validate()  # Should not raise

    # Missing server
    with pytest.raises(ValueError, match="Server address is required"):
        config = NvidiaRivaTTSConfig(
            params={
                "language_code": "en-US",
                "voice_name": "English-US.Female-1",
            }
        )
        config.validate()

    # Missing language_code
    with pytest.raises(ValueError, match="Language code is required"):
        config = NvidiaRivaTTSConfig(
            params={
                "server": "localhost:50051",
                "voice_name": "English-US.Female-1",
            }
        )
        config.validate()

    # Missing voice_name
    with pytest.raises(ValueError, match="Voice name is required"):
        config = NvidiaRivaTTSConfig(
            params={
                "server": "localhost:50051",
                "language_code": "en-US",
            }
        )
        config.validate()


def test_config_defaults():
    """Test default configuration values"""
    config = NvidiaRivaTTSConfig(
        params={
            "server": "localhost:50051",
            "language_code": "en-US",
            "voice_name": "English-US.Female-1",
        }
    )

    assert config.dump is False
    assert "nvidia_riva_tts_in.pcm" in config.dump_path
    assert config.params["server"] == "localhost:50051"
    assert config.params.get("sample_rate", 16000) == 16000
    assert config.params.get("use_ssl", False) is False
