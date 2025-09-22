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
            text="智能阶跃，十倍每个人的可能",
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


@patch("stepfun_tts_python.extension.StepFunTTS")
def test_error_debug_information(MockStepFunTTS):
    """Test that the extension provides detailed error information for debugging."""
    # Mock the StepFunTTS class to raise a detailed error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise a detailed error
    async def mock_get(text, request_id):
        # Raise exception before yielding anything
        raise Exception(
            "Detailed error message: Authentication failed with code 401, please check your API key"
        )
        # This line will never be reached
        yield b"", 0, None

    # Set up all required attributes and methods
    mock_client_instance.get = mock_get
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance.send_text_in_connection = False
    mock_client_instance.cur_request_id = ""

    # Mock config properties and methods
    mock_client_instance.config.get_model = AsyncMock(return_value="step-tts-mini")
    mock_client_instance.config.get_voice = AsyncMock(return_value="cixingnansheng")
    mock_client_instance.config.get_response_format = AsyncMock(return_value="mp3")
    mock_client_instance.config.get_speed = AsyncMock(return_value=1.0)
    mock_client_instance.config.get_volume = AsyncMock(return_value=1.0)
    mock_client_instance.config.get_sample_rate = AsyncMock(return_value=24000)
    mock_client_instance.config.get_voice_label = AsyncMock(return_value={})

    # Mock the constructor to return our mock instance
    MockStepFunTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterErrorDebug()

    # Set up configuration
    config = {
        "params": {
            "api_key": "fake_api_key_for_mock_testing",
            "model": "step-tts-mini",
            "voice": "cixingnansheng",
            "response_format": "mp3",
            "speed": 1.0,
            "volume": 1.0,
            "sample_rate": 24000,
        },
    }

    tester.set_test_mode_single("stepfun_tts_python", json.dumps(config))
    tester.run()

    # Verify that error was received with details
    assert tester.error_received, "Error event was not received"
    assert (
        "message" in tester.error_details
    ), "Error message not found in error details"
    assert (
        "authentication" in tester.error_details["message"].lower()
        or "401" in tester.error_details["message"]
        or "api key" in tester.error_details["message"].lower()
    ), f"Expected authentication error, got: {tester.error_details['message']}"


@patch("stepfun_tts_python.extension.StepFunTTS")
def test_error_debug_stack_trace(MockStepFunTTS):
    """Test that the extension provides stack trace information for debugging."""
    # Mock the StepFunTTS class to raise an error with stack trace
    mock_client_instance = AsyncMock()

    # Mock the get method to raise an error
    async def mock_get(text, request_id):
        try:
            # Simulate a deeper error
            raise ValueError("Invalid parameter: voice not supported")
        except ValueError as e:
            raise Exception(f"StepFun TTS error: {str(e)}") from e
        # This line will never be reached
        yield b"", 0, None

    # Set up all required attributes and methods
    mock_client_instance.get = mock_get
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance.send_text_in_connection = False
    mock_client_instance.cur_request_id = ""

    # Mock config properties and methods
    mock_client_instance.config.get_model = AsyncMock(return_value="step-tts-mini")
    mock_client_instance.config.get_voice = AsyncMock(return_value="invalid_voice")
    mock_client_instance.config.get_response_format = AsyncMock(return_value="mp3")
    mock_client_instance.config.get_speed = AsyncMock(return_value=1.0)
    mock_client_instance.config.get_volume = AsyncMock(return_value=1.0)
    mock_client_instance.config.get_sample_rate = AsyncMock(return_value=24000)
    mock_client_instance.config.get_voice_label = AsyncMock(return_value={})

    # Mock the constructor to return our mock instance
    MockStepFunTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterErrorDebug()

    # Set up configuration
    config = {
        "params": {
            "api_key": "fake_api_key_for_mock_testing",
            "model": "step-tts-mini",
            "voice": "invalid_voice",
            "response_format": "mp3",
            "speed": 1.0,
            "volume": 1.0,
            "sample_rate": 24000,
        },
    }

    tester.set_test_mode_single("stepfun_tts_python", json.dumps(config))
    tester.run()

    # Verify that error was received with details
    assert tester.error_received, "Error event was not received"
    assert (
        "message" in tester.error_details
    ), "Error message not found in error details"
    assert (
        "stepfun tts error" in tester.error_details["message"].lower()
        or "invalid parameter" in tester.error_details["message"].lower()
        or "voice not supported" in tester.error_details["message"].lower()
    ), f"Expected detailed error, got: {tester.error_details['message']}"


@patch("stepfun_tts_python.extension.StepFunTTS")
def test_error_debug_request_context(MockStepFunTTS):
    """Test that the extension provides request context in error details."""
    # Mock the StepFunTTS class to raise an error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise an error
    async def mock_get(text, request_id):
        # Raise exception before yielding anything
        raise Exception(
            f"Error processing text: '{text[:50]}...' (length: {len(text)}) for request {request_id}"
        )
        # This line will never be reached
        yield b"", 0, None

    # Set up all required attributes and methods
    mock_client_instance.get = mock_get
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance.send_text_in_connection = False
    mock_client_instance.cur_request_id = ""

    # Mock config properties and methods
    mock_client_instance.config.get_model = AsyncMock(return_value="step-tts-mini")
    mock_client_instance.config.get_voice = AsyncMock(return_value="cixingnansheng")
    mock_client_instance.config.get_response_format = AsyncMock(return_value="mp3")
    mock_client_instance.config.get_speed = AsyncMock(return_value=1.0)
    mock_client_instance.config.get_volume = AsyncMock(return_value=1.0)
    mock_client_instance.config.get_sample_rate = AsyncMock(return_value=24000)
    mock_client_instance.config.get_voice_label = AsyncMock(return_value={})

    # Mock the constructor to return our mock instance
    MockStepFunTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterErrorDebug()

    # Set up configuration
    config = {
        "params": {
            "api_key": "fake_api_key_for_mock_testing",
            "model": "step-tts-mini",
            "voice": "cixingnansheng",
            "response_format": "mp3",
            "speed": 1.0,
            "volume": 1.0,
            "sample_rate": 24000,
        },
    }

    tester.set_test_mode_single("stepfun_tts_python", json.dumps(config))
    tester.run()

    # Verify that error was received with request context
    assert tester.error_received, "Error event was not received"
    assert (
        "message" in tester.error_details
    ), "Error message not found in error details"
    assert (
        "智能阶跃" in tester.error_details["message"]
        or "tts_request_1" in tester.error_details["message"]
    ), f"Expected request context in error, got: {tester.error_details['message']}"


@patch("stepfun_tts_python.extension.StepFunTTS")
def test_error_debug_voice_label_context(MockStepFunTTS):
    """Test that the extension provides voice label context in error details."""
    # Mock the StepFunTTS class to raise an error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise an error
    async def mock_get(text, request_id):
        # Raise exception before yielding anything
        raise Exception(
            "Error with voice label configuration: language '粤语' not supported for voice 'cixingnansheng'"
        )
        # This line will never be reached
        yield b"", 0, None

    # Set up all required attributes and methods
    mock_client_instance.get = mock_get
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance.send_text_in_connection = False
    mock_client_instance.cur_request_id = ""

    # Mock config properties and methods
    mock_client_instance.config.get_model = AsyncMock(return_value="step-tts-vivid")
    mock_client_instance.config.get_voice = AsyncMock(return_value="cixingnansheng")
    mock_client_instance.config.get_response_format = AsyncMock(return_value="wav")
    mock_client_instance.config.get_speed = AsyncMock(return_value=1.2)
    mock_client_instance.config.get_volume = AsyncMock(return_value=1.5)
    mock_client_instance.config.get_sample_rate = AsyncMock(return_value=24000)
    mock_client_instance.config.get_voice_label = AsyncMock(return_value={
        "language": "粤语",
        "emotion": "高兴",
        "style": "慢速"
    })

    # Mock the constructor to return our mock instance
    MockStepFunTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterErrorDebug()

    # Set up configuration with voice label
    config = {
        "params": {
            "api_key": "fake_api_key_for_mock_testing",
            "model": "step-tts-vivid",
            "voice": "cixingnansheng",
            "response_format": "wav",
            "speed": 1.2,
            "volume": 1.5,
            "sample_rate": 24000,
            "voice_label": {
                "language": "粤语",
                "emotion": "高兴",
                "style": "慢速"
            }
        },
    }

    tester.set_test_mode_single("stepfun_tts_python", json.dumps(config))
    tester.run()

    # Verify that error was received with voice label context
    assert tester.error_received, "Error event was not received"
    assert (
        "message" in tester.error_details
    ), "Error message not found in error details"
    assert (
        "voice label" in tester.error_details["message"].lower()
        or "粤语" in tester.error_details["message"]
        or "cixingnansheng" in tester.error_details["message"]
    ), f"Expected voice label context in error, got: {tester.error_details['message']}"


@patch("stepfun_tts_python.extension.StepFunTTS")
def test_error_debug_network_error(MockStepFunTTS):
    """Test that the extension provides network error context for debugging."""
    # Mock the StepFunTTS class to raise a network error
    mock_client_instance = AsyncMock()

    # Mock the get method to raise a network error
    async def mock_get(text, request_id):
        # Raise exception before yielding anything
        raise Exception(
            "Network error: Failed to connect to StepFun API after 3 attempts. Please check your internet connection and API endpoint."
        )
        # This line will never be reached
        yield b"", 0, None

    # Set up all required attributes and methods
    mock_client_instance.get = mock_get
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance.send_text_in_connection = False
    mock_client_instance.cur_request_id = ""

    # Mock config properties and methods
    mock_client_instance.config.get_model = AsyncMock(return_value="step-tts-mini")
    mock_client_instance.config.get_voice = AsyncMock(return_value="cixingnansheng")
    mock_client_instance.config.get_response_format = AsyncMock(return_value="mp3")
    mock_client_instance.config.get_speed = AsyncMock(return_value=1.0)
    mock_client_instance.config.get_volume = AsyncMock(return_value=1.0)
    mock_client_instance.config.get_sample_rate = AsyncMock(return_value=24000)
    mock_client_instance.config.get_voice_label = AsyncMock(return_value={})

    # Mock the constructor to return our mock instance
    MockStepFunTTS.return_value = mock_client_instance

    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterErrorDebug()

    # Set up configuration
    config = {
        "params": {
            "api_key": "fake_api_key_for_mock_testing",
            "base_url": "https://invalid-api.stepfun.com/v1",
            "model": "step-tts-mini",
            "voice": "cixingnansheng",
            "response_format": "mp3",
            "speed": 1.0,
            "volume": 1.0,
            "sample_rate": 24000,
        },
    }

    tester.set_test_mode_single("stepfun_tts_python", json.dumps(config))
    tester.run()

    # Verify that error was received with network context
    assert tester.error_received, "Error event was not received"
    assert (
        "message" in tester.error_details
    ), "Error message not found in error details"
    assert (
        "network error" in tester.error_details["message"].lower()
        or "failed to connect" in tester.error_details["message"].lower()
        or "stepfun api" in tester.error_details["message"].lower()
    ), f"Expected network error context, got: {tester.error_details['message']}"


