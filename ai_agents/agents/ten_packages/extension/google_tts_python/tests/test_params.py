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
from asyncio import QueueEmpty

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput


class ExtensionTesterParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.received_audio_chunks = []
        self.error_received = False
        self.error_message = ""

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info("Params test started, sending TTS request.")

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
        if name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end, stopping test.")
            self.audio_end_received = True
            ten_env.stop_test()
        elif name == "error":
            if self.error_received:
                ten_env.log_info(
                    f"Error already received, ignoring further errors."
                )
                return

            ten_env.log_info("Received error event.")
            self.error_received = True
            json_str, _ = data.get_property_to_json("")
            if json_str:
                import json

                error_data = json.loads(json_str)
                self.error_message = error_data.get("message", "")
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receives audio frames and collects their data."""
        buf = audio_frame.lock_buf()
        try:
            copied_data = bytes(buf)
            self.received_audio_chunks.append(copied_data)
        finally:
            audio_frame.unlock_buf(buf)


@patch("google_tts_python.extension.GoogleTTS")
def test_default_params(MockGoogleTTS):
    """Test that the extension works with default parameters."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data_1", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        yield b"fake_audio_data_2", 1, None  # EVENT_TTS_RESPONSE without ttfb_ms
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.get_streaming = mock_get  # Streaming method
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance._should_stop_streaming = False
    MockGoogleTTS.return_value = mock_client_instance

    # Set up required attributes
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

    # Create and run the tester
    tester = ExtensionTesterParams()

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

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"


@patch("google_tts_python.extension.GoogleTTS")
def test_custom_params(MockGoogleTTS):
    """Test that the extension works with custom parameters."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data_1", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        yield b"fake_audio_data_2", 1, None  # EVENT_TTS_RESPONSE without ttfb_ms
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.get_streaming = mock_get  # Streaming method
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance._should_stop_streaming = False
    MockGoogleTTS.return_value = mock_client_instance

    # Set up required attributes
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

    # Create and run the tester
    tester = ExtensionTesterParams()

    # Set up custom configuration
    config = {
        "params": {
            "AudioConfig": {"sample_rate_hertz": 16000},
            "credentials": "fake_credentials_for_mock_testing",
        },
    }

    tester.set_test_mode_single("google_tts_python", json.dumps(config))
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"


@patch("google_tts_python.extension.GoogleTTS")
def test_sample_rate_params(MockGoogleTTS):
    """Test that the extension works with different sample rates."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data_1", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        yield b"fake_audio_data_2", 1, None  # EVENT_TTS_RESPONSE without ttfb_ms
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.get_streaming = mock_get  # Streaming method
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance._should_stop_streaming = False
    MockGoogleTTS.return_value = mock_client_instance

    # Set up required attributes
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

    # Test different sample rates
    sample_rates = [8000, 16000, 24000, 48000]

    for sample_rate in sample_rates:
        # Create and run the tester
        tester = ExtensionTesterParams()

        # Set up configuration with specific sample rate
        config = {
            "params": {
                "AudioConfig": {"sample_rate_hertz": sample_rate},
                "VoiceSelectionParams": {
                    "language_code": "en-US",
                    "ssml_gender": "NEUTRAL",
                },
                "credentials": "fake_credentials_for_mock_testing",
            },
        }

        tester.set_test_mode_single("google_tts_python", json.dumps(config))
        tester.run()

        # Verify that audio end was received
        assert (
            tester.audio_end_received
        ), f"Audio end event was not received for sample rate {sample_rate}"


@patch("google_tts_python.extension.GoogleTTS")
def test_voice_params(MockGoogleTTS):
    """Test that the extension works with different voice parameters."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data_1", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        yield b"fake_audio_data_2", 1, None  # EVENT_TTS_RESPONSE without ttfb_ms
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.get_streaming = mock_get  # Streaming method
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance._should_stop_streaming = False
    MockGoogleTTS.return_value = mock_client_instance

    # Set up required attributes
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

    # Test different voice configurations
    voice_configs = [
        {
            "language_code": "en-US",
            "voice_name": "en-US-Standard-A",
            "ssml_gender": "FEMALE",
        },
        {
            "language_code": "en-US",
            "voice_name": "en-US-Standard-B",
            "ssml_gender": "MALE",
        },
        {
            "language_code": "zh-CN",
            "voice_name": "zh-CN-Standard-A",
            "ssml_gender": "FEMALE",
        },
        {
            "language_code": "ja-JP",
            "voice_name": "ja-JP-Standard-A",
            "ssml_gender": "NEUTRAL",
        },
    ]

    for voice_config in voice_configs:
        # Create and run the tester
        tester = ExtensionTesterParams()

        # Set up configuration with specific voice parameters
        config = {
            "params": {
                "AudioConfig": {"sample_rate_hertz": 16000},
                "VoiceSelectionParams": voice_config,
                "credentials": "fake_credentials_for_mock_testing",
            },
        }

        tester.set_test_mode_single("google_tts_python", json.dumps(config))
        tester.run()

        # Verify that audio end was received
        assert (
            tester.audio_end_received
        ), f"Audio end event was not received for voice config {voice_config}"


@patch("google_tts_python.extension.GoogleTTS")
def test_audio_params(MockGoogleTTS):
    """Test that the extension works with different audio parameters."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data_1", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        yield b"fake_audio_data_2", 1, None  # EVENT_TTS_RESPONSE without ttfb_ms
        yield None, 2, None  # EVENT_TTS_REQUEST_END

    mock_client_instance.get = mock_get
    mock_client_instance.get_streaming = mock_get  # Streaming method
    mock_client_instance.clean = AsyncMock()
    mock_client_instance.reset = AsyncMock()
    mock_client_instance._should_stop_streaming = False
    MockGoogleTTS.return_value = mock_client_instance

    # Set up required attributes
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

    # Test different audio parameters
    audio_configs = [
        {"speaking_rate": 0.5, "pitch": -5.0, "volume_gain_db": -3.0},
        {"speaking_rate": 1.0, "pitch": 0.0, "volume_gain_db": 0.0},
        {"speaking_rate": 2.0, "pitch": 5.0, "volume_gain_db": 3.0},
    ]

    for audio_config in audio_configs:
        # Create and run the tester
        tester = ExtensionTesterParams()

        # Set up configuration with specific audio parameters
        config = {
            "params": {
                "AudioConfig": {"sample_rate_hertz": 16000, **audio_config},
                "VoiceSelectionParams": {
                    "language_code": "en-US",
                    "ssml_gender": "NEUTRAL",
                },
                "credentials": "fake_credentials_for_mock_testing",
            },
        }

        tester.set_test_mode_single("google_tts_python", json.dumps(config))
        tester.run()

        # Verify that audio end was received
        assert (
            tester.audio_end_received
        ), f"Audio end event was not received for audio config {audio_config}"


@patch("google_tts_python.extension.GoogleTTS")
def test_missing_credentials(MockGoogleTTS):
    """Test that the extension handles missing credentials correctly."""
    # This test should receive an error event when credentials are missing
    # The extension should send a tts_error event during initialization

    # Create and run the tester
    tester = ExtensionTesterParams()

    # Set up configuration without credentials
    config = {
        "params": {
            "AudioConfig": {"sample_rate_hertz": 16000},
            "VoiceSelectionParams": {
                "language_code": "en-US",
                "ssml_gender": "NEUTRAL",
            },
        },
    }

    # Run the test - should receive error event
    tester.set_test_mode_single("google_tts_python", json.dumps(config))
    tester.run()

    # Verify that error event was received
    assert (
        tester.error_received
    ), "Error event was not received when credentials are missing"
    assert (
        "credentials" in tester.error_message.lower()
        or "configuration" in tester.error_message.lower()
    ), f"Expected credentials error message, got: {tester.error_message}"
    print(f"Test correctly received error: {tester.error_message}")
