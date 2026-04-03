import sys
from pathlib import Path

# Add project root to sys.path
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from unittest.mock import patch, AsyncMock

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput
from deepgram_tts.deepgram_tts import (
    EVENT_TTS_RESPONSE,
    EVENT_TTS_END,
    EVENT_TTS_TTFB_METRIC,
)


# ================ test different sample rates ================
class ExtensionTesterSampleRate(ExtensionTester):
    def __init__(self, sample_rate: int):
        super().__init__()
        self.sample_rate = sample_rate
        self.audio_end_received = False
        self.audio_chunks_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info(f"Sample rate test: {self.sample_rate}Hz")

        tts_input = TTSTextInput(
            request_id="tts_request_sr",
            text="Testing different sample rates.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        self.audio_chunks_count += 1


def _create_mock_client():
    """Helper to create a mock client for tests."""
    from unittest.mock import MagicMock
    mock = MagicMock()
    mock.start = AsyncMock()
    mock.stop = AsyncMock()
    mock.cancel = AsyncMock()
    mock.reset_ttfb = lambda: None

    fake_audio_chunk = b"\x00\x01\x02\x03" * 100

    async def mock_get_audio_stream(text: str):
        yield (100, EVENT_TTS_TTFB_METRIC)
        yield (fake_audio_chunk, EVENT_TTS_RESPONSE)
        yield (None, EVENT_TTS_END)

    mock.get.side_effect = mock_get_audio_stream
    return mock


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_sample_rate_16000(MockDeepgramTTSClient):
    """Test with 16000 Hz sample rate."""
    MockDeepgramTTSClient.return_value = _create_mock_client()

    tester = ExtensionTesterSampleRate(16000)
    tester.set_test_mode_single(
        "deepgram_tts",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "model": "aura-2-thalia-en",
                    "encoding": "linear16",
                    "sample_rate": 16000,
                },
            }
        ),
    )

    tester.run()

    assert tester.audio_end_received, "tts_audio_end was not received."
    assert tester.audio_chunks_count > 0, "No audio chunks received."


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_sample_rate_24000(MockDeepgramTTSClient):
    """Test with 24000 Hz sample rate."""
    MockDeepgramTTSClient.return_value = _create_mock_client()

    tester = ExtensionTesterSampleRate(24000)
    tester.set_test_mode_single(
        "deepgram_tts",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "model": "aura-2-thalia-en",
                    "encoding": "linear16",
                    "sample_rate": 24000,
                },
            }
        ),
    )

    tester.run()

    assert tester.audio_end_received, "tts_audio_end was not received."
    assert tester.audio_chunks_count > 0, "No audio chunks received."


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_sample_rate_48000(MockDeepgramTTSClient):
    """Test with 48000 Hz sample rate."""
    MockDeepgramTTSClient.return_value = _create_mock_client()

    tester = ExtensionTesterSampleRate(48000)
    tester.set_test_mode_single(
        "deepgram_tts",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "model": "aura-2-thalia-en",
                    "encoding": "linear16",
                    "sample_rate": 48000,
                },
            }
        ),
    )

    tester.run()

    assert tester.audio_end_received, "tts_audio_end was not received."
    assert tester.audio_chunks_count > 0, "No audio chunks received."
