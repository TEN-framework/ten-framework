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
        self.ttfb_metrics_received = False
        self.audio_start_received = False
        self.audio_end_received = False
        self.text_result_received = False

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
        if name == "metrics":
            ten_env.log_info("Received metrics data")
            self.ttfb_metrics_received = True
        elif name == "tts_audio_start":
            ten_env.log_info("Received tts_audio_start")
            self.audio_start_received = True
        elif name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end")
            self.audio_end_received = True
            ten_env.stop_test()
        elif name == "tts_text_result":
            ten_env.log_info("Received tts_text_result")
            self.text_result_received = True


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_metrics_functionality(MockElevenLabsTTS2):
    """Test that the extension sends proper metrics."""
    # Mock the ElevenLabsTTS2 class
    mock_client_instance = AsyncMock()

    # Mock the start_connection method
    mock_client_instance.start_connection = AsyncMock()

    # Mock the text_input_queue to avoid blocking
    mock_client_instance.text_input_queue = asyncio.Queue()

    # Mock the text_to_speech_ws_streaming method to consume from queue
    async def mock_text_to_speech_ws_streaming():
        while True:
            try:
                await mock_client_instance.text_input_queue.get()
            except asyncio.CancelledError:
                break

    mock_client_instance.text_to_speech_ws_streaming = (
        mock_text_to_speech_ws_streaming
    )

    # Mock the get_synthesized_audio method to return audio data
    audio_data_queue = asyncio.Queue()
    # Pre-populate the queue - multiple chunks, only last one is final
    audio_data_queue.put_nowait([b"fake_audio_data_1", False, ""])
    audio_data_queue.put_nowait([b"fake_audio_data_2", False, ""])
    audio_data_queue.put_nowait(
        [b"fake_audio_data_3", True, "hello word, hello agora"]
    )

    async def mock_get_synthesized_audio():
        try:
            return await audio_data_queue.get()
        except asyncio.QueueEmpty:
            # If the queue is empty, return a special value to stop the loop
            return [None, True, "STOP_LOOP"]

    mock_client_instance.get_synthesized_audio = mock_get_synthesized_audio
    MockElevenLabsTTS2.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that all expected events were received
    assert tester.ttfb_metrics_received, "TTFB metrics were not received"
    assert tester.audio_start_received, "Audio start event was not received"
    assert tester.audio_end_received, "Audio end event was not received"


class ExtensionTesterMetricsWithMetadata(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.metrics_data = None
        self.audio_start_data = None
        self.audio_end_data = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request with metadata."""
        ten_env_tester.log_info(
            "Metrics with metadata test started, sending TTS request."
        )

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
            text_input_end=True,
            metadata={"session_id": "test_session_123", "turn_id": 456},
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "metrics":
            ten_env.log_info("Received metrics data")
            payload, _ = data.get_property_to_json("")
            self.metrics_data = json.loads(payload)
        elif name == "tts_audio_start":
            ten_env.log_info("Received tts_audio_start")
            payload, _ = data.get_property_to_json("")
            self.audio_start_data = json.loads(payload)
        elif name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end")
            payload, _ = data.get_property_to_json("")
            self.audio_end_data = json.loads(payload)
            ten_env.stop_test()


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_metrics_with_metadata(MockElevenLabsTTS2):
    """Test that the extension includes metadata in metrics."""
    # Mock the ElevenLabsTTS2 class
    mock_client_instance = AsyncMock()

    # Mock the start_connection method
    mock_client_instance.start_connection = AsyncMock()

    # Mock the text_input_queue to avoid blocking
    mock_client_instance.text_input_queue = asyncio.Queue()

    # Mock the text_to_speech_ws_streaming method to consume from queue
    async def mock_text_to_speech_ws_streaming():
        while True:
            try:
                await mock_client_instance.text_input_queue.get()
            except asyncio.CancelledError:
                break

    mock_client_instance.text_to_speech_ws_streaming = (
        mock_text_to_speech_ws_streaming
    )

    # Mock the get_synthesized_audio method to return audio data
    audio_data_queue = asyncio.Queue()
    # Pre-populate the queue - multiple chunks, only last one is final
    audio_data_queue.put_nowait([b"fake_audio_data_1", False, ""])
    audio_data_queue.put_nowait([b"fake_audio_data_2", False, ""])
    audio_data_queue.put_nowait(
        [b"fake_audio_data_3", True, "hello word, hello agora"]
    )

    async def mock_get_synthesized_audio():
        try:
            return await audio_data_queue.get()
        except asyncio.QueueEmpty:
            # If the queue is empty, return a special value to stop the loop
            return [None, True, "STOP_LOOP"]

    mock_client_instance.get_synthesized_audio = mock_get_synthesized_audio
    MockElevenLabsTTS2.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterMetricsWithMetadata()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that metrics data contains expected fields
    assert tester.metrics_data is not None, "Metrics data was not received"
    assert "id" in tester.metrics_data, "Metrics missing request_id"
    assert "module" in tester.metrics_data, "Metrics missing module"
    assert "vendor" in tester.metrics_data, "Metrics missing vendor"
    assert "metrics" in tester.metrics_data, "Metrics missing metrics object"
    assert "metadata" in tester.metrics_data, "Metrics missing metadata"

    # Verify metadata fields
    metadata = tester.metrics_data["metadata"]
    assert metadata["session_id"] == "test_session_123", "Session ID mismatch"
    assert metadata["turn_id"] == 456, "Turn ID mismatch"

    # Verify audio start data
    assert (
        tester.audio_start_data is not None
    ), "Audio start data was not received"
    assert (
        "request_id" in tester.audio_start_data
    ), "Audio start missing request_id"
    assert "metadata" in tester.audio_start_data, "Audio start missing metadata"

    # Verify audio end data
    assert tester.audio_end_data is not None, "Audio end data was not received"
    assert "request_id" in tester.audio_end_data, "Audio end missing request_id"
    assert (
        "request_event_interval_ms" in tester.audio_end_data
    ), "Audio end missing interval"
    assert (
        "request_total_audio_duration_ms" in tester.audio_end_data
    ), "Audio end missing duration"
    assert "metadata" in tester.audio_end_data, "Audio end missing metadata"
