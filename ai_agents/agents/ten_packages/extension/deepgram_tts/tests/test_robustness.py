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
from unittest.mock import MagicMock


def create_mock_client():
    mock = MagicMock()
    mock.start = AsyncMock()
    mock.stop = AsyncMock()
    mock.cancel = AsyncMock()
    mock.reset_ttfb = lambda: None
    fake_audio = b"\x00\x01\x02\x03" * 100

    async def mock_get(text):
        yield (100, EVENT_TTS_TTFB_METRIC)
        yield (fake_audio, EVENT_TTS_RESPONSE)
        yield (None, EVENT_TTS_END)

    mock.get.side_effect = mock_get
    return mock


# ================ test empty text ================
class ExtensionTesterEmptyText(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Empty text test started.")

        tts_input = TTSTextInput(
            request_id="tts_request_empty",
            text="",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end for empty text.")
            self.audio_end_received = True
            ten_env.stop_test()


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_empty_text(MockDeepgramTTSClient):
    """Test that empty text is handled gracefully."""
    MockDeepgramTTSClient.return_value = create_mock_client()

    tester = ExtensionTesterEmptyText()
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

    assert (
        tester.audio_end_received
    ), "tts_audio_end should be sent for empty text."


# ================ test whitespace only text ================
class ExtensionTesterWhitespaceText(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Whitespace text test started.")

        tts_input = TTSTextInput(
            request_id="tts_request_whitespace",
            text="   \n\t   ",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end for whitespace text.")
            self.audio_end_received = True
            ten_env.stop_test()


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_whitespace_text(MockDeepgramTTSClient):
    """Test that whitespace-only text is handled gracefully."""
    MockDeepgramTTSClient.return_value = create_mock_client()

    tester = ExtensionTesterWhitespaceText()
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

    assert (
        tester.audio_end_received
    ), "tts_audio_end should be sent for whitespace text."


# ================ test long text ================
class ExtensionTesterLongText(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.audio_chunks_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Long text test started.")

        long_text = "This is a longer piece of text. " * 20

        tts_input = TTSTextInput(
            request_id="tts_request_long",
            text=long_text,
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end for long text.")
            self.audio_end_received = True
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        self.audio_chunks_count += 1


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_long_text(MockDeepgramTTSClient):
    """Test that long text is handled correctly."""
    MockDeepgramTTSClient.return_value = create_mock_client()

    tester = ExtensionTesterLongText()
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

    assert (
        tester.audio_end_received
    ), "tts_audio_end was not received for long text."
    assert (
        tester.audio_chunks_count > 0
    ), "No audio chunks received for long text."


# ================ test special characters ================
class ExtensionTesterSpecialChars(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.error_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Special characters test started.")

        tts_input = TTSTextInput(
            request_id="tts_request_special",
            text="Hello! How are you? I'm fine, thanks. $100 is 100%.",
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
        elif name == "error":
            self.error_received = True
            ten_env.stop_test()


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_special_characters(MockDeepgramTTSClient):
    """Test that special characters are handled correctly."""
    MockDeepgramTTSClient.return_value = create_mock_client()

    tester = ExtensionTesterSpecialChars()
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
    assert (
        not tester.error_received
    ), "Error should not be received for special chars."
