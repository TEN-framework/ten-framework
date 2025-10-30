#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from unittest.mock import patch, AsyncMock
import asyncio
import json

from ten_runtime import (
    Cmd,
    CmdResult,
    ExtensionTester,
    StatusCode,
    TenEnvTester,
    TenError,
)
from ..cosy_tts import MESSAGE_TYPE_CMD_COMPLETE


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


@patch("cosy_tts_python.extension.CosyTTSClient")
def test_params_passthrough(MockCosyTTSClient):
    """
    Tests that custom parameters passed in the configuration are correctly
    forwarded to the CosyTTSClient client constructor.
    """
    print("Starting test_params_passthrough with mock...")

    # --- Mock Setup ---
    # Create a mock instance with properly configured async methods
    mock_instance = MockCosyTTSClient.return_value
    mock_instance.synthesize_audio = AsyncMock()

    # Create state to hold the queue and task
    stream_state = {"queue": None, "task": None}

    async def get_audio_data():
        """Simulate async streaming data from queue"""
        # Lazy initialization: create queue and start producer on first call
        if stream_state["queue"] is None:
            stream_state["queue"] = asyncio.Queue()

            async def simulate_audio_stream():
                """Simulate TTS service completing immediately"""
                queue = stream_state["queue"]
                await asyncio.sleep(
                    0.01
                )  # Small delay to ensure proper initialization
                await queue.put((True, MESSAGE_TYPE_CMD_COMPLETE, None))

            # Start producer task in background
            stream_state["task"] = asyncio.create_task(simulate_audio_stream())

        return await stream_state["queue"].get()

    mock_instance.get_audio_data.side_effect = get_audio_data

    # --- Test Setup ---
    # Define a configuration with custom, arbitrary parameters inside 'params'.
    # These are the parameters we expect to be "passed through".
    passthrough_params = {
        "api_key": "a_valid_key",
        "model": "cosyvoice-v1",
        "sample_rate": 16000,
        "voice": "longxiaochun",
    }
    passthrough_config = {
        "params": passthrough_params,
    }

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single(
        "cosy_tts_python", json.dumps(passthrough_config)
    )

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    # --- Assertions ---
    # Check that the CosyTTSClient client was instantiated exactly once.
    MockCosyTTSClient.assert_called_once()

    # Get the arguments that the mock was called with.
    # The constructor signature is (self, config, ten_env, vendor),
    # so we inspect the 'config' object at index 1 of the call arguments.
    call_args, call_kwargs = MockCosyTTSClient.call_args
    called_config = call_args[0]

    # Verify that the 'params' dictionary in the config object passed to the
    # client constructor is identical to the one we defined in our test config.
    assert (
        called_config.params == passthrough_params
    ), f"Expected params to be {passthrough_params}, but got {called_config.params}"

    print("✅ Params passthrough test passed successfully.")
    print(f"✅ Verified params: {called_config.params}")
