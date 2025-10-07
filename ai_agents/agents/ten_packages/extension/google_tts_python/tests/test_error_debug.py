import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
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
import asyncio

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput


class ExtensionTesterErrorDebug(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_details = {}

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info(
            "Error debug test started, sending TTS request."
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
        if name == "error":
            if self.error_received:
                ten_env.log_info(
                    f"Error already received, ignoring further errors."
                )
                return
            json_str, _ = data.get_property_to_json("")
            error_data = json.loads(json_str) if json_str else {}
            self.error_details = error_data
            ten_env.log_info(f"Received error details: {self.error_details}")
            self.error_received = True
            ten_env.stop_test()


@patch("google_tts_python.extension.GoogleTTS")
def test_error_debug_information(MockGoogleTTS):
    """Test that the extension provides detailed error information for debugging."""
    # Mock the GoogleTTS class to raise a detailed error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise a detailed error
    async def mock_get(text, request_id):
        # Raise exception before yielding anything
        raise Exception(
            "Detailed error message: Authentication failed with code 401, please check your credentials"
        )
        # This line will never be reached
        yield b"", 0, None

    # Set up all required attributes and methods
    mock_client_instance.get = mock_get
    mock_client_instance.get_streaming = mock_get  # Streaming method
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance._should_stop_streaming = False
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance._is_cancelled = False
    mock_client_instance.credentials = None
    mock_client_instance.send_text_in_connection = False
    mock_client_instance.cur_request_id = ""
    mock_client_instance.streaming_enabled = False
    mock_client_instance.streaming_config = None

    # Mock config properties and methods
    mock_client_instance.config.language_code = "en-US"
    mock_client_instance.config.get_ssml_gender = AsyncMock(
        return_value=1
    )  # NEUTRAL

    # Mock the constructor to return our mock instance
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterErrorDebug()

    # Set up configuration
    config = {
        "params": {
            "AudioConfig": {"sample_rate_hertz": 16000},
            "VoiceSelectionParams": {
                "language_code": "en-US",
                "ssml_gender": "NEUTRAL",
            },
            "credentials": "fake_credentials_for_mock_testing",
        },
    }

    tester.set_test_mode_single("google_tts_python", json.dumps(config))
    tester.run()

    # Verify that error was received with details
    assert tester.error_received, "Error event was not received"
    assert (
        "message" in tester.error_details
    ), "Error message not found in error details"
    assert (
        "authentication" in tester.error_details["message"].lower()
        or "401" in tester.error_details["message"]
    ), f"Expected authentication error, got: {tester.error_details['message']}"


@patch("google_tts_python.extension.GoogleTTS")
def test_error_debug_stack_trace(MockGoogleTTS):
    """Test that the extension provides stack trace information for debugging."""
    # Mock the GoogleTTS class to raise an error with stack trace
    mock_client_instance = AsyncMock()

    # Mock the get method to raise an error
    async def mock_get(text, request_id):
        try:
            # Simulate a deeper error
            raise ValueError("Invalid parameter")
        except ValueError as e:
            raise Exception(f"Google TTS error: {str(e)}") from e
        # This line will never be reached
        yield b"", 0, None

    # Set up all required attributes and methods
    mock_client_instance.get = mock_get
    mock_client_instance.get_streaming = mock_get  # Streaming method
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance._should_stop_streaming = False
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance._is_cancelled = False
    mock_client_instance.credentials = None
    mock_client_instance.send_text_in_connection = False
    mock_client_instance.cur_request_id = ""
    mock_client_instance.streaming_enabled = False
    mock_client_instance.streaming_config = None

    # Mock config properties and methods
    mock_client_instance.config.language_code = "en-US"
    mock_client_instance.config.get_ssml_gender = AsyncMock(
        return_value=1
    )  # NEUTRAL

    # Mock the constructor to return our mock instance
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterErrorDebug()

    # Set up configuration
    config = {
        "params": {
            "AudioConfig": {"sample_rate_hertz": 16000},
            "VoiceSelectionParams": {
                "language_code": "en-US",
                "ssml_gender": "NEUTRAL",
            },
            "credentials": "fake_credentials_for_mock_testing",
        },
    }

    tester.set_test_mode_single("google_tts_python", json.dumps(config))
    tester.run()

    # Verify that error was received with details
    assert tester.error_received, "Error event was not received"
    assert (
        "message" in tester.error_details
    ), "Error message not found in error details"
    assert (
        "google tts error" in tester.error_details["message"].lower()
        or "invalid parameter" in tester.error_details["message"].lower()
    ), f"Expected detailed error, got: {tester.error_details['message']}"


@patch("google_tts_python.extension.GoogleTTS")
def test_error_debug_request_context(MockGoogleTTS):
    """Test that the extension provides request context in error details."""
    # Mock the GoogleTTS class to raise an error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise an error
    async def mock_get(text, request_id):
        # Raise exception before yielding anything
        raise Exception(
            f"Error processing text: '{text[:50]}...' (length: {len(text)})"
        )
        # This line will never be reached
        yield b"", 0, None

    # Set up all required attributes and methods
    mock_client_instance.get = mock_get
    mock_client_instance.get_streaming = mock_get  # Streaming method
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance._should_stop_streaming = False
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance._is_cancelled = False
    mock_client_instance.credentials = None
    mock_client_instance.send_text_in_connection = False
    mock_client_instance.cur_request_id = ""
    mock_client_instance.streaming_enabled = False
    mock_client_instance.streaming_config = None

    # Mock config properties and methods
    mock_client_instance.config.language_code = "en-US"
    mock_client_instance.config.get_ssml_gender = AsyncMock(
        return_value=1
    )  # NEUTRAL

    # Mock the constructor to return our mock instance
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterErrorDebug()

    # Set up configuration
    config = {
        "params": {
            "AudioConfig": {"sample_rate_hertz": 16000},
            "VoiceSelectionParams": {
                "language_code": "en-US",
                "ssml_gender": "NEUTRAL",
            },
            "credentials": "fake_credentials_for_mock_testing",
        },
    }

    tester.set_test_mode_single("google_tts_python", json.dumps(config))
    tester.run()

    # Verify that error was received with request context
    assert tester.error_received, "Error event was not received"
    assert (
        "message" in tester.error_details
    ), "Error message not found in error details"
    assert (
        "hello word" in tester.error_details["message"].lower()
    ), f"Expected request context in error, got: {tester.error_details['message']}"
