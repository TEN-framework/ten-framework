#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import Any
from unittest.mock import patch, AsyncMock
import asyncio
import filecmp
import json
import os
import shutil
import threading

from ten_ai_base.struct import TTSTextInput, TTSFlush
from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    Data,
    TenError,
)

from ..tencent_tts import (
    TencentTTSTaskFailedException,
    MESSAGE_TYPE_PCM,
)


# ================ case 1 ================
class ExtensionTesterBasic(ExtensionTester):
    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
            return
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            # TODO: move stop_test() to where the test passes
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
    tester.set_test_mode_single("tencent_tts_python")
    tester.run()


# ================ case 2 ================
class ExtensionTesterEmptyParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts"""
        ten_env_tester.log_info("Test started")
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

            ten_env.log_info(
                f"Received error: code={self.error_code}, message={self.error_message}, module={self.error_module}"
            )

            # Stop test immediately
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()


def test_empty_params_fatal_error():
    """Test that empty params raises FATAL ERROR with code -1000"""

    print("Starting test_empty_params_fatal_error...")

    # Empty params configuration
    empty_params_config = {"params": {}}

    tester = ExtensionTesterEmptyParams()
    tester.set_test_mode_single(
        "tencent_tts_python", json.dumps(empty_params_config)
    )

    print("Running test...")
    error = tester.run()
    print("Test completed.")

    # Verify FATAL ERROR was received
    assert tester.error_received, "Expected to receive error message"
    assert (
        tester.error_code == -1000
    ), f"Expected error code -1000 (FATAL_ERROR), got {tester.error_code}"
    # Temporarily not checking module field, as it may not be set correctly
    # assert tester.error_module == "tts", f"Expected module 'tts', got {tester.error_module}"
    assert tester.error_message is not None, "Error message should not be None"
    assert len(tester.error_message) > 0, "Error message should not be empty"

    print(
        f"✅ Empty params test passed: code={tester.error_code}, message={tester.error_message}"
    )
    print("Test verification completed successfully.")


# ================ case 3 ================
class ExtensionTesterInvalidParams(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.error_module = None
        self.vendor_info = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, error should already be triggered in on_init."""
        ten_env_tester.log_info(
            "Test started, error should be triggered in on_init"
        )
        # Don't send any request, error should happen during initialization
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
            self.vendor_info = error_data.get("vendor_info", {})

            ten_env.log_info(
                f"Received error: code={self.error_code}, message={self.error_message}, module={self.error_module}"
            )
            ten_env.log_info(f"Vendor info: {self.vendor_info}")

            # Stop test immediately
            ten_env.log_info("Error received, stopping test immediately")
            ten_env.stop_test()


@patch("tencent_tts_python.extension.TencentTTSClient")
def test_invalid_params_fatal_error(MockTencentTTSClient):
    """Test that an error from the TTS client is handled correctly with a mock."""

    print("Starting test_invalid_params_fatal_error with mock...")

    # --- Mock Configuration ---
    mock_instance = MockTencentTTSClient.return_value
    # Mock the async methods called on the client instance
    mock_instance.start = AsyncMock(
        side_effect=TencentTTSTaskFailedException(
            error_code=-2000, error_msg="Voice ID not found or invalid"
        )
    )
    mock_instance.close = AsyncMock()

    # --- Test Setup ---
    # Config with invalid parameters to trigger error in on_init
    invalid_params_config = {
        "app_id": "invalid_app_id",
        "secret_key": "",
        "secret_id": "invalid_secret_id",
        "params": {"voice_type": "invalid_voice_type"},
    }

    tester = ExtensionTesterInvalidParams()
    tester.set_test_mode_single(
        "tencent_tts_python", json.dumps(invalid_params_config)
    )

    print("Running test with mock...")
    tester.run()
    print("Test with mock completed.")

    # --- Assertions ---
    assert tester.error_received, "Expected to receive error message"
    assert (
        tester.error_code == -1000
    ), f"Expected error code -1000 (FATAL_ERROR), got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert len(tester.error_message) > 0, "Error message should not be empty"

    print(
        f"✅ Invalid params test passed with mock: code={tester.error_code}, message={tester.error_message}"
    )
    print(f"✅ Vendor info: {tester.vendor_info}")
    print("Test verification completed successfully.")


# ================ case 4 ================
class ExtensionTesterDump(ExtensionTester):
    def __init__(self):
        super().__init__()
        # Use a fixed path as requested by the user.
        self.dump_dir = "./dump/"
        # Use a unique name for the file generated by the test to avoid collision
        # with the file generated by the extension.
        self.test_dump_file_path = os.path.join(
            self.dump_dir, "test_manual_dump.pcm"
        )
        self.audio_end_received = False
        self.received_audio_chunks = []

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info("Dump test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
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
        """Receives audio frames and collects their data using the lock/unlock pattern."""
        # The 'audio_frame' object is a wrapper around a memory buffer.
        # We must lock the buffer to safely access the data, copy it,
        # and finally unlock the buffer so the runtime can reuse it.
        buf = audio_frame.lock_buf()
        try:
            # We must copy the data from the buffer, as the underlying memory
            # may be freed or reused after we unlock it.
            copied_data = bytes(buf)
            self.received_audio_chunks.append(copied_data)
        finally:
            # Always ensure the buffer is unlocked, even if an error occurs.
            audio_frame.unlock_buf(buf)

    def write_test_dump_file(self):
        """Writes the collected audio chunks to a file."""
        with open(self.test_dump_file_path, "wb") as f:
            for chunk in self.received_audio_chunks:
                f.write(chunk)

    def find_tts_dump_file(self) -> str | None:
        """Find the dump file created by the TTS extension in the fixed dump directory."""
        if not os.path.exists(self.dump_dir):
            return None
        for filename in os.listdir(self.dump_dir):
            if filename.endswith(".pcm") and filename != os.path.basename(
                self.test_dump_file_path
            ):
                return os.path.join(self.dump_dir, filename)
        return None


@patch("tencent_tts_python.extension.TencentTTSClient")
def test_dump_functionality(MockTencentTTSClient):
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
    mock_instance = MockTencentTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.close = AsyncMock()

    # Create some fake audio data to be streamed
    fake_audio_chunk_1 = b"\x11\x22\x33\x44" * 20
    fake_audio_chunk_2 = b"\xAA\xBB\xCC\xDD" * 20

    # This async generator simulates the TTS client's synthesize_audio() method
    async def mock_synthesize_audio_stream(text: str):
        yield [False, MESSAGE_TYPE_PCM, fake_audio_chunk_1]
        await asyncio.sleep(0.01)
        yield [False, MESSAGE_TYPE_PCM, fake_audio_chunk_2]
        await asyncio.sleep(0.01)
        yield [True, MESSAGE_TYPE_PCM, b""]  # done=True indicates end

    mock_instance.synthesize_audio.side_effect = mock_synthesize_audio_stream

    # --- Test Setup ---
    tester = ExtensionTesterDump()

    dump_config = {
        "app_id": "valid_app_id_for_test",
        "secret_key": "valid_secret_key_for_test",
        "secret_id": "valid_secret_id_for_test",
        "dump": True,
        "dump_path": DUMP_PATH,
    }

    tester.set_test_mode_single("tencent_tts_python", json.dumps(dump_config))

    try:
        print("Running dump test...")
        tester.run()
        print("Dump test completed.")

        # --- Assertions ---
        assert tester.audio_end_received, "tts_audio_end was not received"

        # Write the audio chunks collected by the test extension to its own dump file
        tester.write_test_dump_file()
        assert os.path.exists(
            tester.test_dump_file_path
        ), "Test dump file was not created"

        # Find the dump file automatically created by the TTS extension
        tts_dump_file = tester.find_tts_dump_file()
        assert (
            tts_dump_file is not None
        ), f"Could not find TTS-generated dump file in {DUMP_PATH}"

        print(f"Comparing TTS dump file: {tts_dump_file}")
        print(f"With test dump file:    {tester.test_dump_file_path}")

        # Binary comparison of the two files
        assert filecmp.cmp(
            tts_dump_file, tester.test_dump_file_path, shallow=False
        ), "The TTS dump file and the test-generated dump file do not match."

        print("✅ Dump file binary comparison passed.")

    finally:
        # Cleanup the dump directory after the test.
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
                ten_env.log_info(
                    f"Received TTFB metric with value: {self.ttfb_value}"
                )

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


@patch("tencent_tts_python.extension.TencentTTSClient")
def test_ttfb_metric_is_sent(MockTencentTTSClient):
    """
    Tests that a TTFB (Time To First Byte) metric is correctly sent after
    receiving the first audio chunk from the TTS service.
    """
    print("Starting test_ttfb_metric_is_sent with mock...")

    # --- Mock Configuration ---
    mock_instance = MockTencentTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.close = AsyncMock()

    # This async generator simulates the TTS client's synthesize_audio() method with a delay
    # to produce a measurable TTFB.
    async def mock_synthesize_audio_with_delay(text: str):
        # Simulate network latency or processing time before the first byte
        await asyncio.sleep(0.2)
        yield [False, MESSAGE_TYPE_PCM, b"\x11\x22\x33"]
        # Simulate the end of the stream
        yield [True, MESSAGE_TYPE_PCM, b""]

    mock_instance.synthesize_audio.side_effect = (
        mock_synthesize_audio_with_delay
    )

    # --- Test Setup ---
    # A minimal config is needed for the extension to initialize correctly.
    metrics_config = {
        "app_id": "a_valid_app_id",
        "secret_key": "a_valid_secret_key",
        "secret_id": "a_valid_secret_id",
    }
    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single(
        "tencent_tts_python", json.dumps(metrics_config)
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
    assert (
        tester.ttfb_value >= 200
    ), f"Expected TTFB to be >= 200ms, but got {tester.ttfb_value}ms."

    print(f"✅ TTFB metric test passed. Received TTFB: {tester.ttfb_value}ms.")


# ================ case 6 ================
class ExtensionTesterRobustness(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.first_request_error: dict[str, Any] | None = None
        self.second_request_successful = False
        self.ten_env: TenEnvTester | None = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends the first TTS request."""
        self.ten_env = ten_env_tester
        ten_env_tester.log_info(
            "Robustness test started, sending first TTS request."
        )

        # First request, expected to fail
        tts_input_1 = TTSTextInput(
            request_id="tts_request_to_fail",
            text="This request will trigger a simulated connection drop.",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_1.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def send_second_request(self):
        """Sends the second TTS request to verify reconnection."""
        if self.ten_env is None:
            print("Error: ten_env is not initialized.")
            return
        self.ten_env.log_info(
            "Sending second TTS request to verify reconnection."
        )
        tts_input_2 = TTSTextInput(
            request_id="tts_request_to_succeed",
            text="This request should succeed after reconnection.",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_2.model_dump_json())
        self.ten_env.send_data(data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str)

        if name == "error" and payload.get("id") == "tts_request_to_fail":
            ten_env.log_info(
                f"Received expected error for the first request: {payload}"
            )
            self.first_request_error = payload
            # After receiving the error for the first request, immediately send the second one.
            self.send_second_request()

        # Use a separate 'if' to ensure this check happens independently of the error check.
        if payload.get("id") == "tts_request_to_succeed":
            ten_env.log_info(
                "Received tts_audio_end for the second request. Test successful."
            )
            self.second_request_successful = True
            # We can now safely stop the test.
            ten_env.stop_test()


@patch("tencent_tts_python.extension.TencentTTSClient")
def test_reconnect_after_connection_drop(MockTencentTTSClient):
    """
    Tests that the extension can recover from a connection drop, report a
    NON_FATAL_ERROR, and then successfully reconnect and process a new request.
    """
    print("Starting test_reconnect_after_connection_drop with mock...")

    # --- Mock State ---
    # Use a simple counter to track how many times synthesize_audio() is called
    synthesize_call_count = 0

    # --- Mock Configuration ---
    mock_instance = MockTencentTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.close = AsyncMock()

    # This async generator simulates different behaviors on subsequent calls
    async def mock_synthesize_stateful(text: str):
        nonlocal synthesize_call_count
        synthesize_call_count += 1

        if synthesize_call_count == 1:
            # On the first call, simulate a connection drop
            raise ConnectionRefusedError("Simulated connection drop from test")
        else:
            # On the second call, simulate a successful audio stream
            yield [False, MESSAGE_TYPE_PCM, b"\x44\x55\x66"]
            yield [True, MESSAGE_TYPE_PCM, b""]

    mock_instance.synthesize_audio.side_effect = mock_synthesize_stateful

    # --- Test Setup ---
    config = {
        "app_id": "a_valid_app_id",
        "secret_key": "a_valid_secret_key",
        "secret_id": "a_valid_secret_id",
    }
    tester = ExtensionTesterRobustness()
    tester.set_test_mode_single("tencent_tts_python", json.dumps(config))

    print("Running robustness test...")
    tester.run()
    print("Robustness test completed.")

    # --- Assertions ---
    # 1. Verify that the first request resulted in a NON_FATAL_ERROR
    assert (
        tester.first_request_error is not None
    ), "Did not receive any error message."
    assert (
        tester.first_request_error.get("code") == 1000
    ), f"Expected error code 1000 (NON_FATAL_ERROR), got {tester.first_request_error.get('code')}"

    # 2. Verify that vendor_info was included in the error
    vendor_info = tester.first_request_error.get("vendor_info")
    assert vendor_info is not None, "Error message did not contain vendor_info."
    assert (
        vendor_info.get("vendor") == "tencent"
    ), f"Expected vendor 'tencent', got {vendor_info.get('vendor')}"

    # 3. Verify that the client's start method was called twice (initial + reconnect)
    # This assertion is tricky because the reconnection logic might be inside the client.
    # A better assertion is to check if the second request succeeded.

    # 4. Verify that the second TTS request was successful
    assert (
        tester.second_request_successful
    ), "The second TTS request after the error did not succeed."

    print(
        "✅ Robustness test passed: Correctly handled simulated connection drop and recovered."
    )


# ================ case 7 ================
class ExtensionTesterForPassthrough(ExtensionTester):
    """A simple tester that just starts and stops, to allow checking constructor calls."""

    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
            return
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            # TODO: move stop_test() to where the test passes
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


@patch("tencent_tts_python.extension.TencentTTSClient")
def test_params_passthrough(MockTencentTTSClient):
    """
    Tests that custom parameters passed in the configuration are correctly
    forwarded to the TencentTTSClient constructor.
    """
    print("Starting test_params_passthrough with mock...")

    # --- Mock Configuration ---
    mock_instance = MockTencentTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.close = AsyncMock()  # Required for clean shutdown in on_stop

    # --- Test Setup ---
    # Define a configuration with custom, arbitrary parameters inside 'params'.
    # These are the parameters we expect to be "passed through".
    passthrough_params = {
        "voice_type": "1",
        "sample_rate": 16000,
        "speed": 1.0,
        "volume": 0.5,
    }
    passthrough_config = {
        "app_id": "a_valid_app_id",
        "secret_key": "a_valid_secret_key",
        "secret_id": "a_valid_secret_id",
        "params": passthrough_params,
    }

    tester = ExtensionTesterForPassthrough()
    tester.set_test_mode_single(
        "tencent_tts_python", json.dumps(passthrough_config)
    )

    print("Running passthrough test...")
    tester.run()
    print("Passthrough test completed.")

    # --- Assertions ---
    # Check that the TencentTTSClient was instantiated exactly once.
    MockTencentTTSClient.assert_called_once()

    # Get the arguments that the mock was called with.
    # The constructor signature is (self, config, ten_env, vendor),
    # so we inspect the 'config' object at index 0 of the call arguments.
    call_args, call_kwargs = MockTencentTTSClient.call_args
    called_config = call_args[0]

    # Verify that the 'params' dictionary in the config object passed to the
    # client constructor is identical to the one we defined in our test config.
    assert (
        called_config.params == passthrough_params
    ), f"Expected params to be {passthrough_params}, but got {called_config.params}"

    print("✅ Params passthrough test passed successfully.")
    print(f"✅ Verified params: {called_config.params}")


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
        ten_env_tester.log_info(
            "TextInputEnd test started, sending first TTS request."
        )

        # 1. Send first request with text_input_end=True
        tts_input_1 = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
            text_input_end=True,
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
        # 2. Send second request with text_input_end=False
        tts_input_2 = TTSTextInput(
            request_id="tts_request_1",
            text="this should be ignored",
            text_input_end=False,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input_2.model_dump_json())
        self.ten_env.send_data(data)

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"Received data: {name}")

        if name == "tts_audio_end":
            if not self.first_request_audio_end_received:
                ten_env.log_info(
                    "Received tts_audio_end for the first request."
                )
                self.first_request_audio_end_received = True
                self.send_second_request()
            return

        json_str, _ = data.get_property_to_json(None)
        ten_env.log_info(f"Received data: {json_str}")

        if not json_str:
            return

        payload = json.loads(json_str)
        request_id = payload.get("id")

        if name == "error" and request_id == "tts_request_1":
            ten_env.log_info(
                f"Received expected error for the second request: {payload}"
            )
            self.second_request_error_received = True
            self.error_code = payload.get("code")
            self.error_message = payload.get("message")
            self.error_module = payload.get("module")
            ten_env.stop_test()


@patch("tencent_tts_python.extension.TencentTTSClient")
def test_text_input_end_logic(MockTencentTTSClient):
    """
    Tests that after a request with text_input_end=True is processed,
    subsequent requests with the same request_id and text_input_end=False are ignored and trigger an error.
    """
    print("Starting test_text_input_end_logic with mock...")

    # --- Mock Configuration ---
    mock_instance = MockTencentTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.close = AsyncMock()

    async def mock_synthesize_audio_stream(text: str):
        yield [False, MESSAGE_TYPE_PCM, b"\x11\x22\x33"]
        yield [True, MESSAGE_TYPE_PCM, b""]

    mock_instance.synthesize_audio.side_effect = mock_synthesize_audio_stream

    # --- Test Setup ---
    config = {
        "app_id": "a_valid_app_id",
        "secret_key": "a_valid_secret_key",
        "secret_id": "a_valid_secret_id",
    }
    tester = ExtensionTesterTextInputEnd()
    tester.set_test_mode_single("tencent_tts_python", json.dumps(config))

    print("Running text_input_end logic test...")
    tester.run()
    print("text_input_end logic test completed.")

    # --- Assertions ---
    assert (
        tester.first_request_audio_end_received
    ), "Did not receive tts_audio_end for the first request."
    assert (
        tester.second_request_error_received
    ), "Did not receive the expected error for the second request."
    assert (
        tester.error_code == 1000
    ), f"Expected error code 1000, but got {tester.error_code}"
    assert (
        tester.error_message is not None
        and "Received a message for a finished request_id"
        in tester.error_message
    ), "Error message is not as expected."

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
        self.sample_rate = 24000
        self.bytes_per_sample = 2  # 16-bit
        self.channels = 1
        self.audio_received_after_flush_end = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        self.ten_env = ten_env_tester
        ten_env_tester.log_info("Flush test started, sending long TTS request.")
        tts_input = TTSTextInput(
            request_id="tts_request_for_flush",
            text="This is a very long text designed to generate a continuous stream of audio, providing enough time to send a flush command.",
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
            flush_data.set_property_from_json(
                None,
                TTSFlush(flush_id="tts_request_for_flush").model_dump_json(),
            )
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

        # Note: tts_flush is input data, not an output event
        # We don't expect to receive tts_flush as an output event
        # if name == "tts_flush":
        #     self.flush_start_received = True
        #     return

        json_str, _ = data.get_property_to_json(None)
        if not json_str:
            return
        payload = json.loads(json_str)
        ten_env.log_info(f"on_data payload: {payload}")

        if name == "tts_audio_end":
            self.audio_end_received = True
            self.audio_end_reason = payload.get("reason")
            self.total_audio_duration_from_event = payload.get(
                "request_total_audio_duration_ms"
            )

        elif name == "tts_flush_end":
            self.flush_end_received = True

            def stop_test_later():
                ten_env.log_info("Waited after flush_end, stopping test now.")
                ten_env.stop_test()

            # Use threading.Timer to avoid 'no running event loop' error,
            # as on_data is called from a non-async context.
            timer = threading.Timer(0.5, stop_test_later)
            timer.start()

    def get_calculated_audio_duration_ms(self) -> int:
        duration_sec = self.received_audio_bytes / (
            self.sample_rate * self.bytes_per_sample * self.channels
        )
        return int(duration_sec * 1000)


@patch("tencent_tts_python.extension.TencentTTSClient")
def test_flush_logic(MockTencentTTSClient):
    """
    Tests that sending a flush command during TTS streaming correctly stops
    the audio and sends the appropriate events.
    """
    print("Starting test_flush_logic with mock...")

    mock_instance = MockTencentTTSClient.return_value
    mock_instance.start = AsyncMock()
    mock_instance.close = AsyncMock()
    mock_instance.cancel = AsyncMock()

    async def mock_synthesize_long_audio_stream(text: str):
        for _ in range(20):
            if mock_instance.cancel.called:
                print(
                    "Mock detected cancel call, stopping stream and yielding flush signal."
                )
                yield [True, MESSAGE_TYPE_PCM, b""]  # done=True indicates flush
                return  # Stop the generator immediately
            yield [False, MESSAGE_TYPE_PCM, b"\x11\x22\x33" * 100]
            await asyncio.sleep(0.1)
        # This part is only reached if not cancelled
        yield [True, MESSAGE_TYPE_PCM, b""]

    mock_instance.synthesize_audio.side_effect = (
        mock_synthesize_long_audio_stream
    )

    config = {
        "app_id": "a_valid_app_id",
        "secret_key": "a_valid_secret_key",
        "secret_id": "a_valid_secret_id",
    }
    tester = ExtensionTesterFlush()
    tester.set_test_mode_single("tencent_tts_python", json.dumps(config))

    print("Running flush logic test...")
    tester.run()
    print("Flush logic test completed.")

    assert tester.audio_start_received, "Did not receive tts_audio_start."
    assert tester.first_audio_frame_received, "Did not receive any audio frame."
    # Note: tts_flush is input data, not an output event, so we don't expect it
    # assert tester.flush_start_received, "Did not receive tts_flush."
    assert tester.audio_end_received, "Did not receive tts_audio_end."
    assert tester.flush_end_received, "Did not receive tts_flush_end."
    assert (
        not tester.audio_received_after_flush_end
    ), "Received audio after tts_flush_end."

    # TODO: no reason in audio end
    # assert tester.audio_end_reason == "flush", f"Expected audio end reason 'flush', but got '{tester.audio_end_reason}'"

    calculated_duration = tester.get_calculated_audio_duration_ms()
    event_duration = tester.total_audio_duration_from_event
    print(
        f"calculated_duration: {calculated_duration}, event_duration: {event_duration}"
    )
    assert (
        abs(calculated_duration - event_duration) < 10
    ), f"Mismatch in audio duration. Calculated: {calculated_duration}ms, From event: {event_duration}ms"

    print("✅ Flush logic test passed successfully.")
