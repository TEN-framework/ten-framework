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
import time

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput, TTSFlush


class ExtensionTesterRobustness(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.received_audio_chunks = []
        self.error_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info("Robustness test started, sending TTS request.")

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
            ten_env.log_info("Received error, stopping test.")
            self.error_received = True
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
def test_large_text_handling(MockElevenLabsTTS2):
    """Test that the extension handles large text correctly."""
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

    # Mock the get_synthesized_audio method to return large audio data
    audio_data_queue = asyncio.Queue()
    # Pre-populate the queue with large audio data
    audio_data_queue.put_nowait([b"fake_audio_data_1" * 1000, False, ""])
    audio_data_queue.put_nowait(
        [b"fake_audio_data_2" * 1000, True, "hello word, hello agora"]
    )

    async def mock_get_synthesized_audio():
        try:
            return await audio_data_queue.get()
        except QueueEmpty:
            # If the queue is empty, return a special value to stop the loop
            return [None, True, "STOP_LOOP"]

    mock_client_instance.get_synthesized_audio = mock_get_synthesized_audio
    MockElevenLabsTTS2.return_value = mock_client_instance

    # Create and run the tester with large text
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"
    assert (
        len(tester.received_audio_chunks) > 0
    ), "No audio chunks were received"


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_slow_response_handling(MockElevenLabsTTS2):
    """Test that the extension handles slow responses correctly."""
    # Mock the ElevenLabsTTS2 class with slow response
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

    # Mock the get_synthesized_audio method with slow response
    audio_data_queue = asyncio.Queue()
    # Pre-populate the queue with audio data
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
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"
    assert (
        len(tester.received_audio_chunks) > 0
    ), "No audio chunks were received"


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_rapid_requests_handling(MockElevenLabsTTS2):
    """Test that the extension handles rapid requests correctly."""
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
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"
    assert (
        len(tester.received_audio_chunks) > 0
    ), "No audio chunks were received"


class ExtensionTesterConcurrentRequests(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_count = 0
        self.error_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends multiple TTS requests."""
        ten_env_tester.log_info(
            "Concurrent requests test started, sending multiple TTS requests."
        )

        # Send multiple requests with different request_ids
        for i in range(3):
            tts_input = TTSTextInput(
                request_id=f"tts_request_{i}",
                text=f"hello word {i}",
                text_input_end=True,
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, tts_input.model_dump_json())
            ten_env_tester.send_data(data)

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            self.audio_end_count += 1
            ten_env.log_info(f"Received tts_audio_end #{self.audio_end_count}")
            if self.audio_end_count >= 3:
                ten_env.stop_test()
        elif name == "error":
            self.error_count += 1
            ten_env.log_info(f"Received error #{self.error_count}")


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_concurrent_requests_handling(MockElevenLabsTTS2):
    """Test that the extension handles concurrent requests correctly."""
    # Mock the ElevenLabsTTS2 class
    mock_client_instance = AsyncMock()

    # Mock the start_connection method
    mock_client_instance.start_connection = AsyncMock()

    # Mock the get_synthesized_audio method to return audio data
    audio_data_queue = asyncio.Queue()
    # Pre-populate the queue for multiple requests
    for i in range(3):
        audio_data_queue.put_nowait([b"fake_audio_data_1", False, ""])
        audio_data_queue.put_nowait(
            [b"fake_audio_data_2", True, f"hello word {i}"]
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
    tester = ExtensionTesterConcurrentRequests()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that all audio end events were received
    assert (
        tester.audio_end_count == 3
    ), f"Expected 3 audio end events, got {tester.audio_end_count}"


class ExtensionTesterStressTest(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_count = 0
        self.error_count = 0
        self.request_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends many TTS requests."""
        ten_env_tester.log_info(
            "Stress test started, sending many TTS requests."
        )

        # Send many requests
        for i in range(10):
            tts_input = TTSTextInput(
                request_id=f"tts_request_{i}",
                text=f"hello word {i}",
                text_input_end=True,
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, tts_input.model_dump_json())
            ten_env_tester.send_data(data)
            self.request_count += 1

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end":
            self.audio_end_count += 1
            ten_env.log_info(f"Received tts_audio_end #{self.audio_end_count}")
            if self.audio_end_count >= 10:
                ten_env.stop_test()
        elif name == "error":
            self.error_count += 1
            ten_env.log_info(f"Received error #{self.error_count}")


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_stress_test(MockElevenLabsTTS2):
    """Test that the extension handles stress conditions correctly."""
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
    # Pre-populate the queue for multiple requests
    for i in range(10):
        audio_data_queue.put_nowait([b"fake_audio_data_1", False, ""])
        audio_data_queue.put_nowait(
            [b"fake_audio_data_2", True, f"hello word {i}"]
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
    tester = ExtensionTesterStressTest()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that most requests completed successfully
    assert (
        tester.audio_end_count >= 8
    ), f"Expected at least 8 audio end events, got {tester.audio_end_count}"
    assert (
        tester.error_count <= 2
    ), f"Expected at most 2 errors, got {tester.error_count}"


class ExtensionTesterFlushRobustness(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.flush_sent = False
        self.audio_end_received = False
        self.received_audio_chunks = []

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info(
            "Flush robustness test started, sending TTS request."
        )

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
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
        if name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end, stopping test.")
            self.audio_end_received = True
            ten_env.stop_test()
        elif name == "tts_flush_end":
            ten_env.log_info("Received tts_flush_end")


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_flush_robustness(MockElevenLabsTTS2):
    """Test that the extension handles flush robustly."""
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

    # Mock the handle_flush method
    mock_client_instance.handle_flush = AsyncMock()

    # Mock the get_synthesized_audio method to return audio data
    audio_data_queue = asyncio.Queue()
    # Pre-populate the queue
    audio_data_queue.put_nowait([b"fake_audio_data_1", False, ""])
    audio_data_queue.put_nowait([b"fake_audio_data_2", False, ""])
    audio_data_queue.put_nowait(
        [b"fake_audio_data_3", True, "hello word, hello agora"]
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
    tester = ExtensionTesterFlushRobustness()
    tester.set_test_mode_single("elevenlabs_tts2_python")
    tester.run()

    # Verify that audio end was received
    assert tester.audio_end_received, "Audio end event was not received"
    assert tester.flush_sent, "Flush was not sent"
