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
                ten_env.log_info(f"Received TTFB metric with value: {self.ttfb_value}")

        elif name == "tts_audio_end":
            self.audio_end_received = True
            # Stop the test only after both TTFB and audio end are received
            if self.ttfb_received:
                ten_env.log_info("Received tts_audio_end, stopping test.")
                ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receives audio frames and confirms the stream is working."""
        if not self.audio_frame_received:
             self.audio_frame_received = True
             ten_env.log_info("First audio frame received.")

@patch('bytedance_tts_duplex.extension.BytedanceV3Client')
def test_ttfb_metric_is_sent(MockBytedanceV3Client):
    """
    Tests that a TTFB (Time To First Byte) metric is correctly sent after
    receiving the first audio chunk from the TTS service.
    """
    print("Starting test_ttfb_metric_is_sent with mock...")

    # --- Mock Configuration ---
    mock_instance = MockBytedanceV3Client.return_value
    mock_instance.connect = AsyncMock()
    mock_instance.start_connection = AsyncMock()
    mock_instance.start_session = AsyncMock()
    mock_instance.send_text = AsyncMock()
    mock_instance.finish_session = AsyncMock()
    mock_instance.finish_connection = AsyncMock()
    mock_instance.close = AsyncMock()

    # Mock the client constructor to handle the response queue
    def mock_client_init(config, ten_env, vendor, response_msgs):
        mock_instance.response_msgs = response_msgs

        async def populate_queue():
            EVENT_TTSResponse = 352
            EVENT_SessionFinished = 152

            # Simulate network latency before the first byte
            await asyncio.sleep(0.2)

            await response_msgs.put((EVENT_TTSResponse, b'\x11\x22\x33'))
            await response_msgs.put((EVENT_SessionFinished, b''))

        asyncio.create_task(populate_queue())
        return mock_instance

    MockBytedanceV3Client.side_effect = mock_client_init

    # --- Test Setup ---
    metrics_config = {
        "appid": "a_valid_appid",
        "token": "a_valid_token",
    }
    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single(
        "bytedance_tts_duplex",
        json.dumps(metrics_config)
    )

    print("Running TTFB metrics test...")
    tester.run()
    print("TTFB metrics test completed.")

    # --- Assertions ---
    assert tester.audio_frame_received, "Did not receive any audio frame."
    assert tester.audio_end_received, "Did not receive the tts_audio_end event."
    assert tester.ttfb_received, "TTFB metric was not received."

    # Check if the TTFB value is reasonable.
    # It should be slightly more than the 0.2s delay we introduced.
    print(f"TTFB value: {tester.ttfb_value}")
    assert tester.ttfb_value >= 200, \
        f"Expected TTFB to be >= 200ms, but got {tester.ttfb_value}ms."

    print(f"✅ TTFB metric test passed. Received TTFB: {tester.ttfb_value}ms.")