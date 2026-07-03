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
import asyncio
from typing import Any
from unittest.mock import AsyncMock, MagicMock, patch

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput


# ================ test robustness ================
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

        ten_env.log_info(
            f"DEBUG: Received event '{name}' with payload: {payload}"
        )

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
            ten_env.log_info("Received tts_audio_end for the second request.")
            self.second_request_successful = True
            ten_env.stop_test()

        elif name == "tts_audio_end":
            if payload.get("id") == "tts_request_to_succeed":
                self.second_request_successful = True
                ten_env.stop_test()


@patch("cartesia_tts.extension.CartesiaTTSClient")
def test_reconnect_after_connection_drop(MockCartesiaTTSClient):
    """
    Tests that the extension can recover from a connection drop, report a
    NON_FATAL_ERROR, and then successfully reconnect and process a new request.

    In the full-duplex architecture, text_to_speech() puts text into a queue.
    We simulate a failure on the first call and success on the second.
    """
    print("Starting test_reconnect_after_connection_drop with mock...")

    mock_instance = MagicMock()
    pcm_queue = asyncio.Queue()
    words_queue = asyncio.Queue()

    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.set_current_request_id = AsyncMock()
    mock_instance.send_audio_end_signal = AsyncMock()

    tts_call_count = 0

    async def mock_tts(t):
        nonlocal tts_call_count
        tts_call_count += 1
        request_id = t.request_id

        if tts_call_count == 1:
            # First call: simulate connection drop
            raise ConnectionRefusedError("Simulated connection drop from test")
        else:
            # Subsequent calls: stream audio normally
            async def _stream():
                await asyncio.sleep(0.01)
                await pcm_queue.put((b"\x44\x55\x66", request_id, 0))
                await asyncio.sleep(0.01)
                await pcm_queue.put((None, request_id, 0))

            asyncio.create_task(_stream())

    mock_instance.text_to_speech = AsyncMock(side_effect=mock_tts)

    async def mock_get_audio():
        return await pcm_queue.get()

    mock_instance.get_audio = AsyncMock(side_effect=mock_get_audio)

    async def mock_get_words():
        return await words_queue.get()

    mock_instance.get_words = AsyncMock(side_effect=mock_get_words)

    MockCartesiaTTSClient.return_value = mock_instance

    config = {"params": {"api_key": "a_valid_key"}}
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single("cartesia_tts", json.dumps(config))

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
        vendor_info.get("vendor") == "cartesia"
    ), f"Expected vendor 'cartesia', got {vendor_info.get('vendor')}"

    assert (
        tester.second_request_successful
    ), "The second TTS request did not succeed."

    print(
        "✅ Robustness test passed: Correctly handled connection drop and recovered."
    )
