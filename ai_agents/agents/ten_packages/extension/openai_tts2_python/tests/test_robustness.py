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
from unittest.mock import MagicMock, AsyncMock, patch

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput, TTS2HttpResponseEventType


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
            text_input_end=True,  # Set to True so error handler can properly finish request
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
            text_input_end=True,  # Set to True to trigger session finish
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_2.model_dump_json())
        self.ten_env.send_data(data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str) if json_str else {}

        # Add debug logging for all events
        ten_env.log_info(
            f"DEBUG: Received event '{name}' with payload: {payload}"
        )

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

        # Also check for tts_audio_end without specific request_id filtering
        elif name == "tts_audio_end":
            ten_env.log_info(
                f"Received tts_audio_end for request_id: {payload.get('id')}, but expected 'tts_request_to_succeed'"
            )
            # If this is the second request, consider it successful anyway
            if payload.get("id") == "tts_request_to_succeed":
                ten_env.log_info("Actually this matches! Stopping test.")
                self.second_request_successful = True
                ten_env.stop_test()


@patch("openai_tts2_python.openai_tts.Limits")
@patch("openai_tts2_python.openai_tts.Timeout")
@patch("openai_tts2_python.openai_tts.AsyncClient")
def test_reconnect_after_connection_drop(
    MockAsyncClient, MockTimeout, MockLimits
):
    """
    Tests that the extension can recover from a connection drop, report a
    NON_FATAL_ERROR, and then successfully reconnect and process a new request.
    """
    print("Starting test_reconnect_after_connection_drop with mock...")

    # --- Mock State ---
    # Use a simple counter to track how many times stream() is called
    stream_call_count = 0

    # --- Mock Configuration ---
    def create_mock_response(*args, **kwargs):
        """Create mock response for httpx.AsyncClient.stream() calls."""
        nonlocal stream_call_count
        stream_call_count += 1

        if stream_call_count == 1:
            # First call: simulate connection drop
            # httpx.stream() returns an async context manager
            # When entering the context, raise ConnectionRefusedError
            mock_context = AsyncMock()

            async def raise_error():
                raise ConnectionRefusedError(
                    "Simulated connection drop from test"
                )

            mock_context.__aenter__ = raise_error
            mock_context.__aexit__ = AsyncMock(return_value=None)
            return mock_context
        else:
            # Second call: successful response
            mock_response = AsyncMock()
            mock_response.status_code = 200

            # Mock aread() for error body reading (not used in success case)
            mock_response.aread = AsyncMock(return_value=b"")

            # Mock aiter_bytes() for streaming audio data
            async def mock_aiter_bytes():
                yield b"\x44\x55\x66"

            mock_response.aiter_bytes = mock_aiter_bytes

            # __aenter__ should be an async function that returns the response itself
            async def mock_aenter():
                return mock_response

            mock_response.__aenter__ = mock_aenter
            mock_response.__aexit__ = AsyncMock(return_value=None)

            # Return a context manager that yields the mock_response
            mock_context = AsyncMock()
            mock_context.__aenter__ = mock_aenter
            mock_context.__aexit__ = AsyncMock(return_value=None)
            return mock_context

    # Mock Timeout and Limits to avoid issues
    MockTimeout.return_value = MagicMock()
    MockLimits.return_value = MagicMock()

    mock_client = AsyncMock()
    # httpx.AsyncClient.stream() is called with method, url, headers, json
    mock_client.stream = MagicMock(side_effect=create_mock_response)
    mock_client.aclose = AsyncMock()
    MockAsyncClient.return_value = mock_client

    # --- Test Setup ---
    config = {
        "params": {"api_key": "a_valid_key"},
    }
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single("openai_tts2_python", json.dumps(config))

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
        vendor_info.get("vendor") == "openai"
    ), f"Expected vendor 'openai', got {vendor_info.get('vendor')}"

    # 3. Verify that stream() was called twice (initial + reconnect)
    assert (
        stream_call_count == 2
    ), f"Expected stream() to be called twice, but was called {stream_call_count} times"

    # 4. Verify that the second TTS request was successful
    assert (
        tester.second_request_successful
    ), "The second TTS request after the error did not succeed."

    print(
        "✅ Robustness test passed: Correctly handled simulated connection drop and recovered."
    )
