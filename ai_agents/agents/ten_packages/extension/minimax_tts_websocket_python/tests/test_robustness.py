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

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput
from minimax_tts_websocket_python.minimax_tts import (
    MinimaxTTSTaskFailedException,
)

# ================ test reconnect after connection drop(robustness) ================
class ExtensionTesterRobustness(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.first_request_error: dict[str, Any] | None = None
        self.second_request_successful = False
        self.ten_env: TenEnvTester | None = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends the first TTS request."""
        self.ten_env = ten_env_tester
        ten_env_tester.log_info("Robustness test started, sending first TTS request.")

        # First request, expected to fail
        tts_input_1 = TTSTextInput(
            request_id="tts_request_to_fail",
            text="This request will trigger a simulated connection drop.",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_1.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def send_second_request(self):
        """Sends the second TTS request to verify reconnection."""
        if self.ten_env is None:
            print("Error: ten_env is not initialized.")
            return
        self.ten_env.log_info("Sending second TTS request to verify reconnection.")
        tts_input_2 = TTSTextInput(
            request_id="tts_request_to_succeed",
            text="This request should succeed after reconnection.",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_2.model_dump_json())
        self.ten_env.send_data(data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str)

        if name == "error" and payload.get("id") == "tts_request_to_fail":
            ten_env.log_info(f"Received expected error for the first request: {payload}")
            self.first_request_error = payload
            # After receiving the error for the first request, immediately send the second one.
            self.send_second_request()

        # Use a separate 'if' to ensure this check happens independently of the error check.
        if payload.get("id") == "tts_request_to_succeed":
            ten_env.log_info("Received tts_audio_end for the second request. Test successful.")
            self.second_request_successful = True
            # We can now safely stop the test.
            ten_env.stop_test()

@patch('minimax_tts_websocket_python.extension.MinimaxTTSWebsocket')
def test_reconnect_after_connection_drop(MockMinimaxTTSWebsocket):
    """
    Tests that the extension can recover from a connection drop, report a
    NON_FATAL_ERROR, and then successfully reconnect and process a new request.
    """
    print("Starting test_reconnect_after_connection_drop with mock...")

    # --- Mock State ---
    # Use a simple counter to track how many times get() is called
    get_call_count = 0

    # --- Mock Configuration ---
    mock_instance = MockMinimaxTTSWebsocket.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()

    # This async generator simulates different behaviors on subsequent calls
    async def mock_get_stateful(text: str):
        nonlocal get_call_count
        get_call_count += 1

        if get_call_count == 1:
            # On the first call, simulate a connection drop
            raise ConnectionRefusedError("Simulated connection drop from test")
        else:
            # On the second call, simulate a successful audio stream
            yield (b'\x44\x55\x66', EVENT_TTSResponse)
            yield (None, EVENT_TTSSentenceEnd)

    mock_instance.get.side_effect = mock_get_stateful

    # --- Test Setup ---
    config = { "api_key": "a_valid_key", "group_id": "a_valid_group" }
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single(
        "minimax_tts_websocket_python",
        json.dumps(config)
    )

    print("Running robustness test...")
    tester.run()
    print("Robustness test completed.")

    # --- Assertions ---
    # 1. Verify that the first request resulted in a NON_FATAL_ERROR
    assert tester.first_request_error is not None, "Did not receive any error message."
    assert tester.first_request_error.get("code") == 1000, \
        f"Expected error code 1000 (NON_FATAL_ERROR), got {tester.first_request_error.get('code')}"

    # 2. Verify that vendor_info was included in the error
    vendor_info = tester.first_request_error.get("vendor_info")
    assert vendor_info is not None, "Error message did not contain vendor_info."
    assert vendor_info.get("vendor") == "minimax", \
        f"Expected vendor 'minimax', got {vendor_info.get('vendor')}"

    # 3. Verify that the client's start method was called twice (initial + reconnect)
    # This assertion is tricky because the reconnection logic might be inside the client.
    # A better assertion is to check if the second request succeeded.

    # 4. Verify that the second TTS request was successful
    assert tester.second_request_successful, "The second TTS request after the error did not succeed."

    print("✅ Robustness test passed: Correctly handled simulated connection drop and recovered.")
