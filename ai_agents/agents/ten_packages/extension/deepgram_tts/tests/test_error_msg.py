#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from unittest.mock import patch, AsyncMock, MagicMock

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput


# ================ test empty params ================
class ExtensionTesterEmptyParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts"""
        ten_env_tester.log_info("Test started")
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)

            self.error_code = error_data.get("code")
            self.error_message = error_data.get("message", "")
            self.error_module = error_data.get("module", "")

            ten_env.log_info(
                f"Received error: code={self.error_code}, message={self.error_message}"
            )
            ten_env.stop_test()


def test_empty_params_fatal_error():
    """Test that empty params raises FATAL ERROR with code -1000"""
    print("Starting test_empty_params_fatal_error...")

    # Empty params configuration
    empty_params_config = {
        "params": {
            "api_key": "",
        }
    }

    tester = ExtensionTesterEmptyParams()
    tester.set_test_mode_single("deepgram_tts", json.dumps(empty_params_config))

    print("Running test...")
    tester.run()
    print("Test completed.")

    # Verify FATAL ERROR was received
    assert tester.error_received, "Expected to receive error message"
    assert (
        tester.error_code == -1000
    ), f"Expected error code -1000 (FATAL_ERROR), got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert len(tester.error_message) > 0, "Error message should not be empty"

    print(f"Empty params test passed: code={tester.error_code}")


# ================ test invalid api key ================
class ExtensionTesterInvalidApiKey(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.vendor_info = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request to trigger the logic."""
        ten_env_tester.log_info(
            "Invalid API key test started, sending TTS request"
        )

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
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)

            self.error_code = error_data.get("code")
            self.error_message = error_data.get("message", "")
            self.vendor_info = error_data.get("vendor_info", {})

            ten_env.log_info(
                f"Received error: code={self.error_code}, message={self.error_message}"
            )
            ten_env.stop_test()
        elif name == "tts_audio_end":
            ten_env.stop_test()


@patch("deepgram_tts.deepgram_tts.websockets.connect")
def test_invalid_api_key_error(mock_websocket_connect):
    """Test that an invalid API key is handled correctly with a mock."""
    print("Starting test_invalid_api_key_error with mock...")

    # Mock websocket to raise 401 unauthorized error
    mock_websocket_connect.side_effect = Exception(
        "401 Unauthorized - Invalid API key"
    )

    # Config with invalid API key
    invalid_key_config = {
        "params": {
            "api_key": "invalid_api_key_test",
            "model": "aura-2-thalia-en",
            "encoding": "linear16",
            "sample_rate": 24000,
        },
    }

    tester = ExtensionTesterInvalidApiKey()
    tester.set_test_mode_single("deepgram_tts", json.dumps(invalid_key_config))

    print("Running test with mock...")
    tester.run()
    print("Test with mock completed.")

    # Verify FATAL ERROR was received for incorrect API key
    assert tester.error_received, "Expected to receive error message"
    assert (
        tester.error_code == -1000
    ), f"Expected error code -1000 (FATAL_ERROR), got {tester.error_code}"

    # Verify vendor_info
    vendor_info = tester.vendor_info
    assert vendor_info is not None, "Expected vendor_info to be present"
    assert (
        vendor_info.get("vendor") == "deepgram"
    ), f"Expected vendor 'deepgram', got {vendor_info.get('vendor')}"

    print(f"Invalid API key test passed: code={tester.error_code}")
