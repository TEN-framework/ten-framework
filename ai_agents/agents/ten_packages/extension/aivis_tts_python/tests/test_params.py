#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import sys
from pathlib import Path

project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

from pathlib import Path
import json
from unittest.mock import AsyncMock, patch

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    TenError,
)


class ExtensionTesterForPassthrough(ExtensionTester):
    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
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


@patch("aivis_tts_python.extension.AivisTTSClient")
def test_params_passthrough(MockAivisTTSClient):
    """Tests that params are correctly forwarded to the AivisTTSClient constructor."""
    print("Starting test_params_passthrough with mock...")

    mock_instance = MockAivisTTSClient.return_value
    mock_instance.clean = AsyncMock()

    real_params = {
        "api_key": "test_api_key",
        "model_uuid": "a59cb814-0083-4369-8542-f51a29e72af7",
        "language": "ja",
        "output_sampling_rate": 16000,
        "output_audio_channels": "mono",
        "use_ssml": False,
        "leading_silence_seconds": 0.0,
        "trailing_silence_seconds": 0.1,
        "base_url": "https://api.aivis-project.com",
    }

    real_config = {
        "params": real_params,
    }

    # update_params() forces output_format to "wav" and strips api_key/base_url from
    # the request body (not from params). The params dict that ends up on the
    # client sees the additions from update_params().
    expected_params = dict(real_params)
    expected_params["output_format"] = "wav"

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single("aivis_tts_python", json.dumps(real_config))

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    MockAivisTTSClient.assert_called_once()

    _, call_kwargs = MockAivisTTSClient.call_args
    called_config = call_kwargs["config"]

    print(f"called_config: {called_config.params}")
    assert called_config.params == expected_params, (
        f"Expected params to be {expected_params}, "
        f"but got {called_config.params}"
    )

    print("✅ Params passthrough test passed successfully.")
    print(f"✅ Verified params: {called_config.params}")
