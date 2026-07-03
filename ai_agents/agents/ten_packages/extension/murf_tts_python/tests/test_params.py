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
from unittest.mock import patch, AsyncMock

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    Data,
)
from ten_ai_base.struct import TTSTextInput


# ================ test params passthrough ================
class ExtensionTesterForPassthrough(ExtensionTester):
    """A simple tester that just starts and stops, to allow checking constructor calls."""

    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test()
            return
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
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


@patch("murf_tts_python.extension.MurfTTSClient")
def test_params_passthrough(MockMurfTTSClient):
    """
    Tests that custom parameters passed in the configuration are correctly
    forwarded to the MurfTTSClient constructor.
    """
    print("Starting test_params_passthrough with mock...")

    # --- Mock Configuration ---
    mock_instance = MockMurfTTSClient.return_value
    mock_instance.send_text = AsyncMock()
    mock_instance.close = AsyncMock()

    # Mock the client constructor to properly handle the response_msgs queue
    def mock_client_init(config, ten_env, vendor, response_msgs):
        # Store the real queue passed by the extension
        mock_instance.response_msgs = response_msgs
        return mock_instance

    MockMurfTTSClient.side_effect = mock_client_init

    # --- Test Setup ---
    # Define a configuration with custom parameters inside 'params'.
    # These are the parameters we expect to be "passed through".
    real_params = {
        "api_key": "a_test_api_key",
        "base_url": "wss://example.com/murf",
        "sample_rate": 16000,
        "voiceId": "Natalie",
        "multiNativeLocale": "en-GB",
        "style": "Narration",
        "rate": 0.2,
        "pitch": -0.1,
        "variation": 1.1,
        "model": "GEN2",
    }

    real_config = {
        "params": real_params,
    }

    passthrough_params = {
        "voiceId": "Natalie",
        "multiNativeLocale": "en-GB",
        "style": "Narration",
        "rate": 0.2,
        "pitch": -0.1,
        "variation": 1.1,
    }

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single("murf_tts_python", json.dumps(real_config))

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    # --- Assertions ---
    # Check that the MurfTTSClient client was instantiated exactly once.
    MockMurfTTSClient.assert_called_once()

    # Get the arguments that the mock was called with.
    # The constructor is called with keyword arguments like config=...
    # so we inspect the keyword arguments dictionary.
    call_args, _ = MockMurfTTSClient.call_args
    called_config = call_args[0]

    # Verify that the 'params' dictionary in the config object passed to the
    # client constructor is identical to the one we defined in our test config.
    print(f"called_config: {called_config.params}")
    assert (
        called_config.params == passthrough_params
    ), f"Expected params to be {passthrough_params}, but got {called_config.params}"

    print("✅ Params passthrough test passed successfully.")
    print(f"✅ Verified params: {called_config.params}")
