#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from unittest.mock import patch, AsyncMock, MagicMock
import json
import asyncio

from ten_runtime import (
    Cmd,
    CmdResult,
    ExtensionTester,
    StatusCode,
    TenEnvTester,
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


@patch("tencent_tts_python.extension.TencentTTSClient")
def test_params_passthrough(MockTencentTTSClient):
    """
    Tests that custom parameters passed in the configuration are correctly
    forwarded to the RimeTTSClient constructor.
    """
    print("Starting test_params_passthrough with mock...")

    # --- Mock Configuration ---
    mock_instance = MockTencentTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.synthesize_audio = MagicMock()
    # Mock synthesize_audio and get_audio_data with proper timing using asyncio.Queue
    audio_queue = asyncio.Queue()

    async def mock_get_audio_data():
        return await audio_queue.get()

    mock_instance.get_audio_data.side_effect = mock_get_audio_data

    # --- Test Setup ---
    # Define a configuration with custom parameters inside 'params'.
    # These are the parameters we expect to be "passed through".
    real_params = {
        "app_id": "1234567890",
        "secret_id": "a_test_secret_id",
        "secret_key": "a_test_secret_key",
        "voice_type": 1008,
    }

    real_config = {
        "params": real_params,
    }

    passthrough_params = {
        "app_id": "1234567890",
        "sample_rate": 16000,
        "secret_id": "a_test_secret_id",
        "secret_key": "a_test_secret_key",
        "voice_type": 1008,
    }

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single("tencent_tts_python", json.dumps(real_config))

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    # --- Assertions ---
    # Check that the RimeTTSClient client was instantiated exactly once.
    MockTencentTTSClient.assert_called_once()

    # Get the arguments that the mock was called with.
    # The constructor is called with keyword arguments like config=...
    # so we inspect the keyword arguments dictionary.
    call_args, _ = MockTencentTTSClient.call_args
    called_config = call_args[0]

    # Verify that the 'params' dictionary in the config object passed to the
    # client constructor is identical to the one we defined in our test config.
    print(f"called_config: {called_config.params}")
    assert (
        called_config.params == passthrough_params
    ), f"Expected params to be {passthrough_params}, but got {called_config.params}"

    print("✅ Params passthrough test passed successfully.")
    print(f"✅ Verified params: {called_config.params}")
