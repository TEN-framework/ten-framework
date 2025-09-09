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


# ================ test params passthrough ================
class ExtensionTesterForPassthrough(ExtensionTester):
    """A tester that sends a TTS request to trigger client initialization."""

    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request to trigger client initialization."""
        ten_env_tester.log_info(
            "Params passthrough test started, sending TTS request."
        )

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end, stopping test.")
            self.audio_end_received = True
            ten_env.stop_test()


# ================ test default params ================
class ExtensionTesterDefaultParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.received_audio_chunks = []

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info(
            "Default params test started, sending TTS request."
        )

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end, stopping test.")
            self.audio_end_received = True
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receives audio frames and collects their data."""
        buf = audio_frame.lock_buf()
        try:
            copied_data = bytes(buf)
            self.received_audio_chunks.append(copied_data)
        finally:
            audio_frame.unlock_buf(buf)


@patch("elevenlabs_tts2_python.elevenlabs_tts.ElevenLabsTTS2Client")
def test_default_params(MockElevenLabsTTS2Client):
    """Test that the extension works with default parameters."""
    print("Starting test_default_params with mock...")

    # --- Mock Configuration ---
    mock_instance = MockElevenLabsTTS2Client.return_value
    mock_instance.start_connection = AsyncMock()
    mock_instance.text_to_speech_ws_streaming = AsyncMock()
    mock_instance.send_text = AsyncMock()
    mock_instance.close = AsyncMock()

    # Set up send_text to return immediately
    mock_instance.send_text.return_value = None

    # Mock the client constructor to properly handle the response_msgs queue
    def mock_client_init(
        config,
        ten_env,
        error_callback=None,
        response_msgs=None,
        ttfb_metrics_callback=None,
    ):
        # Use the real queue passed by the extension
        mock_instance.response_msgs = (
            response_msgs if response_msgs else asyncio.Queue()
        )

        # Populate the queue with mock data asynchronously
        async def populate_queue():
            await asyncio.sleep(0.01)  # Small delay to let the extension start
            await mock_instance.response_msgs.put(
                (b"fake_audio_data", True, "hello word, hello agora", 123)
            )

        # Start the population task
        asyncio.create_task(populate_queue())
        return mock_instance

    MockElevenLabsTTS2Client.side_effect = mock_client_init

    # --- Test Setup ---
    tester = ExtensionTesterDefaultParams()
    tester.set_test_mode_single(
        "elevenlabs_tts2_python",
        json.dumps(
            {
                "params": {"key": "valid_api_key_for_test"},
            }
        ),
    )

    print("Running default params test...")
    tester.run()
    print("Default params test completed.")

    # --- Assertions ---
    assert tester.audio_end_received, "Audio end event was not received"
    assert (
        len(tester.received_audio_chunks) > 0
    ), "No audio chunks were received"

    print("✅ Default params test passed successfully.")
