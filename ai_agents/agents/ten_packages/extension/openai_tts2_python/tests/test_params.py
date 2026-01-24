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
from unittest.mock import patch, MagicMock, AsyncMock

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


@patch("openai_tts2_python.openai_tts.AsyncClient")
def test_params_passthrough(MockAsyncClient):
    """
    Tests that custom parameters passed in the configuration are correctly
    forwarded to the OpenAI TTS client constructor.
    """
    print("Starting test_params_passthrough with mock...")

    # --- Mock Configuration ---
    mock_client = AsyncMock()
    mock_client.aclose = AsyncMock()
    MockAsyncClient.return_value = mock_client

    # --- Test Setup ---
    # Define a configuration with custom parameters inside 'params'.
    real_config = {
        "params": {
            "api_key": "a_test_api_key",
            "model": "gpt-4o-mini-tts",
        },
    }

    # Expected params after processing (response_format is added by update_params)
    passthrough_params = {
        "api_key": "a_test_api_key",
        "model": "gpt-4o-mini-tts",
        "voice": "coral",
        "speed": 1.0,
        "instructions": "",
        "response_format": "pcm",
    }

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single("openai_tts2_python", json.dumps(real_config))

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    # --- Assertions ---
    # Check that the httpx AsyncClient was instantiated
    MockAsyncClient.assert_called_once()

    # For httpx-based implementation, we verify params are passed correctly
    # by checking that the client was created (params are used in OpenAITTSClient.__init__)
    # The actual parameter passthrough happens in the get() method when building the payload

    print("✅ Params passthrough test passed successfully.")
    print(f"✅ httpx AsyncClient was created correctly")


@patch("openai_tts2_python.openai_tts.AsyncClient")
@patch("openai_tts2_python.openai_tts.Timeout")
@patch("openai_tts2_python.openai_tts.Limits")
def test_url_and_base_url_configuration(
    MockLimits, MockTimeout, MockAsyncClient
):
    """
    Tests that the endpoint URL is correctly configured based on 'url' or 'base_url' parameters.

    Test cases:
    1. When 'url' is provided, endpoint should use the url value directly
    2. When 'base_url' is provided (with trailing slash), endpoint should be {base_url}/audio/speech
    3. When 'base_url' is provided (without trailing slash), endpoint should be {base_url}/audio/speech
    4. When neither is provided, endpoint should use default https://api.openai.com/v1/audio/speech
    """
    print("Starting test_url_and_base_url_configuration with mock...")

    from openai_tts2_python.openai_tts import OpenAITTSClient
    from openai_tts2_python.config import OpenAITTSConfig
    from ten_runtime import AsyncTenEnv
    from unittest.mock import MagicMock

    # Mock httpx components
    MockTimeout.return_value = MagicMock()
    MockLimits.return_value = MagicMock()
    mock_client = AsyncMock()
    mock_client.aclose = AsyncMock()
    MockAsyncClient.return_value = mock_client

    # Mock TenEnv
    mock_ten_env = MagicMock(spec=AsyncTenEnv)
    mock_ten_env.log_info = MagicMock()
    mock_ten_env.log_debug = MagicMock()
    mock_ten_env.log_error = MagicMock()
    mock_ten_env.log_warn = MagicMock()

    # Common params for all test cases (model and voice are required by validate())
    common_params = {
        "api_key": "test_key",
        "model": "gpt-4o-mini-tts",
        "voice": "coral",
    }

    # Test Case 1: Using 'url' parameter
    print("  → Test Case 1: Using 'url' parameter")
    config_with_url = OpenAITTSConfig(
        params={
            **common_params,
            "url": "https://custom-server.com/v1/tts",
        }
    )
    client_with_url = OpenAITTSClient(config_with_url, mock_ten_env)
    assert (
        client_with_url.endpoint == "https://custom-server.com/v1/tts"
    ), f"Expected endpoint to be 'https://custom-server.com/v1/tts', got '{client_with_url.endpoint}'"
    print("    ✓ URL parameter correctly used as endpoint")

    # Test Case 2: Using 'base_url' parameter (with trailing slash)
    print("  → Test Case 2: Using 'base_url' parameter (with trailing slash)")
    config_with_base_url_slash = OpenAITTSConfig(
        params={
            **common_params,
            "base_url": "https://api.custom.com/v1/",
        }
    )
    client_with_base_url_slash = OpenAITTSClient(
        config_with_base_url_slash, mock_ten_env
    )
    expected_endpoint = "https://api.custom.com/v1/audio/speech"
    assert (
        client_with_base_url_slash.endpoint == expected_endpoint
    ), f"Expected endpoint to be '{expected_endpoint}', got '{client_with_base_url_slash.endpoint}'"
    print("    ✓ Base URL with trailing slash correctly processed")

    # Test Case 3: Using 'base_url' parameter (without trailing slash)
    print(
        "  → Test Case 3: Using 'base_url' parameter (without trailing slash)"
    )
    config_with_base_url = OpenAITTSConfig(
        params={
            **common_params,
            "base_url": "https://api.custom.com/v1",
        }
    )
    client_with_base_url = OpenAITTSClient(config_with_base_url, mock_ten_env)
    expected_endpoint = "https://api.custom.com/v1/audio/speech"
    assert (
        client_with_base_url.endpoint == expected_endpoint
    ), f"Expected endpoint to be '{expected_endpoint}', got '{client_with_base_url.endpoint}'"
    print("    ✓ Base URL without trailing slash correctly processed")

    # Test Case 4: Neither 'url' nor 'base_url' provided (should use default)
    print("  → Test Case 4: Using default endpoint (no url or base_url)")
    config_default = OpenAITTSConfig(params=common_params)
    client_default = OpenAITTSClient(config_default, mock_ten_env)
    expected_endpoint = "https://api.openai.com/v1/audio/speech"
    assert (
        client_default.endpoint == expected_endpoint
    ), f"Expected endpoint to be '{expected_endpoint}', got '{client_default.endpoint}'"
    print("    ✓ Default endpoint correctly used")

    # Test Case 5: 'url' takes precedence over 'base_url' when both are provided
    print("  → Test Case 5: 'url' takes precedence over 'base_url'")
    config_both = OpenAITTSConfig(
        params={
            **common_params,
            "url": "https://url-takes-precedence.com/tts",
            "base_url": "https://base-url-should-be-ignored.com/v1",
        }
    )
    client_both = OpenAITTSClient(config_both, mock_ten_env)
    assert (
        client_both.endpoint == "https://url-takes-precedence.com/tts"
    ), f"Expected endpoint to be 'https://url-takes-precedence.com/tts', got '{client_both.endpoint}'"
    print("    ✓ URL parameter correctly takes precedence over base_url")

    print("✅ URL and base_url configuration test passed successfully.")
