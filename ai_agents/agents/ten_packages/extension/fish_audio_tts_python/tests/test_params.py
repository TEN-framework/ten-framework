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
from unittest.mock import patch, MagicMock

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
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


@patch("fish_audio_tts_python.extension.FishAudioTTSClient")
def test_params_passthrough(MockFishAudioTTSClient):
    """
    Tests that custom parameters passed in the configuration are correctly
    forwarded to the FishAudioTTS client constructor.
    """
    print("Starting test_params_passthrough with mock...")

    # --- Mock Configuration ---
    mock_instance = MockFishAudioTTSClient.return_value
    mock_instance.clean = MagicMock()  # Required for clean shutdown in on_flush

    # --- Test Setup ---
    # Define a configuration with custom parameters inside 'params'.
    # These are the parameters we expect to be "passed through".
    passthrough_params = {
        "api_key": "test_api_key",
        "reference_id": "728f6ff2240d49308e8137ffe66008e2",
        "top_p": 0.7,
        "sample_rate": 16000,
        "temperature": 0.7,
    }
    passthrough_config = {
        "dump": False,
        "dump_path": "./dump/",
        "params": passthrough_params,
    }

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single(
        "fish_audio_tts_python", json.dumps(passthrough_config)
    )

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    # --- Assertions ---
    # Check that the FishAudioTTS client was instantiated exactly once.
    MockFishAudioTTSClient.assert_called_once()

    # Get the arguments that the mock was called with.
    # The constructor is called with keyword arguments like config=...
    # so we inspect the keyword arguments dictionary.
    call_args, call_kwargs = MockFishAudioTTSClient.call_args
    called_config = call_kwargs["config"]

    # Verify that the configuration object contains our expected parameters
    # Note: FishAudioTTS uses update_params() to merge params into the config
    assert hasattr(
        called_config, "api_key"
    ), "Config should have api_key parameter"
    assert (
        called_config.api_key == "test_api_key"
    ), f"Expected api_key to be test_api_key, but got {called_config.api_key}"

    config_params = called_config.params

    assert (
        "reference_id" in config_params
    ), "Config should have reference_id parameter"
    assert (
        config_params["reference_id"] == "728f6ff2240d49308e8137ffe66008e2"
    ), f"Expected reference_id to be 728f6ff2240d49308e8137ffe66008e2, but got {config_params['reference_id']}"

    assert "top_p" in config_params, "Config should have top_p parameter"
    assert (
        config_params["top_p"] == 0.7
    ), f"Expected top_p to be 0.7, but got {config_params['top_p']}"

    assert (
        "sample_rate" in config_params
    ), "Config should have sample_rate parameter"
    assert (
        config_params["sample_rate"] == 16000
    ), f"Expected sample_rate to be 16000, but got {config_params['sample_rate']}"

    assert (
        "temperature" in config_params
    ), "Config should have temperature parameter"
    assert (
        config_params["temperature"] == 0.7
    ), f"Expected temperature to be 0.7, but got {config_params['temperature']}"

    print("✅ Params passthrough test passed successfully.")
    print(
        f"✅ Verified config api_key: {called_config.api_key}, reference_id: {config_params['reference_id']}, top_p: {config_params['top_p']}, sample_rate: {config_params['sample_rate']}, temperature: {config_params['temperature']}"
    )
