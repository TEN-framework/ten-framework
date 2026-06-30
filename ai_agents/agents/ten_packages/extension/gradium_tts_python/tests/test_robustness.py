import json
from unittest.mock import patch

from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester

from .gradium_mocks import make_streaming_mock_client


def create_mock_client():
    return make_streaming_mock_client(
        audio_chunks=(b"\x00\x01\x02\x03" * 100,), ttfb_ms=100
    )


class ExtensionTesterEmptyText(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
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
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_empty_text(mock_client):
    mock_client.return_value = create_mock_client()
    tester = ExtensionTesterEmptyText()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                }
            }
        ),
    )
    tester.run()
    assert tester.audio_end_received


class ExtensionTesterWhitespaceText(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
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
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_whitespace_text(mock_client):
    mock_client.return_value = create_mock_client()
    tester = ExtensionTesterWhitespaceText()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                }
            }
        ),
    )
    tester.run()
    assert tester.audio_end_received


class ExtensionTesterLongText(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.audio_chunks_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
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
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()

    def on_audio_frame(self, _ten_env: TenEnvTester, _audio_frame):
        self.audio_chunks_count += 1


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_long_text(mock_client):
    mock_client.return_value = create_mock_client()
    tester = ExtensionTesterLongText()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                }
            }
        ),
    )
    tester.run()
    assert tester.audio_end_received
    assert tester.audio_chunks_count > 0


class ExtensionTesterSpecialChars(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.error_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
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
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()
        elif data.get_name() == "error":
            self.error_received = True
            ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_special_characters(mock_client):
    mock_client.return_value = create_mock_client()
    tester = ExtensionTesterSpecialChars()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                }
            }
        ),
    )
    tester.run()
    assert tester.audio_end_received
    assert not tester.error_received


class ExtensionTesterPunctuationOnly(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.error_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        tts_input = TTSTextInput(
            request_id="tts_request_punctuation",
            text="...?!",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()
        elif data.get_name() == "error":
            self.error_received = True
            ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_punctuation_only_text(mock_client):
    mock_client.return_value = create_mock_client()
    tester = ExtensionTesterPunctuationOnly()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                }
            }
        ),
    )
    tester.run()
    assert tester.audio_end_received
    assert not tester.error_received
