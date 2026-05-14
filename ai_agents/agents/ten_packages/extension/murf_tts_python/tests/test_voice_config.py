import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
# The project root is 6 levels up from the parent directory of this file.
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

import asyncio
import json
from unittest.mock import AsyncMock

from murf_tts_python.config import MurfTTSConfig
from murf_tts_python.murf_tts import MurfTTSynthesizer


class FakeTenEnv:
    def log_info(self, *args, **kwargs) -> None:
        pass

    def log_debug(self, *args, **kwargs) -> None:
        pass

    def log_warn(self, *args, **kwargs) -> None:
        pass

    def log_error(self, *args, **kwargs) -> None:
        pass


def test_get_voice_config_from_params_filters_to_supported_keys():
    synthesizer = object.__new__(MurfTTSynthesizer)
    synthesizer.config = MurfTTSConfig(
        params={
            "api_key": "secret",
            "base_url": "wss://example.com/murf",
            "sample_rate": 16000,
            "voiceId": "Natalie",
            "locale": "en-GB",
            "style": "Narration",
            "rate": 0.2,
            "pitch": -0.1,
            "variation": 1.1,
            "model": "GEN2",
            "unknown": "ignored",
            "pronunciation_dictionary": {
                "live": {
                    "type": "IPA",
                    "pronunciation": "laɪv",
                }
            },
        }
    )

    assert synthesizer._get_voice_config_from_params() == {
        "voiceId": "Natalie",
        "locale": "en-GB",
        "style": "Narration",
        "rate": 0.2,
        "pitch": -0.1,
        "variation": 1.1,
        "pronunciation_dictionary": {
            "live": {
                "type": "IPA",
                "pronunciation": "laɪv",
            }
        },
    }


def test_get_advanced_settings_from_params_filters_to_supported_keys():
    synthesizer = object.__new__(MurfTTSynthesizer)
    synthesizer.config = MurfTTSConfig(
        params={
            "min_buffer_size": 60,
            "max_buffer_delay_in_ms": 500,
            "voiceId": "Natalie",
            "unknown": "ignored",
        }
    )

    assert synthesizer._get_advanced_settings_from_params() == {
        "min_buffer_size": 60,
        "max_buffer_delay_in_ms": 500,
    }


def test_send_voice_config_uses_filtered_voice_params():
    synthesizer = object.__new__(MurfTTSynthesizer)
    synthesizer.config = MurfTTSConfig(
        params={
            "voiceId": "Natalie",
            "locale": "en-US",
            "style": "Narration",
            "pronunciation_dictionary": {
                "2010": {
                    "type": "SAY_AS",
                    "pronunciation": "two thousand and ten",
                }
            },
            "api_key": "secret",
            "model": "GEN2",
            "sample_rate": 16000,
        }
    )
    synthesizer.ten_env = FakeTenEnv()
    ws = AsyncMock()

    asyncio.run(synthesizer._send_voice_config(ws))

    sent_payload = json.loads(ws.send.await_args.args[0])
    assert sent_payload == {
        "voice_config": {
            "voiceId": "Natalie",
            "locale": "en-US",
            "style": "Narration",
            "pronunciation_dictionary": {
                "2010": {
                    "type": "SAY_AS",
                    "pronunciation": "two thousand and ten",
                }
            },
        }
    }


def test_send_advanced_settings_uses_filtered_params_once_configured():
    synthesizer = object.__new__(MurfTTSynthesizer)
    synthesizer.config = MurfTTSConfig(
        params={
            "min_buffer_size": 60,
            "max_buffer_delay_in_ms": 500,
            "voiceId": "Natalie",
        }
    )
    synthesizer.ten_env = FakeTenEnv()
    ws = AsyncMock()

    asyncio.run(synthesizer._send_advanced_settings(ws))

    sent_payload = json.loads(ws.send.await_args.args[0])
    assert sent_payload == {
        "min_buffer_size": 60,
        "max_buffer_delay_in_ms": 500,
    }


def test_send_advanced_settings_skips_when_not_configured():
    synthesizer = object.__new__(MurfTTSynthesizer)
    synthesizer.config = MurfTTSConfig(
        params={
            "voiceId": "Natalie",
            "style": "Narration",
        }
    )
    synthesizer.ten_env = FakeTenEnv()
    ws = AsyncMock()

    asyncio.run(synthesizer._send_advanced_settings(ws))

    ws.send.assert_not_called()


def test_send_text_includes_voice_config_with_each_message():
    synthesizer = object.__new__(MurfTTSynthesizer)
    synthesizer.config = MurfTTSConfig(
        params={
            "voiceId": "Natalie",
            "locale": "en-GB",
            "style": "Narration",
            "rate": 0.2,
            "pronunciation_dictionary": {
                "live": {
                    "type": "IPA",
                    "pronunciation": "laɪv",
                }
            },
            "api_key": "secret",
            "unknown": "ignored",
        }
    )
    synthesizer.ten_env = FakeTenEnv()
    synthesizer.request_first_chunk_sent_time = {}
    ws = AsyncMock()

    asyncio.run(
        synthesizer._send_text_internal(
            ws, "hello world", "request-1", is_last=True
        )
    )

    sent_payload = json.loads(ws.send.await_args.args[0])
    assert sent_payload == {
        "text": "hello world",
        "context_id": "request-1",
        "end": True,
        "voice_config": {
            "voiceId": "Natalie",
            "locale": "en-GB",
            "style": "Narration",
            "rate": 0.2,
            "pronunciation_dictionary": {
                "live": {
                    "type": "IPA",
                    "pronunciation": "laɪv",
                }
            },
        },
    }
