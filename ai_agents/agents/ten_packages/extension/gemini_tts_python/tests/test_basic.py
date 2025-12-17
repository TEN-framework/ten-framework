"""
Basic functionality tests for Gemini TTS extension
"""


def test_import():
    """Test that extension can be imported"""
    from gemini_tts_python.extension import GeminiTTSExtension

    assert GeminiTTSExtension is not None


def test_config_flash_model():
    """Test configuration model with Flash model"""
    from gemini_tts_python.config import GeminiTTSConfig

    config = GeminiTTSConfig(
        params={
            "api_key": "test_key",
            "model": "gemini-2.5-flash-preview-tts",
            "voice": "Kore",
        }
    )

    assert config.params["api_key"] == "test_key"
    assert config.params["model"] == "gemini-2.5-flash-preview-tts"
    assert config.params["voice"] == "Kore"


def test_config_pro_model():
    """Test configuration model with Pro model"""
    from gemini_tts_python.config import GeminiTTSConfig

    config = GeminiTTSConfig(
        params={
            "api_key": "test_key",
            "model": "gemini-2.5-pro-preview-tts",
            "voice": "Charon",
        }
    )

    assert config.params["api_key"] == "test_key"
    assert config.params["model"] == "gemini-2.5-pro-preview-tts"
    assert config.params["voice"] == "Charon"
