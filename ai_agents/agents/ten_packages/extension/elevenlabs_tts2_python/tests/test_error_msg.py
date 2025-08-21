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


class ExtensionTesterInvalidParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None
        self.vendor_info = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request to trigger the logic."""
        ten_env_tester.log_info(
            "Test started, sending TTS request to trigger mocked error"
        )

        tts_input = TTSTextInput(
            request_id="test-request-for-invalid-params",
            text="This text will trigger the mocked error.",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)

            self.error_code = error_data.get("code")
            self.error_message = error_data.get("message", "")
            self.error_module = error_data.get("module", "")
            self.vendor_info = error_data.get("vendor_info", {})

            ten_env.log_info(
                f"Received error: code={self.error_code}, message={self.error_message}, module={self.error_module}"
            )
            ten_env.log_info(f"Vendor info: {self.vendor_info}")

            ten_env.stop_test()


@patch("elevenlabs_tts2_python.elevenlabs_tts.ElevenLabsTTS2Client")
def test_vendor_exception_handling(MockElevenLabsTTS2Client):
    """Test that an error from the TTS client is handled correctly with a mock."""

    print("Starting test_vendor_exception_handling with mock...")

    # --- Mock Configuration ---
    mock_instance = MockElevenLabsTTS2Client.return_value
    mock_instance.start_connection = AsyncMock()
    mock_instance.text_to_speech_ws_streaming = AsyncMock()
    mock_instance.close = AsyncMock()

    # Mock text_to_speech_ws_streaming to raise an exception
    async def mock_tts_with_error(text: str):
        vendor_info = ModuleErrorVendorInfo(
            vendor="elevenlabs",
            code="40000",
            message="Invalid voice or parameters",
        )
        raise ModuleVendorException(vendor_info)

    mock_instance.text_to_speech_ws_streaming.side_effect = mock_tts_with_error

    # Mock the response_msgs queue
    mock_response_msgs = AsyncMock()
    mock_response_msgs.get = AsyncMock()
    mock_response_msgs.get.side_effect = asyncio.CancelledError(
        "Mock cancelled for error test"
    )
    mock_instance.response_msgs = mock_response_msgs

    MockElevenLabsTTS2Client.return_value = mock_instance

    # --- Test Setup ---
    tester = ExtensionTesterInvalidParams()
    tester.set_test_mode_single("elevenlabs_tts2_python")

    print("Running test with mock...")
    tester.run()
    print("Test with mock completed.")

    # --- Assertions ---
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_code is not None, "Error code should not be None"
    assert tester.error_message is not None, "Error message should not be None"
    assert len(tester.error_message) > 0, "Error message should not be empty"

    print(
        f"✅ Vendor exception test passed with mock: code={tester.error_code}, message={tester.error_message}"
    )
    print(f"✅ Vendor info: {tester.vendor_info}")
    print("Test verification completed successfully.")


# @patch("elevenlabs_tts2_python.elevenlabs_tts.ElevenLabsTTS2Client")
# def test_general_exception_handling(MockElevenLabsTTS2Client):
#     """Test that the extension handles general exceptions correctly."""
#     # Mock the ElevenLabsTTS2 class to raise a general exception
#     mock_client_instance = AsyncMock()

#     # Mock the start_connection method to raise a general exception
#     mock_client_instance.start_connection.side_effect = Exception(
#         "Test general error"
#     )

#     # Mock the response_msgs queue to prevent ValueError in _loop
#     mock_response_msgs = AsyncMock()
#     mock_response_msgs.get = AsyncMock()
#     mock_response_msgs.get.side_effect = asyncio.CancelledError("Mock cancelled for error test")
#     mock_client_instance.response_msgs = mock_response_msgs

#     MockElevenLabsTTS2Client.return_value = mock_client_instance

#     # Create and run the tester
#     tester = ExtensionTesterErrorHandling()
#     tester.set_test_mode_single("elevenlabs_tts2_python")
#     tester.run()

#     # Verify that error was received
#     assert tester.error_received, "Error was not received"
#     assert tester.error_data is not None, "Error data was not received"

#     # Verify error data structure
#     assert "id" in tester.error_data, "Error missing request_id"
#     assert "code" in tester.error_data, "Error missing code"
#     assert "message" in tester.error_data, "Error missing message"
#     assert "vendor_info" in tester.error_data, "Error missing vendor_info"

#     # Verify that vendor_info is empty for general exceptions
#     vendor_info = tester.error_data["vendor_info"]
#     assert (
#         vendor_info["vendor"] == ""
#     ), "Vendor should be empty for general exceptions"
#     assert (
#         vendor_info["code"] == ""
#     ), "Code should be empty for general exceptions"
#     assert (
#         vendor_info["message"] == ""
#     ), "Message should be empty for general exceptions"


# class ExtensionTesterFlushError(ExtensionTester):
#     def __init__(self):
#         super().__init__()
#         self.flush_error_received = False
#         self.flush_error_data = None
#         self.flush_sent = False
#         self.received_audio_chunks = []

#     def on_start(self, ten_env_tester: TenEnvTester) -> None:
#         """Called when test starts, sends a TTS request."""
#         ten_env_tester.log_info(
#             "Flush error test started, sending TTS request."
#         )

#         tts_input = TTSTextInput(
#             request_id="tts_request_1",
#             text="hello word, hello agora",
#             text_input_end=True,
#         )
#         data = Data.create("tts_text_input")
#         data.set_property_from_json(None, tts_input.model_dump_json())
#         ten_env_tester.send_data(data)
#         ten_env_tester.on_start_done()

#     def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
#         """Receives audio frames and sends flush after first chunk."""
#         buf = audio_frame.lock_buf()
#         try:
#             copied_data = bytes(buf)
#             self.received_audio_chunks.append(copied_data)

#             # Send flush after receiving first audio chunk
#             if len(self.received_audio_chunks) == 1 and not self.flush_sent:
#                 self.flush_sent = True
#                 ten_env.log_info(
#                     "Sending flush request after first audio chunk"
#                 )

#                 from ten_ai_base.struct import TTSFlush

#                 flush_input = TTSFlush(
#                     flush_id="tts_request_1",
#                 )
#                 flush_data = Data.create("tts_flush")
#                 flush_data.set_property_from_json(
#                     None, flush_input.model_dump_json()
#                 )
#                 ten_env.send_data(flush_data)
#         finally:
#             audio_frame.unlock_buf(buf)

#     def on_data(self, ten_env: TenEnvTester, data) -> None:
#         name = data.get_name()
#         if name == "error":
#             ten_env.log_info("Received error data")
#             self.flush_error_received = True
#             payload, _ = data.get_property_to_json("")
#             self.flush_error_data = json.loads(payload)
#             ten_env.stop_test()


# @patch("elevenlabs_tts2_python.elevenlabs_tts.ElevenLabsTTS2Client")
# def test_flush_error_handling(MockElevenLabsTTS2Client):
#     """Test that the extension handles flush errors correctly."""
#     # Mock the ElevenLabsTTS2 class
#     mock_client_instance = AsyncMock()

#     # Mock the start_connection method
#     mock_client_instance.start_connection = AsyncMock()

#     # Mock the text_input_queue to avoid blocking
#     mock_client_instance.text_input_queue = asyncio.Queue()

#     # 移除对不存在方法的mock

#     # Mock the response_msgs queue to return audio data
#     # Create a mock queue that simulates the behavior without binding to event loop
#     mock_response_msgs = AsyncMock()
#     mock_response_msgs.get = AsyncMock()
#     # Set up the mock to return audio data in sequence

#     # Create a counter to track calls and provide appropriate responses
#     call_count = 0
#     async def mock_get():
#         nonlocal call_count
#         call_count += 1
#         if call_count == 1:
#             return (b"fake_audio_data_1", False, "")
#         elif call_count == 2:
#             return (b"fake_audio_data_2", True, "hello word, hello agora")
#         else:
#             # After final response, simulate task cancellation to stop the loop
#             raise asyncio.CancelledError("Mock task cancelled after final response")

#     mock_response_msgs.get = mock_get
#     mock_client_instance.response_msgs = mock_response_msgs

#     # Mock the handle_flush method to raise an exception
#     mock_client_instance.handle_flush = AsyncMock()
#     mock_client_instance.handle_flush.side_effect = Exception(
#         "Test flush error"
#     )
#     mock_client_instance.reconnect_connection = AsyncMock()
#     mock_client_instance._handle_reconnection = AsyncMock()
#     MockElevenLabsTTS2Client.return_value = mock_client_instance

#     # Create and run the tester
#     tester = ExtensionTesterFlushError()
#     tester.set_test_mode_single("elevenlabs_tts2_python")
#     tester.run()

#     # Verify that error was received (due to flush)
#     assert tester.flush_error_received, "Flush error was not received"
#     assert (
#         tester.flush_error_data is not None
#     ), "Flush error data was not received"

#     # Verify error data structure
#     assert "id" in tester.flush_error_data, "Error missing request_id"
#     assert "code" in tester.flush_error_data, "Error missing code"
#     assert "message" in tester.flush_error_data, "Error missing message"


# class ExtensionTesterDuplicateRequestError(ExtensionTester):
#     def __init__(self):
#         super().__init__()
#         self.duplicate_error_received = False
#         self.duplicate_error_data = None

#     def on_start(self, ten_env_tester: TenEnvTester) -> None:
#         """Called when test starts, sends duplicate TTS requests."""
#         ten_env_tester.log_info(
#             "Duplicate request error test started, sending TTS requests."
#         )

#         # Send first request
#         tts_input1 = TTSTextInput(
#             request_id="tts_request_1",
#             text="hello word, hello agora",
#             text_input_end=True,
#         )
#         data1 = Data.create("tts_text_input")
#         data1.set_property_from_json(None, tts_input1.model_dump_json())
#         ten_env_tester.send_data(data1)

#         # Send second request with same request_id
#         tts_input2 = TTSTextInput(
#             request_id="tts_request_1",
#             text="this should cause an error",
#             text_input_end=True,
#         )
#         data2 = Data.create("tts_text_input")
#         data2.set_property_from_json(None, tts_input2.model_dump_json())
#         ten_env_tester.send_data(data2)

#         ten_env_tester.on_start_done()

#     def on_data(self, ten_env: TenEnvTester, data) -> None:
#         name = data.get_name()
#         if name == "error":
#             ten_env.log_info("Received error data")
#             self.duplicate_error_received = True
#             payload, _ = data.get_property_to_json("")
#             self.duplicate_error_data = json.loads(payload)
#             ten_env.stop_test()
