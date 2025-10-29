#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import asyncio
from unittest.mock import patch, AsyncMock

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput
from ..tencent_tts import (
    TencentTTSTaskFailedException,
    MESSAGE_TYPE_CMD_ERROR,
)


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
                f"Received error: code={self.error_code}, message={self.error_message}, module={self.error_module}"
            )

            # Stop test immediately
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()


def test_empty_params_fatal_error():
    """Test that empty params raises FATAL ERROR with code -1000"""

    print("Starting test_empty_params_fatal_error...")

    # Empty params configuration
    empty_params_config = {
        "params": {
            "app_id": "",
            "secret_id": "",
            "secret_key": "",
        },
    }

    tester = ExtensionTesterEmptyParams()
    tester.set_test_mode_single(
        "tencent_tts_python", json.dumps(empty_params_config)
    )

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

    print(
        f"✅ Empty params test passed: code={tester.error_code}, message={tester.error_message}"
    )
    print("Test verification completed successfully.")


# ================ test invalid params ================
class ExtensionTesterInvalidParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None
        self.vendor_info = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request to trigger the logic."""
        ten_env_tester.log_info(
            "Test started, sending TTS request to trigger mocked error"
        )

        tts_input = TTSTextInput(
            request_id="test-request-for-invalid-params",
            text="This text will trigger the mocked error.",
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
            self.error_module = error_data.get("module", "")
            self.vendor_info = error_data.get("vendor_info", {})

            ten_env.log_info(
                f"Received error: code={self.error_code}, message={self.error_message}, module={self.error_module}"
            )
            ten_env.log_info(f"Vendor info: {self.vendor_info}")

            # Stop test immediately
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()


@patch("tencent_tts_python.extension.TencentTTSClient")
def test_invalid_params_fatal_error(MockTencentTTSClient):
    """Test that an error from the TTS client is handled correctly with a mock."""

    print("Starting test_invalid_params_fatal_error with mock...")

    # --- Mock Configuration ---
    mock_instance = MockTencentTTSClient.return_value
    # Mock the async methods called on the client instance
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.synthesize_audio = AsyncMock()

    queue = asyncio.Queue()
    queue.put_nowait(
        (
            False,
            MESSAGE_TYPE_CMD_ERROR,
            TencentTTSTaskFailedException(
                10003,
                "鉴权失败(AuthorizationFailed:Please check http header 'Authorization' field or request parameter)",
            ),
        )
    )

    async def mock_get_audio_data():
        return await queue.get()

    mock_instance.get_audio_data.side_effect = mock_get_audio_data

    # --- Test Setup ---
    # Config with valid credentials so on_init passes and can proceed
    # to the request_tts call where the mock will be triggered.
    invalid_params_config = {
        "params": {
            "app_id": "1234567890",
            "secret_id": "test_secret_id",
            "secret_key": "invalid_secret_key",
        },
    }

    tester = ExtensionTesterInvalidParams()
    tester.set_test_mode_single(
        "tencent_tts_python", json.dumps(invalid_params_config)
    )

    print("Running test with mock...")
    tester.run()
    print("Test with mock completed.")

    # --- Assertions ---
    assert tester.error_received, "Expected to receive error message"
    assert (
        tester.error_code == 1000
    ), f"Expected error code 1000, got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert len(tester.error_message) > 0, "Error message should not be empty"

    # Verify vendor_info
    vendor_info = tester.vendor_info
    assert vendor_info is not None, "Expected vendor_info to be present"
    assert (
        vendor_info.get("vendor") == "tencent"
    ), f"Expected vendor 'tencent', got {vendor_info.get('vendor')}"
    assert "code" in vendor_info, "Expected 'code' in vendor_info"
    assert "message" in vendor_info, "Expected 'message' in vendor_info"

    print(
        f"✅ Invalid params test passed with mock: code={tester.error_code}, message={tester.error_message}"
    )
    print(f"✅ Vendor info: {tester.vendor_info}")
    print("Test verification completed successfully.")
