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
from ten_ai_base.message import ModuleVendorException, ModuleErrorVendorInfo


class ExtensionTesterErrorHandling(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_data = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info(
            "Error handling test started, sending TTS request."
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
            ten_env.log_info("Received error data")
            self.error_received = True
            payload, _ = data.get_property_to_json("")
            self.error_data = json.loads(payload)
            ten_env.stop_test()

        # If any data is received, it means on_init is successful, we can continue
        ten_env.log_info(f"Received data: {name}")


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_vendor_exception_handling(MockElevenLabsTTS2):
    """Test that the extension handles vendor exceptions correctly."""
    # Mock the ElevenLabsTTS2 class to raise an exception
    mock_client_instance = AsyncMock()

    # Mock the start_connection method to raise an exception
    error_info = ModuleErrorVendorInfo(
        vendor="elevenlabs", code="TEST_ERROR", message="Test error message"
    )
    mock_client_instance.start_connection.side_effect = ModuleVendorException(
        error_info
    )
    MockElevenLabsTTS2.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterErrorHandling()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that error was received
    assert tester.error_received, "Error was not received"
    assert tester.error_data is not None, "Error data was not received"

    # Verify error data structure
    assert "id" in tester.error_data, "Error missing request_id"
    assert "code" in tester.error_data, "Error missing code"
    assert "message" in tester.error_data, "Error missing message"


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_general_exception_handling(MockElevenLabsTTS2):
    """Test that the extension handles general exceptions correctly."""
    # Mock the ElevenLabsTTS2 class to raise a general exception
    mock_client_instance = AsyncMock()

    # Mock the start_connection method to raise a general exception
    mock_client_instance.start_connection.side_effect = Exception(
        "Test general error"
    )
    MockElevenLabsTTS2.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterErrorHandling()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that error was received
    assert tester.error_received, "Error was not received"
    assert tester.error_data is not None, "Error data was not received"

    # Verify error data structure
    assert "id" in tester.error_data, "Error missing request_id"
    assert "code" in tester.error_data, "Error missing code"
    assert "message" in tester.error_data, "Error missing message"
    assert "vendor_info" in tester.error_data, "Error missing vendor_info"

    # Verify that vendor_info is empty for general exceptions
    vendor_info = tester.error_data["vendor_info"]
    assert (
        vendor_info["vendor"] == ""
    ), "Vendor should be empty for general exceptions"
    assert (
        vendor_info["code"] == ""
    ), "Code should be empty for general exceptions"
    assert (
        vendor_info["message"] == ""
    ), "Message should be empty for general exceptions"


class ExtensionTesterFlushError(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.flush_error_received = False
        self.flush_error_data = None
        self.flush_sent = False
        self.received_audio_chunks = []

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info(
            "Flush error test started, sending TTS request."
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

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receives audio frames and sends flush after first chunk."""
        buf = audio_frame.lock_buf()
        try:
            copied_data = bytes(buf)
            self.received_audio_chunks.append(copied_data)

            # Send flush after receiving first audio chunk
            if len(self.received_audio_chunks) == 1 and not self.flush_sent:
                self.flush_sent = True
                ten_env.log_info(
                    "Sending flush request after first audio chunk"
                )

                from ten_ai_base.struct import TTSFlush

                flush_input = TTSFlush(
                    flush_id="tts_request_1",
                )
                flush_data = Data.create("tts_flush")
                flush_data.set_property_from_json(
                    None, flush_input.model_dump_json()
                )
                ten_env.send_data(flush_data)
        finally:
            audio_frame.unlock_buf(buf)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "error":
            ten_env.log_info("Received error data")
            self.flush_error_received = True
            payload, _ = data.get_property_to_json("")
            self.flush_error_data = json.loads(payload)
            ten_env.stop_test()


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_flush_error_handling(MockElevenLabsTTS2):
    """Test that the extension handles flush errors correctly."""
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

    # Mock the handle_flush method to raise an exception
    mock_client_instance.handle_flush = AsyncMock()
    mock_client_instance.handle_flush.side_effect = Exception(
        "Test flush error"
    )
    mock_client_instance.reconnect_connection = AsyncMock()
    mock_client_instance._handle_reconnection = AsyncMock()

    # Mock the get_synthesized_audio method to return audio data
    audio_data_queue = asyncio.Queue()
    # Pre-populate the queue - multiple chunks so flush can be triggered
    audio_data_queue.put_nowait([b"fake_audio_data_1", False, ""])
    audio_data_queue.put_nowait(
        [b"fake_audio_data_2", True, "hello word, hello agora"]
    )

    async def mock_get_synthesized_audio():
        try:
            return await audio_data_queue.get()
        except QueueEmpty:
            # If the queue is empty, return a special value to stop the loop
            return [None, True, "STOP_LOOP"]

    mock_client_instance.get_synthesized_audio = mock_get_synthesized_audio
    MockElevenLabsTTS2.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterFlushError()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that error was received (due to flush)
    assert tester.flush_error_received, "Flush error was not received"
    assert (
        tester.flush_error_data is not None
    ), "Flush error data was not received"

    # Verify error data structure
    assert "id" in tester.flush_error_data, "Error missing request_id"
    assert "code" in tester.flush_error_data, "Error missing code"
    assert "message" in tester.flush_error_data, "Error missing message"


class ExtensionTesterDuplicateRequestError(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.duplicate_error_received = False
        self.duplicate_error_data = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends duplicate TTS requests."""
        ten_env_tester.log_info(
            "Duplicate request error test started, sending TTS requests."
        )

        # Send first request
        tts_input1 = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
            text_input_end=True,
        )
        data1 = Data.create("tts_text_input")
        data1.set_property_from_json(None, tts_input1.model_dump_json())
        ten_env_tester.send_data(data1)

        # Send second request with same request_id
        tts_input2 = TTSTextInput(
            request_id="tts_request_1",
            text="this should cause an error",
            text_input_end=True,
        )
        data2 = Data.create("tts_text_input")
        data2.set_property_from_json(None, tts_input2.model_dump_json())
        ten_env_tester.send_data(data2)

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "error":
            ten_env.log_info("Received error data")
            self.duplicate_error_received = True
            payload, _ = data.get_property_to_json("")
            self.duplicate_error_data = json.loads(payload)
            ten_env.stop_test()


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_duplicate_request_error_handling(MockElevenLabsTTS2):
    """Test that the extension handles duplicate request errors correctly."""
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
    # Pre-populate the queue
    audio_data_queue.put_nowait(
        [b"fake_audio_data_1", True, "hello word, hello agora"]
    )

    async def mock_get_synthesized_audio():
        try:
            return await audio_data_queue.get()
        except QueueEmpty:
            # If the queue is empty, return a special value to stop the loop
            return [None, True, "STOP_LOOP"]

    mock_client_instance.get_synthesized_audio = mock_get_synthesized_audio
    MockElevenLabsTTS2.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterDuplicateRequestError()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    assert (
        tester.duplicate_error_data is not None
    ), "Duplicate request error data was not received"

    # Verify error data structure
    assert "id" in tester.duplicate_error_data, "Error missing request_id"
    assert "code" in tester.duplicate_error_data, "Error missing code"
    assert "message" in tester.duplicate_error_data, "Error missing message"
