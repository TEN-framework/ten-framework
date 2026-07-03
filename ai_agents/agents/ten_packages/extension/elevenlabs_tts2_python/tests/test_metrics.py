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


# ================ test metrics ================
class ExtensionTesterMetrics(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ttfb_received = False
        self.ttfb_value = -1
        self.audio_frame_received = False
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info("Metrics test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_for_metrics",
            text="hello, this is a metrics test.",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")
        if name == "metrics":
            json_str, _ = data.get_property_to_json(None)
            ten_env.log_info(f"Received metrics: {json_str}")
            metrics_data = json.loads(json_str)

            # According to the new structure, 'ttfb' is nested inside a 'metrics' object.
            nested_metrics = metrics_data.get("metrics", {})
            if "ttfb" in nested_metrics:
                self.ttfb_received = True
                self.ttfb_value = nested_metrics.get("ttfb", -1)
                ten_env.log_info(
                    f"Received TTFB metric with value: {self.ttfb_value}"
                )

        elif name == "tts_audio_end":
            self.audio_end_received = True
            # Stop the test only after both TTFB and audio end are received
            ten_env.log_info("Received tts_audio_end, stopping test.")
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receives audio frames and confirms the stream is working."""
        if not self.audio_frame_received:
            self.audio_frame_received = True
            ten_env.log_info("First audio frame received.")


@patch("elevenlabs_tts2_python.elevenlabs_tts.ElevenLabsTTS2Client")
def test_ttfb_metric_is_sent(MockElevenLabsTTS2Client):
    """
    Tests that a TTFB (Time To First Byte) metric is correctly sent after
    receiving the first audio chunk from the TTS service.
    """
    print("Starting test_ttfb_metric_is_sent with mock...")

    # --- Mock Configuration ---
    mock_instance = MockElevenLabsTTS2Client.return_value
    mock_instance.start_connection = AsyncMock()
    mock_instance.text_to_speech_ws_streaming = AsyncMock()
    mock_instance.close = AsyncMock()

    # Mock the client constructor to handle the response queue
    def mock_client_init(*args, **kwargs):
        # Get the response_msgs queue from the client
        response_msgs = mock_instance.response_msgs = AsyncMock()

        async def populate_queue():
            # Simulate network latency before the first byte
            await asyncio.sleep(0.2)

            # Put audio data and final response in the queue
            await response_msgs.put((b"\x11\x22\x33", False, "", 123))
            await response_msgs.put(
                (b"\x44\x55\x66", True, "hello, this is a metrics test.", None)
            )

        # Set up the queue get method
        call_count = 0

        async def mock_get():
            nonlocal call_count
            call_count += 1
            if call_count == 1:
                # Simulate network latency before the first byte
                await asyncio.sleep(0.2)
                return (b"\x11\x22\x33", False, "")
            elif call_count == 2:
                return (b"\x44\x55\x66", True, "hello, this is a metrics test.")
            else:
                # Keep the connection alive or simulate more data
                await asyncio.sleep(0.1)
                return (b"", True, "")

        response_msgs.get = mock_get
        return mock_instance

    MockElevenLabsTTS2Client.side_effect = mock_client_init

    # --- Test Setup ---
    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single(
        "elevenlabs_tts2_python",
        json.dumps(
            {
                "params": {
                    "key": "valid_api_key",
                },
            }
        ),
    )

    print("Running TTFB metrics test...")
    tester.run()
    print("TTFB metrics test completed.")

    # --- Assertions ---
    assert tester.audio_frame_received, "Did not receive any audio frame."
    assert tester.audio_end_received, "Did not receive the tts_audio_end event."
    assert tester.ttfb_received, "TTFB metric was not received."
    assert tester.ttfb_value == 123, "TTFB value is not 123."

    # Check if the TTFB value is reasonable.
    # It should be slightly more than the 0.2s delay we introduced.
    print(f"TTFB value: {tester.ttfb_value}")

    print(f"✅ TTFB metric test passed. Received TTFB: {tester.ttfb_value}ms.")
