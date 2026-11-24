"""
Basic functionality tests for Gemini TTS extension
"""

import asyncio
import base64
from types import SimpleNamespace

from ten_ai_base.struct import TTS2HttpResponseEventType

from gemini_tts_python.config import GeminiTTSConfig
from gemini_tts_python.gemini_tts import GeminiTTSClient


def test_import():
    """Test that extension can be imported"""
    from gemini_tts_python.extension import GeminiTTSExtension

    assert GeminiTTSExtension is not None


def test_config_flash_model():
    """Test configuration model with Flash model"""
    config = GeminiTTSConfig(
        params={
            "api_key": "test_key",
            "model": "gemini-3.8-flash-tts",
            "voice": "Kore",
        }
    )

    assert config.params["api_key"] == "test_key"
    assert config.params["model"] == "gemini-3.8-flash-tts"
    assert config.params["voice"] == "Kore"


def test_config_lite_model():
    """Test configuration model with Flash Lite model"""
    config = GeminiTTSConfig(
        params={
            "api_key": "test_key",
            "model": "gemini-3.8-flash-lite-tts",
            "voice": "Charon",
        }
    )

    assert config.params["api_key"] == "test_key"
    assert config.params["model"] == "gemini-3.8-flash-lite-tts"
    assert config.params["voice"] == "Charon"


def test_default_model_uses_current_tts_model():
    config = GeminiTTSConfig(params={"api_key": "test_key"})
    config.update_params()

    assert config.params["model"] == "gemini-3.8-flash-tts"


def test_client_requests_raw_pcm_from_interactions():
    pcm = b"\x00\x01\x02\x03"

    class FakeTenEnv:
        def log_debug(self, *_args, **_kwargs):
            pass

        def log_error(self, *_args, **_kwargs):
            pass

        def log_info(self, *_args, **_kwargs):
            pass

    class FakeInteractions:
        def __init__(self):
            self.payload = None

        def create(self, **kwargs):
            self.payload = kwargs
            return SimpleNamespace(
                output_audio=SimpleNamespace(
                    data=base64.b64encode(pcm).decode("ascii")
                )
            )

    config = GeminiTTSConfig(
        params={
            "api_key": "test_key",
            "model": "gemini-3.8-flash-tts",
            "voice": "Kore",
            "language_code": "en-US",
            "style_prompt": "Speak calmly",
        }
    )
    client = GeminiTTSClient(config, FakeTenEnv())
    interactions = FakeInteractions()
    client.client = SimpleNamespace(interactions=interactions)

    events = asyncio.run(_collect_events(client))

    assert interactions.payload["model"] == "gemini-3.8-flash-tts"
    assert interactions.payload["input"][0]["content"][0]["text"] == (
        "Speak calmly\n\nHello"
    )
    assert interactions.payload["response_format"] == {
        "type": "audio",
        "mime_type": "audio/l16",
        "sample_rate": 24000,
    }
    assert interactions.payload["generation_config"]["speech_config"] == [
        {"voice": "Kore", "language": "en-US"}
    ]
    assert events == [
        (pcm, TTS2HttpResponseEventType.RESPONSE),
        (None, TTS2HttpResponseEventType.END),
    ]


async def _collect_events(client):
    return [event async for event in client.get("Hello", "request-1")]
