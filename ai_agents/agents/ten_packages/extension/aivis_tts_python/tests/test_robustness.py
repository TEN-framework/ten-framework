#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import sys
from pathlib import Path

project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

import json
from typing import Any
from unittest.mock import AsyncMock, patch

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput, TTS2HttpResponseEventType


class ExtensionTesterRobustness(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.first_request_error: dict[str, Any] | None = None
        self.second_request_successful = False
        self.ten_env: TenEnvTester | None = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        self.ten_env = ten_env_tester
        ten_env_tester.log_info(
            "Robustness test started, sending first TTS request."
        )

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
        if self.ten_env is None:
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
        payload = json.loads(json_str) if json_str else {}

        if name == "error" and payload.get("id") == "tts_request_to_fail":
            ten_env.log_info(
                f"Received expected error for the first request: {payload}"
            )
            self.first_request_error = payload
            self.send_second_request()

        elif (
            name == "tts_audio_end"
            and payload.get("request_id") == "tts_request_to_succeed"
        ):
            self.second_request_successful = True
            ten_env.stop_test()

        elif name == "tts_audio_end":
            if payload.get("id") == "tts_request_to_succeed":
                self.second_request_successful = True
                ten_env.stop_test()


@patch("aivis_tts_python.extension.AivisTTSClient")
def test_reconnect_after_connection_drop(MockAivisTTSClient):
    """Tests that the extension recovers from a connection drop and processes a second request."""
    print("Starting test_reconnect_after_connection_drop with mock...")

    get_call_count = 0

    mock_instance = MockAivisTTSClient.return_value
    mock_instance.clean = AsyncMock()

    async def mock_get_stateful(text: str, request_id: str | None = None):
        nonlocal get_call_count
        get_call_count += 1

        if get_call_count == 1:
            raise ConnectionRefusedError("Simulated connection drop from test")
        else:
            yield (b"\x44\x55\x66", TTS2HttpResponseEventType.RESPONSE)
            yield (None, TTS2HttpResponseEventType.END)

    mock_instance.get.side_effect = mock_get_stateful

    config = {
        "params": {
            "api_key": "a_valid_key",
            "model_uuid": "a59cb814-0083-4369-8542-f51a29e72af7",
        },
    }
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single("aivis_tts_python", json.dumps(config))

    print("Running robustness test...")
    tester.run()
    print("Robustness test completed.")

    assert (
        tester.first_request_error is not None
    ), "Did not receive any error message."
    assert (
        tester.first_request_error.get("code") == 1000
    ), f"Expected error code 1000 (NON_FATAL_ERROR), got {tester.first_request_error.get('code')}"

    vendor_info = tester.first_request_error.get("vendor_info")
    assert vendor_info is not None, "Error message did not contain vendor_info."
    assert (
        vendor_info.get("vendor") == "aivis"
    ), f"Expected vendor 'aivis', got {vendor_info.get('vendor')}"

    assert (
        tester.second_request_successful
    ), "The second TTS request after the error did not succeed."

    print(
        "✅ Robustness test passed: Correctly handled simulated connection drop and recovered."
    )
