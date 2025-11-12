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


class ExtensionTesterErrorMsg(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_message = ""

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info(
            "Error message test started, sending TTS request."
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
            self.error_message = error_data.get("message", "")
            ten_env.log_info(f"Received error: {self.error_message}")
            self.error_received = True
            ten_env.stop_test()


@patch("google_tts_python.extension.GoogleTTS")
def test_network_error(MockGoogleTTS):
    """Test that the extension handles network errors correctly."""
    # Mock the GoogleTTS class to raise a network error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise a network error
    async def mock_get(text, request_id):
        print(f"Mock get called with text: {text}")
        # Raise exception immediately to simulate network error
        raise Exception(
            "503 failed to connect to all addresses; last error: UNAVAILABLE: ipv4:142.251.215.234:443: Socket closed"
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
    print(f"Setting MockGoogleTTS.return_value to: {mock_client_instance}")
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    print(
        "Mock setup complete. MockGoogleTTS.return_value:",
        MockGoogleTTS.return_value,
    )
    tester = ExtensionTesterErrorMsg()

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
    print("Test completed. Checking results...")

    # Verify that error was received
    assert tester.error_received, "Error event was not received"
    assert (
        "network" in tester.error_message.lower()
        or "connect" in tester.error_message.lower()
    ), f"Expected network error, got: {tester.error_message}"


@patch("google_tts_python.extension.GoogleTTS")
def test_authentication_error(MockGoogleTTS):
    """Test that the extension handles authentication errors correctly."""
    # Mock the GoogleTTS class to raise an authentication error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise an authentication error
    async def mock_get(text, request_id):
        print(f"Mock get called with text: {text}")
        # Raise exception immediately to simulate authentication error
        raise Exception("401 Unauthorized: Invalid credentials")
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
    print(f"Setting MockGoogleTTS.return_value to: {mock_client_instance}")
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    print(
        "Mock setup complete. MockGoogleTTS.return_value:",
        MockGoogleTTS.return_value,
    )
    tester = ExtensionTesterErrorMsg()

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
    print("Test completed. Checking results...")

    # Verify that error was received
    assert tester.error_received, "Error event was not received"
    assert (
        "authentication" in tester.error_message.lower()
        or "credentials" in tester.error_message.lower()
        or "unauthorized" in tester.error_message.lower()
    ), f"Expected authentication error, got: {tester.error_message}"


@patch("google_tts_python.extension.GoogleTTS")
def test_quota_exceeded_error(MockGoogleTTS):
    """Test that the extension handles quota exceeded errors correctly."""
    # Mock the GoogleTTS class to raise a quota exceeded error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise a quota exceeded error
    async def mock_get(text, request_id):
        print(f"Mock get called with text: {text}")
        # Raise exception immediately to simulate quota error
        raise Exception("429 Quota exceeded for quota group 'default'")
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
    print(f"Setting MockGoogleTTS.return_value to: {mock_client_instance}")
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    print(
        "Mock setup complete. MockGoogleTTS.return_value:",
        MockGoogleTTS.return_value,
    )
    tester = ExtensionTesterErrorMsg()

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
    print("Test completed. Checking results...")

    # Verify that error was received
    assert tester.error_received, "Error event was not received"
    assert (
        "quota" in tester.error_message.lower() or "429" in tester.error_message
    ), f"Expected quota error, got: {tester.error_message}"


@patch("google_tts_python.extension.GoogleTTS")
def test_invalid_text_error(MockGoogleTTS):
    """Test that the extension handles invalid text errors correctly."""
    # Mock the GoogleTTS class to raise an invalid text error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise an invalid text error
    async def mock_get(text, request_id):
        print(f"Mock get called with text: {text}")
        # Raise exception immediately to simulate invalid text error
        raise Exception("400 Bad Request: Invalid text input")
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
    print(f"Setting MockGoogleTTS.return_value to: {mock_client_instance}")
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    print(
        "Mock setup complete. MockGoogleTTS.return_value:",
        MockGoogleTTS.return_value,
    )
    tester = ExtensionTesterErrorMsg()

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
    print("Test completed. Checking results...")

    # Verify that error was received
    assert tester.error_received, "Error event was not received"
    assert (
        "invalid" in tester.error_message.lower()
        or "400" in tester.error_message
    ), f"Expected invalid text error, got: {tester.error_message}"


@patch("google_tts_python.extension.GoogleTTS")
def test_timeout_error(MockGoogleTTS):
    """Test that the extension handles timeout errors correctly."""
    # Mock the GoogleTTS class to raise a timeout error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise a timeout error
    async def mock_get(text, request_id):
        print(f"Mock get called with text: {text}")
        # Raise exception immediately to simulate timeout error
        raise Exception("504 Gateway Timeout: Request timed out")
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
    print(f"Setting MockGoogleTTS.return_value to: {mock_client_instance}")
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    print(
        "Mock setup complete. MockGoogleTTS.return_value:",
        MockGoogleTTS.return_value,
    )
    tester = ExtensionTesterErrorMsg()

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
    print("Test completed. Checking results...")

    # Verify that error was received
    assert tester.error_received, "Error event was not received"
    assert (
        "timeout" in tester.error_message.lower()
        or "504" in tester.error_message
    ), f"Expected timeout error, got: {tester.error_message}"


@patch("google_tts_python.extension.GoogleTTS")
def test_generic_error(MockGoogleTTS):
    """Test that the extension handles generic errors correctly."""
    # Mock the GoogleTTS class to raise a generic error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise a generic error
    async def mock_get(text, request_id):
        print(f"Mock get called with text: {text}")
        # Raise exception immediately to simulate generic error
        raise Exception("500 Internal Server Error: Something went wrong")
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
    print(f"Setting MockGoogleTTS.return_value to: {mock_client_instance}")
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    print(
        "Mock setup complete. MockGoogleTTS.return_value:",
        MockGoogleTTS.return_value,
    )
    tester = ExtensionTesterErrorMsg()

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
    print("Test completed. Checking results...")

    # Verify that error was received
    assert tester.error_received, "Error event was not received"
    assert (
        "500" in tester.error_message
        or "internal server error" in tester.error_message.lower()
    ), f"Expected generic error, got: {tester.error_message}"


@patch("google_tts_python.extension.GoogleTTS")
def test_unsupported_language_error(MockGoogleTTS):
    """Test that the extension handles unsupported language errors correctly."""
    # Mock the GoogleTTS class to raise an unsupported language error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise an unsupported language error
    async def mock_get(text, request_id):
        print(f"Mock get called with text: {text}")
        # Raise exception immediately to simulate unsupported language error
        raise Exception("400 Bad Request: Unsupported language code")
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
    print(f"Setting MockGoogleTTS.return_value to: {mock_client_instance}")
    MockGoogleTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    print(
        "Mock setup complete. MockGoogleTTS.return_value:",
        MockGoogleTTS.return_value,
    )
    tester = ExtensionTesterErrorMsg()

    # Set up configuration with unsupported language
    config = {
        "params": {
            "AudioConfig": {"sample_rate_hertz": 16000},
            "credentials": "fake_credentials_for_mock_testing",
        },
    }

    tester.set_test_mode_single("google_tts_python", json.dumps(config))
    tester.run()
    print("Test completed. Checking results...")

    # Verify that error was received
    assert tester.error_received, "Error event was not received"
    assert (
        "unsupported" in tester.error_message.lower()
        or "language" in tester.error_message.lower()
        or "400" in tester.error_message
    ), f"Expected unsupported language error, got: {tester.error_message}"


@patch("google_tts_python.extension.GoogleTTS")
def test_simple_mock_verification(MockGoogleTTS):
    """Simple test to verify mock is working"""
    print("=== Simple mock verification test ===")

    # Create mock instance
    mock_client_instance = AsyncMock()

    # Mock the get method
    async def mock_get(text, request_id):
        print(f"Mock get called with text: {text}")
        # Raise exception immediately to simulate test exception
        raise Exception("Test exception from mock")
        # This line will never be reached
        yield b"", 0, None

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
    mock_client_instance.config.language_code = "en-US"
    mock_client_instance.config.get_ssml_gender = AsyncMock(return_value=1)
    mock_client_instance._initialize_client = AsyncMock()

    print(f"Setting MockGoogleTTS.return_value to: {mock_client_instance}")
    MockGoogleTTS.return_value = mock_client_instance

    print("Mock setup complete")

    # Create and run tester
    tester = ExtensionTesterErrorMsg()

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

    print(f"Test completed. Error received: {tester.error_received}")
    print(f"Error message: {tester.error_message}")

    assert tester.error_received, "Error event was not received"
    assert (
        "Test exception" in tester.error_message
    ), f"Expected test exception, got: {tester.error_message}"
