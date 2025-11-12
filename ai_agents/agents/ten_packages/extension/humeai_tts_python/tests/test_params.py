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
from pathlib import Path
import json
from typing import Any
from unittest.mock import patch, AsyncMock, MagicMock
import tempfile
import os
import asyncio
import filecmp
import shutil
import threading
import base64

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    Data,
    TenError,
)
from ten_ai_base.struct import TTSTextInput, TTS2HttpResponseEventType


# ================ test params passthrough ================
class ExtensionTesterForPassthrough(ExtensionTester):
    """A simple tester that just starts and stops, to allow checking constructor calls."""

    def __init__(self):
        super().__init__()
        self.tts_completed = False

    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
            return
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            # Send a simple TTS request to ensure client initialization
            tts_input = TTSTextInput(
                request_id="passthrough_test",
                text="test",
                text_input_end=True,
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, tts_input.model_dump_json())
            ten_env.send_data(data)

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        print("send hello_world")
        ten_env_tester.send_cmd(
            new_cmd,
            lambda ten_env, result, _: self.check_hello(ten_env, result),
        )

        print("tester on_start_done")
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end" and not self.tts_completed:
            self.tts_completed = True
            ten_env.stop_test()


@patch("humeai_tts_python.extension.HumeAiTTS")
def test_params_passthrough(MockHumeAiTTS):
    """
    Tests that custom parameters passed in the configuration are correctly
    forwarded to the HumeAiTTS client constructor.
    """
    print("Starting test_params_passthrough with mock...")

    # --- Mock Configuration ---
    mock_instance = MockHumeAiTTS.return_value
    mock_instance.cancel = (
        AsyncMock()
    )  # Required for clean shutdown in on_flush
    mock_instance.clean = AsyncMock()

    async def mock_get_audio_stream(text: str, request_id: str):
        yield (b"\x11\x22\x33", TTS2HttpResponseEventType.RESPONSE)
        yield (None, TTS2HttpResponseEventType.END)

    mock_instance.get.side_effect = mock_get_audio_stream

    # --- Test Setup ---
    # Define a configuration with custom parameters inside 'params'.
    passthrough_config = {
        "params": {
            "key": "test_api_key",
            "voice_name": "Female English Actor",
            "speed": 1.5,
            "trailing_silence": 0.8,
        },
    }

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single(
        "humeai_tts_python", json.dumps(passthrough_config)
    )

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    # --- Assertions ---
    # Check that the HumeAiTTS client was instantiated exactly once.
    MockHumeAiTTS.assert_called_once()

    # Get the arguments that the mock was called with.
    # The constructor is called with keyword arguments like config=...
    # so we inspect the keyword arguments dictionary.
    call_args, call_kwargs = MockHumeAiTTS.call_args
    called_config = call_kwargs["config"]

    # Verify that the params dictionary contains our expected parameters
    assert (
        "speed" in called_config.params
    ), "Config params should have speed parameter"
    assert (
        called_config.params["speed"] == 1.5
    ), f"Expected speed to be 1.5, but got {called_config.params['speed']}"
    assert (
        "trailing_silence" in called_config.params
    ), "Config params should have trailing_silence parameter"
    assert (
        called_config.params["trailing_silence"] == 0.8
    ), f"Expected trailing_silence to be 0.8, but got {called_config.params['trailing_silence']}"

    print("✅ Params passthrough test passed successfully.")
    print(
        f"✅ Verified params speed: {called_config.params['speed']}, trailing_silence: {called_config.params['trailing_silence']}"
    )
