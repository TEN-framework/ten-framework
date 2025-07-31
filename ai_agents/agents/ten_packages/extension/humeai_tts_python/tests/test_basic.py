import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
# The project root is 6 levels up from the parent directory of this file.
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from pathlib import Path
import json
from typing import Any
from unittest.mock import patch, AsyncMock, MagicMock
import tempfile
import os
import asyncio
import filecmp
import shutil
import threading
import base64

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    Data,
    TenError,
)
from ten_ai_base.struct import TTSTextInput, TTSFlush
from humeai_tts_python.humeTTS import (
    EVENT_TTS_RESPONSE,
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_INVALID_KEY_ERROR,
)

# ================ case 1: 基本功能测试 ================
class ExtensionTesterBasic(ExtensionTester):
    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
            return
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            ten_env.stop_test()

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        print("send hello_world")
        ten_env_tester.send_cmd(
            new_cmd,
            lambda ten_env, result, _: self.check_hello(ten_env, result),
        )

        print("tester on_start_done")
        ten_env_tester.on_start_done()

def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("humeai_tts_python")
    tester.run()

# ================ case 2: 空参数错误测试 ================
class ExtensionTesterEmptyParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts"""
        ten_env_tester.log_info("Empty params test started")
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

            ten_env.log_info(f"Received error: code={self.error_code}, message={self.error_message}, module={self.error_module}")
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()

def test_empty_params_fatal_error():
    """Test that empty params raises FATAL ERROR with code -1000"""
    print("Starting test_empty_params_fatal_error...")

    # Empty params configuration
    empty_params_config = {
        "params": {}
    }

    tester = ExtensionTesterEmptyParams()
    tester.set_test_mode_single(
        "humeai_tts_python",
        json.dumps(empty_params_config)
    )

    print("Running test...")
    error = tester.run()
    print("Test completed.")

    # Verify FATAL ERROR was received
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_code == -1000, f"Expected error code -1000 (FATAL_ERROR), got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert len(tester.error_message) > 0, "Error message should not be empty"

    print(f"✅ Empty params test passed: code={tester.error_code}, message={tester.error_message}")
    print("Test verification completed successfully.")

# ================ case 3: 无效API密钥测试 ================
class ExtensionTesterInvalidApiKey(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None
        self.vendor_info = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request to trigger the logic."""
        ten_env_tester.log_info("Invalid API key test started, sending TTS request")

        tts_input = TTSTextInput(
            request_id="test-request-invalid-key",
            text="This text will trigger API key validation.",
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

            ten_env.log_info(f"Received error: code={self.error_code}, message={self.error_message}")
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()

@patch('humeai_tts_python.humeTTS.AsyncHumeClient')
def test_invalid_api_key_error(MockHumeClient):
    """Test that an invalid API key is handled correctly with a mock."""
    print("Starting test_invalid_api_key_error with mock...")

    # Mock the Hume client to raise an authentication error
    mock_client = MockHumeClient.return_value

    # Define an async generator that raises the invalid key exception
    async def mock_tts_error(context=None, utterances=None, format=None, instant_mode=None):
        error_msg = "headers: {'date': 'Thu, 31 Jul 2025 06:47:59 GMT', 'content-type': 'application/json', 'content-length': '90', 'connection': 'keep-alive', 'x-request-id': '9bccec6c-dd26-4b2d-b99b-a9e2a305e0a4', 'via': '1.1 google', 'cf-cache-status': 'DYNAMIC', 'server': 'cloudflare', 'cf-ray': '967b25c22ed66837-NRT'}, status_code: 401, body: {'fault': {'faultstring': 'Invalid ApiKey', 'detail': {'errorcode': 'oauth.v2.InvalidApiKey'}}}"
        raise Exception(error_msg)
        yield  # Unreachable, but makes this an async generator function

    mock_client.tts.synthesize_json_streaming = mock_tts_error

    # Config with invalid API key
    invalid_key_config = {
        "key": "invalid_api_key_test",
        "voice_id": "daisy",
        "params": {}
    }

    tester = ExtensionTesterInvalidApiKey()
    tester.set_test_mode_single(
        "humeai_tts_python",
        json.dumps(invalid_key_config)
    )

    print("Running test with mock...")
    tester.run()
    print("Test with mock completed.")

    # Verify FATAL ERROR was received for invalid API key
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_code == -1000, f"Expected error code -1000 (FATAL_ERROR), got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert "Invalid ApiKey" in tester.error_message, "Error message should mention Invalid ApiKey"

    # Verify vendor_info
    vendor_info = tester.vendor_info
    assert vendor_info is not None, "Expected vendor_info to be present"
    assert vendor_info.get("vendor") == "humeai", f"Expected vendor 'humeai', got {vendor_info.get('vendor')}"

    print(f"✅ Invalid API key test passed: code={tester.error_code}, message={tester.error_message}")

# ================ case 4: 音频转储功能测试 ================
class ExtensionTesterDump(ExtensionTester):
    def __init__(self):
        super().__init__()
        # Use a fixed path as requested by the user.
        self.dump_dir = "./dump/"
        # Use a unique name for the file generated by the test to avoid collision
        # with the file generated by the extension.
        self.test_dump_file_path = os.path.join(self.dump_dir, "test_hume_manual_dump.pcm")
        self.audio_end_received = False
        self.received_audio_chunks = []

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info("Dump test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_dump",
            text="hello world, testing audio dump functionality",
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

    def write_test_dump_file(self):
        """Writes the collected audio chunks to a file."""
        os.makedirs(self.dump_dir, exist_ok=True)
        with open(self.test_dump_file_path, 'wb') as f:
            for chunk in self.received_audio_chunks:
                f.write(chunk)

    def find_tts_dump_file(self) -> str | None:
        """Find the dump file created by the TTS extension in the fixed dump directory."""
        if not os.path.exists(self.dump_dir):
            return None
        for filename in os.listdir(self.dump_dir):
            if filename.endswith(".pcm") and filename != os.path.basename(self.test_dump_file_path):
                return os.path.join(self.dump_dir, filename)
        return None

@patch('humeai_tts_python.humeTTS.AsyncHumeClient')
def test_dump_functionality(MockHumeClient):
    """Tests that the dump file from the TTS extension matches the audio received by the test extension."""
    print("Starting test_dump_functionality with mock...")

    # --- Directory Setup ---
    # As requested, use a fixed './dump/' directory.
    DUMP_PATH = "./dump/"

    # Clean up directory before the test, in case of previous failed runs.
    if os.path.exists(DUMP_PATH):
        shutil.rmtree(DUMP_PATH)
    os.makedirs(DUMP_PATH)

    # --- Mock Configuration ---
    mock_client = MockHumeClient.return_value

    # Create some fake audio data to be streamed
    fake_audio_chunk_1 = b'\x11\x22\x33\x44' * 20  # 80 bytes
    fake_audio_chunk_2 = b'\xAA\xBB\xCC\xDD' * 20  # 80 bytes

    # This async generator simulates the Hume TTS client's response
    async def mock_tts_stream(context=None, utterances=None, format=None, instant_mode=None):
        # First chunk
        mock_snippet_1 = MagicMock()
        mock_snippet_1.generation_id = "test_gen_id"
        mock_snippet_1.audio = base64.b64encode(fake_audio_chunk_1).decode('utf-8')
        mock_snippet_1.is_last_chunk = False
        yield mock_snippet_1

        # Second chunk
        mock_snippet_2 = MagicMock()
        mock_snippet_2.generation_id = "test_gen_id"
        mock_snippet_2.audio = base64.b64encode(fake_audio_chunk_2).decode('utf-8')
        mock_snippet_2.is_last_chunk = True
        yield mock_snippet_2

    mock_client.tts.synthesize_json_streaming = mock_tts_stream

    # --- Test Setup ---
    tester = ExtensionTesterDump()

    dump_config = {
        "key": "test_api_key",
        "voice_id": "daisy",
        "dump": True,
        "dump_path": DUMP_PATH,
        "params": {}
    }

    tester.set_test_mode_single(
        "humeai_tts_python",
        json.dumps(dump_config)
    )

    print("Running dump test...")
    tester.run()
    print("Dump test completed.")

    # --- Verification ---
    # 1. Verify audio end was received
    assert tester.audio_end_received, "Expected to receive tts_audio_end"
    assert len(tester.received_audio_chunks) > 0, "Expected to receive audio chunks"

    # 2. Write received audio chunks to test file for comparison
    tester.write_test_dump_file()

    # 3. Find the dump file created by the extension
    tts_dump_file = tester.find_tts_dump_file()
    assert tts_dump_file is not None, f"Expected to find a TTS dump file in {DUMP_PATH}"
    assert os.path.exists(tts_dump_file), f"TTS dump file should exist: {tts_dump_file}"

    # 4. Compare the files
    print(f"Comparing test file {tester.test_dump_file_path} with TTS dump file {tts_dump_file}")
    assert filecmp.cmp(tester.test_dump_file_path, tts_dump_file, shallow=False), \
        "Test dump file and TTS dump file should have the same content"

    print(f"✅ Dump functionality test passed: received {len(tester.received_audio_chunks)} audio chunks")
    print(f"   Test file: {tester.test_dump_file_path}")
    print(f"   TTS dump file: {tts_dump_file}")

    # --- Cleanup ---
    if os.path.exists(DUMP_PATH):
        shutil.rmtree(DUMP_PATH)


# ================ case 5 ================
class ExtensionTesterMetrics(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ttfb_received = False
        self.ttfb_value = -1
        self.audio_frame_received = False
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info("Metrics test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_for_metrics",
            text="hello, this is a metrics test.",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")
        if name == "metrics":
            json_str, _ = data.get_property_to_json(None)
            ten_env.log_info(f"Received metrics: {json_str}")
            metrics_data = json.loads(json_str)

            # According to the new structure, 'ttfb' is nested inside a 'metrics' object.
            nested_metrics = metrics_data.get("metrics", {})
            if "ttfb" in nested_metrics:
                self.ttfb_received = True
                self.ttfb_value = nested_metrics.get("ttfb", -1)
                ten_env.log_info(f"Received TTFB metric with value: {self.ttfb_value}")

        elif name == "tts_audio_end":
            self.audio_end_received = True
            # Stop the test only after both TTFB and audio end are received
            if self.ttfb_received:
                ten_env.log_info("Received tts_audio_end, stopping test.")
                ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receives audio frames and confirms the stream is working."""
        if not self.audio_frame_received:
             self.audio_frame_received = True
             ten_env.log_info("First audio frame received.")

@patch('humeai_tts_python.extension.HumeAiTTS')
def test_ttfb_metric_is_sent(MockHumeAiTTS):
    """
    Tests that a TTFB (Time To First Byte) metric is correctly sent after
    receiving the first audio chunk from the TTS service.
    """
    print("Starting test_ttfb_metric_is_sent with mock...")

    # --- Mock Configuration ---
    mock_instance = MockHumeAiTTS.return_value

    # This async generator simulates the TTS client's get() method with a delay
    # to produce a measurable TTFB.
    async def mock_get_audio_with_delay(text: str):
        # Simulate network latency or processing time before the first byte
        await asyncio.sleep(0.2)
        yield (b'\x11\x22\x33', EVENT_TTS_RESPONSE)
        # Simulate the end of the stream
        yield (None, EVENT_TTS_END)

    mock_instance.get.side_effect = mock_get_audio_with_delay

    # --- Test Setup ---
    # A minimal config is needed for the extension to initialize correctly.
    metrics_config = {
        "key": "test_api_key",
        "voice_id": "daisy",
        "params": {}
    }
    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single(
        "humeai_tts_python",
        json.dumps(metrics_config)
    )

    print("Running TTFB metrics test...")
    tester.run()
    print("TTFB metrics test completed.")

    # --- Assertions ---
    assert tester.audio_frame_received, "Did not receive any audio frame."
    assert tester.audio_end_received, "Did not receive the tts_audio_end event."
    assert tester.ttfb_received, "TTFB metric was not received."

    # Check if the TTFB value is reasonable. It should be slightly more than
    # the 0.2s delay we introduced. We check for >= 200ms.
    assert tester.ttfb_value >= 200, \
        f"Expected TTFB to be >= 200ms, but got {tester.ttfb_value}ms."

    print(f"✅ TTFB metric test passed. Received TTFB: {tester.ttfb_value}ms.")

# ================ case 7 ================
class ExtensionTesterForPassthrough(ExtensionTester):
    """A simple tester that just starts and stops, to allow checking constructor calls."""

    def __init__(self):
        super().__init__()
        self.tts_completed = False

    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
            return
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            # Send a simple TTS request to ensure client initialization
            tts_input = TTSTextInput(
                request_id="passthrough_test",
                text="test",
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, tts_input.model_dump_json())
            ten_env.send_data(data)

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        print("send hello_world")
        ten_env_tester.send_cmd(
            new_cmd,
            lambda ten_env, result, _: self.check_hello(ten_env, result),
        )

        print("tester on_start_done")
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_end" and not self.tts_completed:
            self.tts_completed = True
            ten_env.stop_test()

@patch('humeai_tts_python.extension.HumeAiTTS')
def test_params_passthrough(MockHumeAiTTS):
    """
    Tests that custom parameters passed in the configuration are correctly
    forwarded to the HumeAiTTS client constructor.
    """
    print("Starting test_params_passthrough with mock...")

    # --- Mock Configuration ---
    mock_instance = MockHumeAiTTS.return_value
    mock_instance.cancel = AsyncMock() # Required for clean shutdown in on_flush

    async def mock_get_audio_stream(text: str):
        yield (b'\x11\x22\x33', EVENT_TTS_RESPONSE)
        yield (None, EVENT_TTS_END)

    mock_instance.get.side_effect = mock_get_audio_stream

    # --- Test Setup ---
    # Define a configuration with custom parameters inside 'params'.
    # These are the parameters we expect to be "passed through".
    passthrough_params = {
        "speed": 1.5,
        "trailing_silence": 0.8,
        "custom_param": "test_value"
    }
    passthrough_config = {
        "key": "test_api_key",
        "voice_id": "daisy",
        "params": passthrough_params
    }

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single(
        "humeai_tts_python",
        json.dumps(passthrough_config)
    )

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    # --- Assertions ---
    # Check that the HumeAiTTS client was instantiated exactly once.
    MockHumeAiTTS.assert_called_once()

    # Get the arguments that the mock was called with.
    # The constructor is called with keyword arguments like config=...
    # so we inspect the keyword arguments dictionary.
    call_args, call_kwargs = MockHumeAiTTS.call_args
    called_config = call_kwargs['config']

    # Verify that the configuration object contains our expected parameters
    # Note: HumeAi uses update_params() to merge params into the config
    assert hasattr(called_config, 'speed'), "Config should have speed parameter"
    assert called_config.speed == 1.5, f"Expected speed to be 1.5, but got {called_config.speed}"
    assert hasattr(called_config, 'trailing_silence'), "Config should have trailing_silence parameter"
    assert called_config.trailing_silence == 0.8, f"Expected trailing_silence to be 0.8, but got {called_config.trailing_silence}"

    print("✅ Params passthrough test passed successfully.")
    print(f"✅ Verified config speed: {called_config.speed}, trailing_silence: {called_config.trailing_silence}")

# ================ case 8  ================
class ExtensionTesterTextInputEnd(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ten_env: TenEnvTester | None = None
        self.first_request_audio_end_received = False
        self.second_request_error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        self.ten_env = ten_env_tester
        ten_env_tester.log_info("TextInputEnd test started, sending first TTS request.")

        # 1. Send first request with text_input_end=True
        tts_input_1 = TTSTextInput(
            request_id="tts_request_1",
            text="hello world, hello agora",
            text_input_end=True
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_1.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def send_second_request(self):
        """Sends the second TTS request that should be ignored."""
        if self.ten_env is None:
            return

        self.ten_env.log_info("Sending second TTS request, expecting an error.")
        # 2. Send second request with text_input_end=False, which should be ignored
        tts_input_2 = TTSTextInput(
            request_id="tts_request_1",
            text="this should be ignored"
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_2.model_dump_json())
        self.ten_env.send_data(data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"Received data: {name}")

        if name == "tts_audio_end":
            json_str, _ = data.get_property_to_json(None)
            payload = json.loads(json_str) if json_str else {}
            ten_env.log_info(f"Received tts_audio_end: {payload}")
            if payload.get("request_id") == "tts_request_1" and not self.first_request_audio_end_received:
                ten_env.log_info("Received tts_audio_end for the first request.")
                self.first_request_audio_end_received = True
                self.send_second_request() # Now, send the second request that should fail
            return

        json_str, _ = data.get_property_to_json(None)
        if not json_str:
            return

        payload = json.loads(json_str)
        request_id = payload.get("id")

        if name == "error" and request_id == "tts_request_1":
            ten_env.log_info(f"Received expected error for the second request: {payload}")
            self.second_request_error_received = True
            self.error_code = payload.get("code")
            self.error_message = payload.get("message")
            self.error_module = payload.get("module")
            ten_env.stop_test()

@patch('humeai_tts_python.extension.HumeAiTTS')
def test_text_input_end_logic(MockHumeAiTTS):
    """
    Tests that after a request with text_input_end=True is processed,
    subsequent requests with the same request_id are ignored and trigger an error.
    """
    print("Starting test_text_input_end_logic with mock...")

    # --- Mock Configuration ---
    mock_instance = MockHumeAiTTS.return_value
    mock_instance.cancel = AsyncMock()

    async def mock_get_audio_stream(text: str):
        yield (b'\x11\x22\x33', EVENT_TTS_RESPONSE)
        yield (None, EVENT_TTS_END)

    mock_instance.get.side_effect = mock_get_audio_stream

    # --- Test Setup ---
    config = { "key": "test_api_key", "voice_id": "daisy" }
    tester = ExtensionTesterTextInputEnd()
    tester.set_test_mode_single(
        "humeai_tts_python",
        json.dumps(config)
    )

    print("Running text_input_end logic test...")
    tester.run()
    print("text_input_end logic test completed.")

    # --- Assertions ---
    assert tester.first_request_audio_end_received, "Did not receive tts_audio_end for the first request."
    assert tester.second_request_error_received, "Did not receive the expected error for the second request."
    assert tester.error_code == 1000, f"Expected error code 1000, but got {tester.error_code}"
    assert tester.error_message is not None and "Received a message for a finished request_id" in tester.error_message, "Error message is not as expected."

    print("✅ Text input end logic test passed successfully.")

# ================ case 9 ================
class ExtensionTesterFlush(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ten_env: TenEnvTester | None = None
        self.audio_start_received = False
        self.first_audio_frame_received = False
        self.flush_start_received = False
        self.audio_end_received = False
        self.flush_end_received = False
        self.audio_end_reason = ""
        self.total_audio_duration_from_event = 0
        self.received_audio_bytes = 0
        self.sample_rate = 48000  # Hume TTS sample rate
        self.bytes_per_sample = 2  # 16-bit
        self.channels = 1
        self.audio_received_after_flush_end = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        self.ten_env = ten_env_tester
        ten_env_tester.log_info("Flush test started, sending long TTS request.")
        tts_input = TTSTextInput(
            request_id="tts_request_for_flush",
            text="This is a very long text designed to generate a continuous stream of audio, providing enough time to send a flush command."
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        if self.flush_end_received:
            ten_env.log_error("Received audio frame after tts_flush_end!")
            self.audio_received_after_flush_end = True

        if not self.first_audio_frame_received:
            self.first_audio_frame_received = True
            ten_env.log_info("First audio frame received, sending flush data.")
            flush_data = Data.create("tts_flush")
            flush_data.set_property_from_json(None, TTSFlush(flush_id="tts_request_for_flush").model_dump_json())
            ten_env.send_data(flush_data)

        buf = audio_frame.lock_buf()
        try:
            self.received_audio_bytes += len(buf)
        finally:
            audio_frame.unlock_buf(buf)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "tts_audio_start":
            self.audio_start_received = True
            return

        json_str, _ = data.get_property_to_json(None)
        if not json_str:
            return
        payload = json.loads(json_str)
        ten_env.log_info(f"on_data payload: {payload}")

        if name == "tts_flush_start":
            self.flush_start_received = True
            return

        if name == "tts_audio_end":
            self.audio_end_received = True
            self.audio_end_reason = payload.get("reason")
            self.total_audio_duration_from_event = payload.get("request_total_audio_duration_ms")

        elif name == "tts_flush_end":
            self.flush_end_received = True

            def stop_test_later():
                ten_env.log_info("Waited after flush_end, stopping test now.")
                ten_env.stop_test()

            timer = threading.Timer(0.5, stop_test_later)
            timer.start()

    def get_calculated_audio_duration_ms(self) -> int:
        duration_sec = self.received_audio_bytes / (self.sample_rate * self.bytes_per_sample * self.channels)
        return int(duration_sec * 1000)


@patch('humeai_tts_python.extension.HumeAiTTS')
def test_flush_logic(MockHumeAiTTS):
    """
    Tests that sending a flush command during TTS streaming correctly stops
    the audio and sends the appropriate events.
    """
    print("Starting test_flush_logic with mock...")

    mock_instance = MockHumeAiTTS.return_value
    mock_instance.cancel = AsyncMock()

    async def mock_get_long_audio_stream(text: str):
        for _ in range(20):
            # In a real scenario, the cancel() call would set a flag.
            # We simulate this by checking the mock's 'called' status.
            if mock_instance.cancel.called:
                print("Mock detected cancel call, stopping stream.")
                break
            yield (b'\x11\x22\x33' * 100, EVENT_TTS_RESPONSE)
            await asyncio.sleep(0.1)

        # After being cancelled (or finishing), the stream must send EVENT_TTS_END
        yield (None, EVENT_TTS_END)

    mock_instance.get.side_effect = mock_get_long_audio_stream

    config = { "key": "test_api_key", "voice_id": "daisy" }
    tester = ExtensionTesterFlush()
    tester.set_test_mode_single(
        "humeai_tts_python",
        json.dumps(config)
    )

    print("Running flush logic test...")
    tester.run()
    print("Flush logic test completed.")

    assert tester.audio_start_received, "Did not receive tts_audio_start."
    assert tester.first_audio_frame_received, "Did not receive any audio frame."
    assert tester.flush_start_received, "Did not receive tts_flush_start."
    assert tester.audio_end_received, "Did not receive tts_audio_end."
    assert tester.flush_end_received, "Did not receive tts_flush_end."
    assert not tester.audio_received_after_flush_end, "Received audio after tts_flush_end."

    calculated_duration = tester.get_calculated_audio_duration_ms()
    event_duration = tester.total_audio_duration_from_event
    print(f"calculated_duration: {calculated_duration}, event_duration: {event_duration}")
    assert abs(calculated_duration - event_duration) < 10, \
        f"Mismatch in audio duration. Calculated: {calculated_duration}ms, From event: {event_duration}ms"

    print("✅ Flush logic test passed successfully.")

if __name__ == "__main__":
    # Run all tests
    print("Running all HumeAI TTS tests...")
    test_basic()
    test_empty_params_fatal_error()
    test_invalid_api_key_error()
    test_dump_functionality()
    test_flush_logic()
    test_ttfb_metric_is_sent()
    test_params_passthrough()
    test_text_input_end_logic()
    test_flush_logic()
    print("All tests completed!")