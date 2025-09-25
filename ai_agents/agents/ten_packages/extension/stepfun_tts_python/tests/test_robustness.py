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
from stepfun_tts_python.stepfun_tts import (
    EVENT_TTSTaskFinished,
    EVENT_TTSResponse,
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
        ten_env_tester.log_info(
            "Robustness test started, sending first TTS request."
        )

        # First request, expected to fail
        tts_input_1 = TTSTextInput(
            request_id="tts_request_to_fail",
            text="This request will trigger a simulated connection drop.",
            text_input_end=True,
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
        self.ten_env.log_info(
            "Sending second TTS request to verify reconnection."
        )
        tts_input_2 = TTSTextInput(
            request_id="tts_request_to_succeed",
            text="This request should succeed after reconnection.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_2.model_dump_json())
        self.ten_env.send_data(data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str)

        if name == "error" and payload.get("id") == "tts_request_to_fail":
            ten_env.log_info(
                f"Received expected error for the first request: {payload}"
            )
            self.first_request_error = payload
            # After receiving the error for the first request, immediately send the second one.
            self.send_second_request()

        elif (
            name == "tts_audio_end"
            and payload.get("request_id") == "tts_request_to_succeed"
        ):
            ten_env.log_info(
                "Received tts_audio_end for the second request. Test successful."
            )
            self.second_request_successful = True
            # We can now safely stop the test.
            ten_env.stop_test()


@patch("stepfun_tts_python.extension.StepFunTTSWebsocket")
def test_reconnect_after_connection_drop(MockStepFunTTSWebsocket):
    """
    Tests that the extension can recover from a connection drop, report a
    NON_FATAL_ERROR, and then successfully reconnect and process a new request.
    """
    print("Starting test_reconnect_after_connection_drop with mock...")

    # --- Mock State ---
    # Use a simple counter to track how many times get() is called
    get_call_count = 0

    # --- Mock Configuration ---
    mock_instance = MockStepFunTTSWebsocket.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.cancel = AsyncMock()  # Ensure cancel is awaitable

    # Store the callback that will be set by the extension
    robustness_audio_callback = None

    def capture_robustness_callback(*args, **kwargs):
        # Capture the on_audio_data callback from the keyword arguments
        nonlocal robustness_audio_callback
        if "on_audio_data" in kwargs:
            robustness_audio_callback = kwargs["on_audio_data"]
        return mock_instance

    MockStepFunTTSWebsocket.side_effect = capture_robustness_callback

    # Mock the callback-based audio handling for StepFun TTS with stateful behavior
    async def mock_get_stateful(tts_input):
        nonlocal get_call_count
        get_call_count += 1

        if get_call_count == 1:
            # On the first call, simulate a connection drop
            raise ConnectionRefusedError("Simulated connection drop from test")
        else:
            # Wait a bit to ensure callback is captured
            await asyncio.sleep(0.01)

            if robustness_audio_callback:
                # On the second call, simulate a successful audio stream via callbacks
                # Simulate sentence start
                await robustness_audio_callback(
                    b"", 350, 0
                )  # EVENT_TTSSentenceStart

                # Simulate audio chunk
                await robustness_audio_callback(
                    b"\x44\x55\x66", EVENT_TTSResponse, 0
                )

                # Simulate sentence end
                await robustness_audio_callback(b"", EVENT_TTSTaskFinished, 0)

    mock_instance.get = AsyncMock(side_effect=mock_get_stateful)

    # --- Test Setup ---
    config = {
        "params": {
            "api_key": "a_valid_key",
            "model": "step-tts-mini",
            "voice_id": "cixingnansheng",
            "sample_rate": 24000,
        }
    }
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single("stepfun_tts_python", json.dumps(config))

    print("Running robustness test...")
    tester.run()
    print("Robustness test completed.")

    # --- Assertions ---
    # 1. Verify that the first request resulted in a NON_FATAL_ERROR
    assert (
        tester.first_request_error is not None
    ), "Did not receive any error message."
    assert (
        tester.first_request_error.get("code") == 1000
    ), f"Expected error code 1000 (NON_FATAL_ERROR), got {tester.first_request_error.get('code')}"

    # 2. Verify that vendor_info was included in the error
    vendor_info = tester.first_request_error.get("vendor_info")
    assert vendor_info is not None, "Error message did not contain vendor_info."
    assert (
        vendor_info.get("vendor") == "stepfun"
    ), f"Expected vendor 'stepfun', got {vendor_info.get('vendor')}"

    # 3. Verify that the client's start method was called twice (initial + reconnect)
    # This assertion is tricky because the reconnection logic might be inside the client.
    # A better assertion is to check if the second request succeeded.

    # 4. Verify that the second TTS request was successful
    assert (
        tester.second_request_successful
    ), "The second TTS request after the error did not succeed."

    print(
        "✅ Robustness test passed: Correctly handled simulated connection drop and recovered."
    )
