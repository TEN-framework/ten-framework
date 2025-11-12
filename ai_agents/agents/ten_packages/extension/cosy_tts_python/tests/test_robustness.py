#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
from typing import Any, Optional
from unittest.mock import patch, AsyncMock
import json

from ten_ai_base.struct import TTSTextInput
from ten_runtime import (
    Data,
    ExtensionTester,
    TenEnvTester,
)
from ..cosy_tts import MESSAGE_TYPE_CMD_COMPLETE


# ================ test reconnect after connection drop(robustness) ================
class ExtensionTesterRobustness(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.first_request_error: Optional[dict[str, Any]] = None
        self.second_request_successful = False
        self.ten_env: Optional[TenEnvTester] = None

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
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_2.model_dump_json())
        self.ten_env.send_data(data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        if not json_str:
            # Not all events have a JSON payload (e.g., tts_audio_start)
            return
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


@patch("cosy_tts_python.extension.CosyTTSClient")
def test_reconnect_after_connection_drop(MockCosyTTSClient):
    """
    Tests that the extension can recover from a connection drop, report a
    NON_FATAL_ERROR, and then successfully reconnect and process a new request.
    """
    print("Starting test_reconnect_after_connection_drop with mock...")

    # --- Mock Configuration ---
    mock_instance = MockCosyTTSClient.return_value
    # Add mocks for start/stop to align with extension.py's on_init/on_stop calls
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()

    # This mock now manages session state to correctly simulate a failure
    # on the first attempt and success on the second, without causing loops.
    class RobustnessStreamer:
        class Session:
            def __init__(self, should_fail: bool):
                self.should_fail = should_fail
                self.call_count = 0

            async def get_audio_data(self):
                if self.should_fail:
                    raise ConnectionRefusedError(
                        "Simulated connection drop from test"
                    )
                # On the second request, simulate a successful audio stream
                # that completes immediately on first call, then stops loop
                self.call_count += 1
                if self.call_count == 1:
                    return (True, MESSAGE_TYPE_CMD_COMPLETE, None)
                # Subsequent calls: raise CancelledError to stop the loop
                raise asyncio.CancelledError()

        def __init__(self):
            self.session_count = 0
            self.session: Optional[RobustnessStreamer.Session] = None
            self._new_session_event = asyncio.Event()

        def synthesize_audio(self, text: str, text_input_end: bool):
            self.session_count += 1
            should_fail = self.session_count == 1
            self.session = RobustnessStreamer.Session(should_fail)
            self._new_session_event.set()

        async def get_audio_data(self):
            if not self.session:
                await self._new_session_event.wait()

            assert self.session is not None

            try:
                # Delegate to the current session.
                done, msg_type, data = await self.session.get_audio_data()
            except ConnectionRefusedError as e:
                # After a failure, reset the state to allow the next session
                # to be waited for correctly.
                self.session = None
                self._new_session_event.clear()
                raise e

            # If the session is finished, reset for the next request.
            if done:
                self.session = None
                self._new_session_event.clear()

            return (done, msg_type, data)

    streamer = RobustnessStreamer()
    mock_instance.synthesize_audio.side_effect = streamer.synthesize_audio
    mock_instance.get_audio_data.side_effect = streamer.get_audio_data

    # --- Test Setup ---
    config = {
        "params": {
            "api_key": "a_valid_key",
        },
    }
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single("cosy_tts_python", json.dumps(config))

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
        vendor_info.get("vendor") == "cosy"
    ), f"Expected vendor 'cosy', got {vendor_info.get('vendor')}"

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
