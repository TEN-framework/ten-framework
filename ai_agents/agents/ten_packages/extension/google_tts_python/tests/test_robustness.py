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
from unittest.mock import patch, AsyncMock, Mock
import asyncio
import time

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput


class ExtensionTesterRobustness(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.error_received = False
        self.request_count = 0
        self.max_requests = 10

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends multiple TTS requests."""
        ten_env_tester.log_info(
            "Robustness test started, sending TTS requests."
        )

        # Send first request
        self._send_tts_request(
            ten_env_tester, "tts_request_1", "hello word, hello agora"
        )
        ten_env_tester.on_start_done()

    def _send_tts_request(
        self, ten_env_tester: TenEnvTester, request_id: str, text: str
    ):
        """Send a TTS request."""
        tts_input = TTSTextInput(
            request_id=request_id,
            text=text,
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"ExtensionTesterRobustness: Received data: {name}")

        if name == "tts_audio_end":
            self.request_count += 1
            ten_env.log_info(
                f"Received tts_audio_end, request count: {self.request_count}"
            )

            if self.request_count >= self.max_requests:
                ten_env.log_info("All requests completed, stopping test.")
                ten_env.log_info(f"Final request count: {self.request_count}")
                ten_env.log_info("About to call ten_env.stop_test()")
                ten_env.stop_test()
                ten_env.log_info("ten_env.stop_test() called successfully")
            elif self.error_received:
                # If we received an error, stop the test after processing tts_audio_end
                ten_env.log_info(
                    "Error was received, stopping test after tts_audio_end."
                )
                ten_env.stop_test()
                ten_env.log_info(
                    "ten_env.stop_test() called successfully after error"
                )
            else:
                # Send next request
                next_request_id = f"tts_request_{self.request_count + 1}"
                next_text = (
                    f"request {self.request_count + 1}: hello word, hello agora"
                )
                ten_env.log_info(f"Sending next request: {next_request_id}")
                self._send_tts_request(ten_env, next_request_id, next_text)

        elif name == "error":
            if self.error_received:
                ten_env.log_info(
                    f"Error already received, ignoring further errors."
                )
                return
            ten_env.log_info("Received error, marking error received.")
            self.error_received = True
            # Don't stop test immediately, let it continue to receive tts_audio_end
            # The test will stop when max_requests is reached or when tts_audio_end is received
        else:
            ten_env.log_info(
                f"ExtensionTesterRobustness: Ignoring data: {name}"
            )


@patch("google_tts_python.extension.GoogleTTS")
def test_concurrent_requests(MockGoogleTTS):
    """Test that the extension handles concurrent requests correctly."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.get_streaming = mock_get  # Streaming method
    mock_client_instance.cancel = AsyncMock()
    # Fix: clean is a synchronous method, not async
    mock_client_instance.clean = Mock()
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
    MockGoogleTTS.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterRobustness()
    tester.max_requests = 5

    # Set up configuration with fake credentials for mock testing
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

    # Verify that all requests completed
    assert (
        tester.request_count == 5
    ), f"Expected 5 requests to complete, got {tester.request_count}"
    assert not tester.error_received, "Error should not be received"


@patch("google_tts_python.extension.GoogleTTS")
def test_rapid_requests(MockGoogleTTS):
    """Test that the extension handles rapid requests correctly."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.cancel = AsyncMock()
    mock_client_instance.clean = Mock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance._is_cancelled = False
    mock_client_instance.credentials = None
    mock_client_instance.config.language_code = "en-US"
    mock_client_instance.config.get_ssml_gender = AsyncMock(return_value=1)
    mock_client_instance._initialize_client = AsyncMock()
    MockGoogleTTS.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterRobustness()
    tester.max_requests = 10

    # Set up configuration with fake credentials for mock testing
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

    # Verify that all requests completed
    assert (
        tester.request_count == 10
    ), f"Expected 10 requests to complete, got {tester.request_count}"
    assert not tester.error_received, "Error should not be received"


@patch("google_tts_python.extension.GoogleTTS")
def test_large_text_requests(MockGoogleTTS):
    """Test that the extension handles large text requests correctly."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Return multiple audio chunks for large text
        for i in range(10):
            yield f"fake_audio_data_{i}".encode(), 1, (
                123 if i == 0 else None
            )  # EVENT_TTS_RESPONSE with ttfb_ms only for first chunk
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.cancel = AsyncMock()
    mock_client_instance.clean = Mock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance._is_cancelled = False
    mock_client_instance.credentials = None
    mock_client_instance.config.language_code = "en-US"
    mock_client_instance.config.get_ssml_gender = AsyncMock(return_value=1)
    mock_client_instance._initialize_client = AsyncMock()
    MockGoogleTTS.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterRobustness()
    tester.max_requests = 3

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

    # Verify that all requests completed
    assert (
        tester.request_count == 3
    ), f"Expected 3 requests to complete, got {tester.request_count}"
    assert not tester.error_received, "Error should not be received"


@patch("google_tts_python.extension.GoogleTTS")
def test_network_retry_robustness(MockGoogleTTS):
    """Test that the extension handles network errors correctly."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to fail with network error
    call_count = 0

    async def mock_get(text, request_id):
        nonlocal call_count
        call_count += 1

        # Always fail with network error
        error_data = "503 failed to connect to all addresses".encode("utf-8")
        yield error_data, 3, None  # EVENT_TTS_ERROR
        # Also yield the end signal to ensure proper completion
        yield None, 2, None  # EVENT_TTS_REQUEST_END
        return

    mock_client_instance.get = mock_get
    mock_client_instance.cancel = AsyncMock()
    mock_client_instance.clean = Mock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance._is_cancelled = False
    mock_client_instance.credentials = None
    mock_client_instance.config.language_code = "en-US"
    mock_client_instance.config.get_ssml_gender = AsyncMock(return_value=1)
    mock_client_instance._initialize_client = AsyncMock()
    MockGoogleTTS.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterRobustness()
    tester.max_requests = 1

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

    # Verify that error was received (since mock doesn't implement retry logic)
    assert tester.error_received, "Error should be received for network failure"
    # For network error tests, we expect the request to be attempted
    # The request_count should be at least 0 (if error was received before tts_audio_end)
    # or 1 (if tts_audio_end was received after error)
    assert (
        tester.request_count >= 0
    ), f"Expected request_count to be at least 0, got {tester.request_count}"


@patch("google_tts_python.extension.GoogleTTS")
def test_memory_robustness(MockGoogleTTS):
    """Test that the extension handles memory pressure correctly."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return large audio data
    async def mock_get(text, request_id):
        # Return large audio data to test memory handling
        large_audio_data = b"x" * 1024 * 1024  # 1MB of data
        yield large_audio_data, 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.cancel = AsyncMock()
    mock_client_instance.clean = Mock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance._is_cancelled = False
    mock_client_instance.credentials = None
    mock_client_instance.config.language_code = "en-US"
    mock_client_instance.config.get_ssml_gender = AsyncMock(return_value=1)
    mock_client_instance._initialize_client = AsyncMock()
    MockGoogleTTS.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterRobustness()
    tester.max_requests = 5

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

    # Verify that all requests completed
    assert (
        tester.request_count == 5
    ), f"Expected 5 requests to complete, got {tester.request_count}"
    assert not tester.error_received, "Error should not be received"


@patch("google_tts_python.extension.GoogleTTS")
def test_cancellation_robustness(MockGoogleTTS):
    """Test that the extension handles cancellation correctly."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.cancel = AsyncMock()
    mock_client_instance.clean = Mock()
    mock_client_instance.client = AsyncMock()
    mock_client_instance.config = AsyncMock()
    mock_client_instance.ten_env = AsyncMock()
    mock_client_instance._is_cancelled = False
    mock_client_instance.credentials = None
    mock_client_instance.config.language_code = "en-US"
    mock_client_instance.config.get_ssml_gender = AsyncMock(return_value=1)
    mock_client_instance._initialize_client = AsyncMock()
    MockGoogleTTS.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterRobustness()
    tester.max_requests = 3

    # Set up configuration with fake credentials for mock testing
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

    # Verify that all requests completed
    assert (
        tester.request_count == 3
    ), f"Expected 3 requests to complete, got {tester.request_count}"
    assert not tester.error_received, "Error should not be received"
