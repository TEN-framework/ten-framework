import json
from unittest.mock import AsyncMock, patch

from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester


class ExtensionTesterEmptyParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)
            self.error_code = error_data.get("code")
            self.error_message = error_data.get("message", "")
            ten_env.stop_test()


def test_empty_params_fatal_error():
    tester = ExtensionTesterEmptyParams()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps({"params": {"api_key": "", "voice_id": "cLONiZ4hQ8VpQ4Sz"}}),
    )
    tester.run()

    assert tester.error_received
    assert tester.error_code == -1000
    assert tester.error_message


class ExtensionTesterInvalidApiKey(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.vendor_info = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        tts_input = TTSTextInput(
            request_id="test-request-invalid-key",
            text="This text will trigger API key validation.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)
            self.error_code = error_data.get("code")
            self.vendor_info = error_data.get("vendor_info", {})
            ten_env.stop_test()
        elif data.get_name() == "tts_audio_end":
            ten_env.stop_test()


@patch("gradium_tts_python.gradium_tts.websockets.connect")
def test_invalid_api_key_error(mock_websocket_connect):
    mock_websocket_connect.side_effect = Exception(
        "401 Unauthorized - Invalid API key"
    )

    tester = ExtensionTesterInvalidApiKey()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "invalid_api_key_test",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                }
            }
        ),
    )
    tester.run()

    assert tester.error_received
    assert tester.error_code == -1000
    assert tester.vendor_info is not None
    assert tester.vendor_info.get("vendor") == "gradium"
