import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
# The project root is 6 levels up from the parent directory of this file.
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
from typing import Any
from unittest.mock import patch, AsyncMock
import asyncio

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
                f"Received error: code={self.error_code}, message={self.error_message}, module={self.error_module}"
            )

            # 立即停止测试
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()


def test_empty_params_fatal_error():
    """Test that empty params raises FATAL ERROR with code -1000"""

    print("Starting test_empty_params_fatal_error...")

    # Empty params configuration for StepFun
    empty_params_config = {
        "params": {
            "api_key": "",
        }
    }

    tester = ExtensionTesterEmptyParams()
    tester.set_test_mode_single(
        "stepfun_tts_python", json.dumps(empty_params_config)
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


# ================ test websocket error handling ================
class ExtensionTesterWebsocketError(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.vendor_code = None
        self.vendor_message = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info(
            "WebSocket error test started, sending TTS request."
        )

        tts_input = TTSTextInput(
            request_id="tts_request_error",
            text="This will trigger an error",
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

            # Check for vendor info
            vendor_info = error_data.get("vendor_info", {})
            if vendor_info:
                self.vendor_code = vendor_info.get("code")
                self.vendor_message = vendor_info.get("message")

            ten_env.log_info(
                f"Received error: code={self.error_code}, message={self.error_message}, "
                f"vendor_code={self.vendor_code}, vendor_message={self.vendor_message}"
            )

            # 立即停止测试
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()


@patch("stepfun_tts_python.extension.StepFunTTSWebsocket")
def test_websocket_error_handling(MockStepFunTTSWebsocket):
    """Test that WebSocket errors are properly handled and reported"""

    print("Starting test_websocket_error_handling with mock...")

    # --- Mock Configuration ---
    mock_instance = MockStepFunTTSWebsocket.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()

    # Store the callback that will be set by the extension
    error_callback = None

    def capture_callback(*args, **kwargs):
        # Capture the on_error callback from the keyword arguments
        nonlocal error_callback
        if "on_error" in kwargs:
            error_callback = kwargs["on_error"]
        return mock_instance

    MockStepFunTTSWebsocket.side_effect = capture_callback

    # Simulate a StepFun TTS error
    from stepfun_tts_python.stepfun_tts import StepFunTTSTaskFailedException

    async def mock_get_error(tts_input):
        # Wait a bit to ensure callback is captured
        await asyncio.sleep(0.01)

        # Simulate an error via the error callback
        if error_callback:
            error = StepFunTTSTaskFailedException(
                "Authentication failed", 40000001
            )
            error_callback(error)

    mock_instance.get = AsyncMock(side_effect=mock_get_error)

    # --- Test Setup ---
    config = {
        "params": {
            "api_key": "invalid_key_for_test",
            "model": "step-tts-mini",
            "voice_id": "cixingnansheng",
        }
    }

    tester = ExtensionTesterWebsocketError()
    tester.set_test_mode_single("stepfun_tts_python", json.dumps(config))

    print("Running websocket error test...")
    tester.run()
    print("Websocket error test completed.")

    # Verify error was received
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_code is not None, "Error code should not be None"
    assert tester.error_message is not None, "Error message should not be None"
    assert len(tester.error_message) > 0, "Error message should not be empty"

    # Verify vendor info
    assert tester.vendor_code is not None, "Vendor code should not be None"
    assert (
        tester.vendor_message is not None
    ), "Vendor message should not be None"

    print(
        f"✅ WebSocket error test passed: code={tester.error_code}, message={tester.error_message}, "
        f"vendor_code={tester.vendor_code}, vendor_message={tester.vendor_message}"
    )


# ================ test invalid configuration ================
def test_invalid_config_fatal_error():
    """Test that invalid configuration raises FATAL ERROR"""

    print("Starting test_invalid_config_fatal_error...")

    # Invalid configuration (missing required fields)
    invalid_config = {"params": {"api_key": ""}}

    tester = ExtensionTesterEmptyParams()
    tester.set_test_mode_single(
        "stepfun_tts_python", json.dumps(invalid_config)
    )

    print("Running invalid config test...")
    tester.run()
    print("Invalid config test completed.")

    # Verify FATAL ERROR was received
    assert tester.error_received, "Expected to receive error message"
    assert (
        tester.error_code == -1000
    ), f"Expected error code -1000 (FATAL_ERROR), got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert len(tester.error_message) > 0, "Error message should not be empty"

    print(
        f"✅ Invalid config test passed: code={tester.error_code}, message={tester.error_message}"
    )
    print("Test verification completed successfully.")
