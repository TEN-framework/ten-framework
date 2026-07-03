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


class ExtensionTesterMetrics(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_start_received = False
        self.ttfb_metrics_received = False
        self.audio_end_received = False
        self.ttfb_value = None
        self.audio_start_time = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info("Metrics test started, sending TTS request.")

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
        if name == "tts_audio_start":
            ten_env.log_info("Received tts_audio_start")
            self.audio_start_received = True

        elif name == "metrics":
            json_str, _ = data.get_property_to_json("")
            metrics_data = json.loads(json_str) if json_str else {}
            ten_env.log_info(f"Received metrics: {metrics_data}")

            if "ttfb" in metrics_data.get("metrics", {}):
                self.ttfb_metrics_received = True
                self.ttfb_value = metrics_data.get("metrics", {}).get("ttfb", 0)
                ten_env.log_info(f"TTFB value: {self.ttfb_value}")

        elif name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end")
            self.audio_end_received = True
            ten_env.stop_test()


@patch("google_tts_python.extension.GoogleTTS")
def test_ttfb_metrics(MockGoogleTTS):
    """Test that the extension sends TTFB metrics correctly."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data with delay
    async def mock_get(text, request_id):
        # Add delay to ensure TTFB calculation works
        await asyncio.sleep(0.01)
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
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
    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterMetrics()

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

    # Verify that all events were received
    assert tester.audio_start_received, "Audio start event was not received"
    assert tester.ttfb_metrics_received, "TTFB metrics were not received"
    assert tester.audio_end_received, "Audio end event was not received"

    # Verify that TTFB value is reasonable (should be > 0 and < 10000ms)
    assert tester.ttfb_value is not None, "TTFB value is None"
    assert (
        tester.ttfb_value > 0
    ), f"TTFB value should be > 0, got {tester.ttfb_value}"
    assert (
        tester.ttfb_value < 10000
    ), f"TTFB value should be < 10000ms, got {tester.ttfb_value}"


@patch("google_tts_python.extension.GoogleTTS")
def test_audio_timing_metrics(MockGoogleTTS):
    """Test that the extension sends audio timing metrics correctly."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(tex, request_id):
        # Add delay to ensure TTFB calculation works
        await asyncio.sleep(0.01)
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
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
    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterMetrics()

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

    # Verify that all events were received
    assert tester.audio_start_received, "Audio start event was not received"
    assert tester.audio_end_received, "Audio end event was not received"


@patch("google_tts_python.extension.GoogleTTS")
def test_metrics_with_long_text(MockGoogleTTS):
    """Test that the extension sends metrics correctly with long text."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data with multiple chunks
    async def mock_get(text, request_id):
        # Add delay to ensure TTFB calculation works
        await asyncio.sleep(0.01)
        # Return multiple audio chunks to simulate long text
        for i in range(5):
            yield f"fake_audio_data_{i}".encode(), 1, (
                123 if i == 0 else None
            )  # EVENT_TTS_RESPONSE with ttfb_ms only for first chunk
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
    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterMetrics()

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

    # Verify that all events were received
    assert tester.audio_start_received, "Audio start event was not received"
    assert tester.ttfb_metrics_received, "TTFB metrics were not received"
    assert tester.audio_end_received, "Audio end event was not received"

    # Verify that TTFB value is reasonable
    assert tester.ttfb_value is not None, "TTFB value is None"
    assert (
        tester.ttfb_value > 0
    ), f"TTFB value should be > 0, got {tester.ttfb_value}"


@patch("google_tts_python.extension.GoogleTTS")
def test_metrics_with_fast_response(MockGoogleTTS):
    """Test that the extension sends metrics correctly with fast response."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data immediately
    async def mock_get(text, request_id):
        # Add delay to ensure TTFB calculation works
        await asyncio.sleep(0.01)
        # Return audio data immediately (no delay)
        yield b"fake_audio_data", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
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
    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterMetrics()

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

    # Verify that all events were received
    assert tester.audio_start_received, "Audio start event was not received"
    assert tester.ttfb_metrics_received, "TTFB metrics were not received"
    assert tester.audio_end_received, "Audio end event was not received"

    # Verify that TTFB value is reasonable (should be very low for fast response)
    assert tester.ttfb_value is not None, "TTFB value is None"
    assert (
        tester.ttfb_value >= 0
    ), f"TTFB value should be >= 0, got {tester.ttfb_value}"
    assert (
        tester.ttfb_value < 1000
    ), f"TTFB value should be < 1000ms for fast response, got {tester.ttfb_value}"


@patch("google_tts_python.extension.GoogleTTS")
def test_metrics_with_flush(MockGoogleTTS):
    """Test that the extension handles metrics correctly when flush occurs."""
    # Mock the GoogleTTS class
    mock_client_instance = AsyncMock()

    # Mock the get method to return audio data
    async def mock_get(text, request_id):
        # Add delay to ensure TTFB calculation works
        await asyncio.sleep(0.01)
        # Return audio data in the expected format for streaming
        yield b"fake_audio_data", 1, 123  # EVENT_TTS_RESPONSE with ttfb_ms
        # After flush, should not continue

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
    # Mock the _initialize_client method to avoid actual initialization
    mock_client_instance._initialize_client = AsyncMock()

    # Create and run the tester
    tester = ExtensionTesterMetrics()

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

    # Verify that audio start and TTFB metrics were received
    assert tester.audio_start_received, "Audio start event was not received"
    assert tester.ttfb_metrics_received, "TTFB metrics were not received"

    # Verify that TTFB value is reasonable
    assert tester.ttfb_value is not None, "TTFB value is None"
    assert (
        tester.ttfb_value > 0
    ), f"TTFB value should be > 0, got {tester.ttfb_value}"
