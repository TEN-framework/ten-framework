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
    TenError,
)

# ================ test params passthrough ================
class ExtensionTesterForPassthrough(ExtensionTester):
    """A simple tester that just starts and stops, to allow checking constructor calls."""

    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
            return
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            # TODO: move stop_test() to where the test passes
            ten_env.stop_test()

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        print("send hello_world")
        ten_env_tester.send_cmd(
            new_cmd,
            lambda ten_env, result, _: self.check_hello(ten_env, result),
        )

        print("tester on_start_done")
        ten_env_tester.on_start_done()

@patch('minimax_tts_websocket_python.extension.MinimaxTTSWebsocket')
def test_params_passthrough(MockMinimaxTTSWebsocket):
    """
    Tests that custom parameters passed in the configuration are correctly
    forwarded to the MinimaxTTSWebsocket client constructor.
    """
    print("Starting test_params_passthrough with mock...")

    # --- Mock Configuration ---
    mock_instance = MockMinimaxTTSWebsocket.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock() # Required for clean shutdown in on_stop

    # --- Test Setup ---
    # Define a configuration with custom, arbitrary parameters inside 'params'.
    # These are the parameters we expect to be "passed through".
    passthrough_params = {
        "model": "tts_v2",
        "audio_setting": {
            "format": "pcm",
            "sample_rate": 16000,
            "channels": 1
        },
        "voice_setting": {
            "voice_id": "male-qn-qingse"
        }
    }
    passthrough_config = {
        "api_key": "a_valid_key",
        "group_id": "a_valid_group",
        "params": passthrough_params
    }

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single(
        "minimax_tts_websocket_python",
        json.dumps(passthrough_config)
    )

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    # --- Assertions ---
    # Check that the MinimaxTTSWebsocket client was instantiated exactly once.
    MockMinimaxTTSWebsocket.assert_called_once()

    # Get the arguments that the mock was called with.
    # The constructor signature is (self, config, ten_env, vendor),
    # so we inspect the 'config' object at index 1 of the call arguments.
    call_args, call_kwargs = MockMinimaxTTSWebsocket.call_args
    called_config = call_args[0]

    # Verify that the 'params' dictionary in the config object passed to the
    # client constructor is identical to the one we defined in our test config.
    assert called_config.params == passthrough_params, \
        f"Expected params to be {passthrough_params}, but got {called_config.params}"

    print("✅ Params passthrough test passed successfully.")
    print(f"✅ Verified params: {called_config.params}")
