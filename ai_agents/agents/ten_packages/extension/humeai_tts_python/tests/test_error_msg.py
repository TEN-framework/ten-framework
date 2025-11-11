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
from ten_ai_base.struct import TTSTextInput


# ================ test empty params ================
class ExtensionTesterEmptyParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts"""
        ten_env_tester.log_info("Empty params test started")
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)

            self.error_code = error_data.get("code")
            self.error_message = error_data.get("message", "")
            self.error_module = error_data.get("module", "")

            ten_env.log_info(
                f"Received error: code={self.error_code}, message={self.error_message}, module={self.error_module}"
            )
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()


def test_empty_params_fatal_error():
    """Test that empty params raises FATAL ERROR with code -1000"""
    print("Starting test_empty_params_fatal_error...")

    # Empty key configuration (in params)
    empty_params_config = {
        "params": {
            "key": "",
            "voice_name": "Female English Actor",
        },
    }

    tester = ExtensionTesterEmptyParams()
    tester.set_test_mode_single(
        "humeai_tts_python", json.dumps(empty_params_config)
    )

    print("Running test...")
    error = tester.run()
    print("Test completed.")

    # Verify FATAL ERROR was received
    assert tester.error_received, "Expected to receive error message"
    assert (
        tester.error_code == -1000
    ), f"Expected error code -1000 (FATAL_ERROR), got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert len(tester.error_message) > 0, "Error message should not be empty"

    print(
        f"✅ Empty params test passed: code={tester.error_code}, message={tester.error_message}"
    )
    print("Test verification completed successfully.")


# ================ test invalid api key ================
class ExtensionTesterInvalidApiKey(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None
        self.vendor_info = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request to trigger the logic."""
        ten_env_tester.log_info(
            "Invalid API key test started, sending TTS request"
        )

        tts_input = TTSTextInput(
            request_id="test-request-invalid-key",
            text="This text will trigger API key validation.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)

            self.error_code = error_data.get("code")
            self.error_message = error_data.get("message", "")
            self.error_module = error_data.get("module", "")
            self.vendor_info = error_data.get("vendor_info", {})

            ten_env.log_info(
                f"Received error: code={self.error_code}, message={self.error_message}"
            )
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()


@patch("humeai_tts_python.humeTTS.AsyncHumeClient")
def test_invalid_api_key_error(MockHumeClient):
    """Test that an invalid API key is handled correctly with a mock."""
    print("Starting test_invalid_api_key_error with mock...")

    # Mock the Hume client to raise an authentication error
    mock_client = MockHumeClient.return_value
    mock_client.clean = AsyncMock()

    # Define an async generator that raises the invalid key exception
    async def mock_tts_error(
        context=None, utterances=None, format=None, instant_mode=None
    ):
        error_msg = "headers: {'date': 'Thu, 31 Jul 2025 06:47:59 GMT', 'content-type': 'application/json', 'content-length': '90', 'connection': 'keep-alive', 'x-request-id': '9bccec6c-dd26-4b2d-b99b-a9e2a305e0a4', 'via': '1.1 google', 'cf-cache-status': 'DYNAMIC', 'server': 'cloudflare', 'cf-ray': '967b25c22ed66837-NRT'}, status_code: 401, body: {'fault': {'faultstring': 'Invalid ApiKey', 'detail': {'errorcode': 'oauth.v2.InvalidApiKey'}}}"
        raise Exception(error_msg)
        yield  # Unreachable, but makes this an async generator function

    mock_client.tts.synthesize_json_streaming = mock_tts_error

    # Config with invalid API key
    invalid_key_config = {
        "params": {
            "key": "invalid_api_key_test",
            "voice_name": "Female English Actor",
        },
    }

    tester = ExtensionTesterInvalidApiKey()
    tester.set_test_mode_single(
        "humeai_tts_python", json.dumps(invalid_key_config)
    )

    print("Running test with mock...")
    tester.run()
    print("Test with mock completed.")

    # Verify FATAL ERROR was received for invalid API key
    assert tester.error_received, "Expected to receive error message"
    assert (
        tester.error_code == -1000
    ), f"Expected error code -1000 (FATAL_ERROR), got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert (
        "Invalid ApiKey" in tester.error_message
    ), "Error message should mention Invalid ApiKey"

    # Verify vendor_info
    vendor_info = tester.vendor_info
    assert vendor_info is not None, "Expected vendor_info to be present"
    assert (
        vendor_info.get("vendor") == "humeai"
    ), f"Expected vendor 'humeai', got {vendor_info.get('vendor')}"

    print(
        f"✅ Invalid API key test passed: code={tester.error_code}, message={tester.error_message}"
    )
