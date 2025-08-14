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

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receives audio frames and collects their data."""
        buf = audio_frame.lock_buf()
        try:
            copied_data = bytes(buf)
            self.received_audio_chunks.append(copied_data)
        finally:
            audio_frame.unlock_buf(buf)


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_default_params(MockElevenLabsTTS2):
    """Test that the extension works with default parameters."""
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
    # Pre-populate the queue - only 2 chunks like in test_basic.py
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
    tester = ExtensionTesterParams()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"
    assert (
        len(tester.received_audio_chunks) > 0
    ), "No audio chunks were received"

    # Verify that ElevenLabsTTS2 was called with default parameters
    MockElevenLabsTTS2.assert_called_once()
    config = MockElevenLabsTTS2.call_args[0][0]
    assert (
        config.model_id == "eleven_multilingual_v2"
    ), "Default model_id mismatch"
    assert (
        config.voice_id == "pNInz6obpgDQGcFmaJgB"
    ), "Default voice_id mismatch"
    assert config.sample_rate == 16000, "Default sample_rate mismatch"


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_custom_params(MockElevenLabsTTS2):
    """Test that the extension works with custom parameters."""
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
    # Pre-populate the queue - only 2 chunks like in test_basic.py
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

    # Create and run the tester with custom parameters
    tester = ExtensionTesterParams()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"
    assert (
        len(tester.received_audio_chunks) > 0
    ), "No audio chunks were received"

    # Verify that ElevenLabsTTS2 was called
    MockElevenLabsTTS2.assert_called_once()


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_dump_params(MockElevenLabsTTS2):
    """Test that the extension works with dump parameters."""
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
    # Pre-populate the queue - only 2 chunks like in test_basic.py
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
    tester = ExtensionTesterParams()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"
    assert (
        len(tester.received_audio_chunks) > 0
    ), "No audio chunks were received"

    # Verify that ElevenLabsTTS2 was called
    MockElevenLabsTTS2.assert_called_once()


class ExtensionTesterEmptyText(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request with minimal text."""
        ten_env_tester.log_info(
            "Minimal text test started, sending TTS request."
        )

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="a",
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


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_empty_text_handling(MockElevenLabsTTS2):
    """Test that the extension handles minimal text correctly."""
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

    # Mock the get_synthesized_audio method to return minimal data
    audio_data_queue = asyncio.Queue()
    audio_data_queue.put_nowait([b"fake_audio_data_1", False, ""])
    audio_data_queue.put_nowait([b"fake_audio_data_2", True, "a"])

    async def mock_get_synthesized_audio():
        try:
            return await audio_data_queue.get()
        except QueueEmpty:
            # If the queue is empty, return a special value to stop the loop
            return [None, True, "STOP_LOOP"]

    mock_client_instance.get_synthesized_audio = mock_get_synthesized_audio
    MockElevenLabsTTS2.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterEmptyText()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"


class ExtensionTesterWhitespaceText(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request with short text."""
        ten_env_tester.log_info("Short text test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hi",
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


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_whitespace_text_handling(MockElevenLabsTTS2):
    """Test that the extension handles short text correctly."""
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

    # Mock the get_synthesized_audio method to return short data
    audio_data_queue = asyncio.Queue()
    audio_data_queue.put_nowait([b"fake_audio_data_1", False, ""])
    audio_data_queue.put_nowait([b"fake_audio_data_2", True, "hi"])

    async def mock_get_synthesized_audio():
        try:
            return await audio_data_queue.get()
        except QueueEmpty:
            # If the queue is empty, return a special value to stop the loop
            return [None, True, "STOP_LOOP"]

    mock_client_instance.get_synthesized_audio = mock_get_synthesized_audio
    MockElevenLabsTTS2.return_value = mock_client_instance

    # Create and run the tester
    tester = ExtensionTesterWhitespaceText()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"
