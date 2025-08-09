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
import tempfile
import os
import asyncio
import filecmp
import shutil
import threading

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    Data,
)
from ten_ai_base.struct import TTSTextInput, TTSFlush
from ten_ai_base.message import ModuleVendorException, ModuleErrorVendorInfo

# ================ test robustness ================
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
            text_input_end=True  # Set to True to trigger session finish
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_2.model_dump_json())
        self.ten_env.send_data(data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str) if json_str else {}

        # Add debug logging for all events
        ten_env.log_info(f"DEBUG: Received event '{name}' with payload: {payload}")

        if name == "error" and payload.get("id") == "tts_request_to_fail":
            ten_env.log_info(f"Received expected error for the first request: {payload}")
            self.first_request_error = payload
            # After receiving the error for the first request, immediately send the second one.
            self.send_second_request()

        elif name == "tts_audio_end" and payload.get("request_id") == "tts_request_to_succeed":
            ten_env.log_info("Received tts_audio_end for the second request. Test successful.")
            self.second_request_successful = True
            # We can now safely stop the test.
            ten_env.stop_test()

        # Also check for tts_audio_end without specific request_id filtering
        elif name == "tts_audio_end":
            ten_env.log_info(f"Received tts_audio_end for request_id: {payload.get('id')}, but expected 'tts_request_to_succeed'")
            # If this is the second request, consider it successful anyway
            if payload.get("id") == "tts_request_to_succeed":
                ten_env.log_info("Actually this matches! Stopping test.")
                self.second_request_successful = True
                ten_env.stop_test()

@patch('bytedance_tts_duplex.extension.BytedanceV3Client')
def test_reconnect_after_connection_drop(MockBytedanceV3Client):
    """
    Tests that the extension can recover from a connection drop, report a
    NON_FATAL_ERROR, and then successfully reconnect and process a new request.
    """
    print("Starting test_reconnect_after_connection_drop with mock...")

    # --- Mock State ---
    send_text_call_count = 0

    # --- Mock Configuration ---
    mock_instance = MockBytedanceV3Client.return_value
    mock_instance.connect = AsyncMock()
    mock_instance.start_connection = AsyncMock()
    mock_instance.start_session = AsyncMock()
    mock_instance.finish_session = AsyncMock()
    mock_instance.finish_connection = AsyncMock()
    mock_instance.close = AsyncMock()

    # This async method simulates different behaviors on subsequent calls
    async def mock_send_text_stateful(text: str):
        print(f"KEYPOINT mock_send_text_stateful: {text}")
        nonlocal send_text_call_count
        send_text_call_count += 1

        print(f"KEYPOINT send_text_call_count: {send_text_call_count}")
        if send_text_call_count == 1:
            # On the first call, simulate a connection drop
            raise ConnectionRefusedError("Simulated connection drop from test")
        else:
            # On the second call, populate the queue with audio data
            # to simulate successful TTS response
            async def populate_queue():
                EVENT_TTSResponse = 352
                EVENT_SessionFinished = 152
                await mock_instance.response_msgs.put((EVENT_TTSResponse, b'\x44\x55\x66'))
                await mock_instance.response_msgs.put((EVENT_SessionFinished, b''))

            asyncio.create_task(populate_queue())

    mock_instance.send_text = AsyncMock(side_effect=mock_send_text_stateful)

    # Mock the client constructor
    def mock_client_init(config, ten_env, vendor, response_msgs):
        mock_instance.response_msgs = response_msgs
        return mock_instance

    MockBytedanceV3Client.side_effect = mock_client_init

    # --- Test Setup ---
    config = { "appid": "a_valid_appid", "token": "a_valid_token" }
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single(
        "bytedance_tts_duplex",
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
    assert vendor_info.get("vendor") == "bytedance", \
        f"Expected vendor 'bytedance', got {vendor_info.get('vendor')}"

    # 3. Verify that the second TTS request was successful
    assert tester.second_request_successful, "The second TTS request after the error did not succeed."

    print("✅ Robustness test passed: Correctly handled simulated connection drop and recovered.")