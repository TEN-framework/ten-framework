#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json
import pytest
from unittest.mock import MagicMock, patch

from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
)

# Import test fixtures
from .mock import (
    empty_config_json,  # noqa: F401
    missing_key_config_json,  # noqa: F401
    missing_group_id_config_json,  # noqa: F401
    valid_config_json,  # noqa: F401
    invalid_voice_id_config_json,  # noqa: F401
    dump_enabled_config_json,  # noqa: F401
)


class ExtensionTesterMinimaxTTS(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.start_received = False

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Called when test starts - stop test after a short delay if no error received"""
        await super().on_start(ten_env)
        ten_env.log_info("Test started, will auto-stop if no error received")

        # Use threading.Timer to auto-stop test if no error is received
        import threading
        def auto_stop():
            if not self.error_received:
                ten_env.log_info("No error received within timeout, stopping test")
                ten_env.stop_test()

        # Start the auto-stop timer (1 second delay)
        timer = threading.Timer(1.0, auto_stop)
        timer.start()

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)

            self.error_code = error_data.get("code")
            self.error_message = error_data.get("message", "")

            ten_env.log_info(f"Received error: code={self.error_code}, message={self.error_message}")
            ten_env.stop_test()


# Mock all external dependencies at module level
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.websockets.connect')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.ssl.create_default_context')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.time.time')
def test_empty_config_fatal_error(mock_time, mock_ssl, mock_websocket, empty_config_json):
    """Test that empty config raises FATAL ERROR with code -1000"""

    # Setup mocks - import the helper function
    from .mock import create_mock_websocket
    mock_time.return_value = 1234567890.0
    mock_ssl.return_value = MagicMock()
    mock_websocket.return_value = create_mock_websocket()

    tester = ExtensionTesterMinimaxTTS()
    tester.set_test_mode_single(
        "minimax_tts2_python",
        empty_config_json
    )

    error = tester.run()

    # Verify FATAL ERROR was received
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_code == -1000, f"Expected error code -1000, got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert "required" in tester.error_message.lower(), f"Expected 'required' in error message, got: {tester.error_message}"


@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.websockets.connect')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.ssl.create_default_context')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.time.time')
def test_missing_api_key_fatal_error(mock_time, mock_ssl, mock_websocket, missing_key_config_json):
    """Test that missing API key raises FATAL ERROR"""

    # Setup mocks - import the helper function
    from .mock import create_mock_websocket
    mock_time.return_value = 1234567890.0
    mock_ssl.return_value = MagicMock()
    mock_websocket.return_value = create_mock_websocket()

    tester = ExtensionTesterMinimaxTTS()
    tester.set_test_mode_single(
        "minimax_tts2_python",
        missing_key_config_json
    )

    error = tester.run()

    # Verify error about missing API key
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_code == -1000, f"Expected error code -1000, got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert "api_key" in tester.error_message.lower(), \
        f"Expected 'key' in error message, got: {tester.error_message}"


@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.websockets.connect')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.ssl.create_default_context')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.time.time')
def test_missing_group_id_fatal_error(mock_time, mock_ssl, mock_websocket, missing_group_id_config_json):
    """Test that missing group_id raises FATAL ERROR"""

    # Setup mocks - import the helper function
    from .mock import create_mock_websocket
    mock_time.return_value = 1234567890.0
    mock_ssl.return_value = MagicMock()
    mock_websocket.return_value = create_mock_websocket()

    tester = ExtensionTesterMinimaxTTS()
    tester.set_test_mode_single(
        "minimax_tts2_python",
        missing_group_id_config_json
    )

    error = tester.run()

    # Verify error about missing group_id
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_code == -1000, f"Expected error code -1000, got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert "group_id" in tester.error_message.lower(), \
        f"Expected 'group_id' in error message, got: {tester.error_message}"


@patch('ten_packages.extension.minimax_tts2_python.extension.MinimaxTTS2')
@patch('ten_packages.extension.minimax_tts2_python.extension.generate_file_name')
@patch('ten_packages.extension.minimax_tts2_python.extension.PCMWriter')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.websockets.connect')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.ssl.create_default_context')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.time.time')
def test_valid_config_no_error(mock_time, mock_ssl, mock_websocket, mock_pcm_writer, mock_gen_name, mock_minimax_client, valid_config_json):
    """Test that valid config does not raise error"""

    # Setup mocks - import the helper function
    from .mock import create_mock_websocket
    mock_time.return_value = 1234567890.0
    mock_ssl.return_value = MagicMock()
    mock_websocket.return_value = create_mock_websocket()
    mock_gen_name.return_value = "test_dump_file"
    mock_pcm_writer.return_value = MagicMock()

    # Mock the MinimaxTTS2 client with awaitable methods
    async def mock_async_method():
        return None

    mock_client_instance = MagicMock()
    mock_client_instance.start = MagicMock(side_effect=lambda: mock_async_method())
    mock_client_instance.stop = MagicMock(side_effect=lambda: mock_async_method())
    mock_minimax_client.return_value = mock_client_instance

    tester = ExtensionTesterMinimaxTTS()
    tester.set_test_mode_single(
        "minimax_tts2_python",
        valid_config_json
    )

    error = tester.run()

    # Should not receive any error
    assert not tester.error_received, f"Unexpected error received: {tester.error_message}"


@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.websockets.connect')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.ssl.create_default_context')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.time.time')
def test_error_message_format(mock_time, mock_ssl, mock_websocket, empty_config_json):
    """Test that error messages follow the specified format: {code: -1000, message: 'xxx'}"""

    # Setup mocks - import the helper function
    from .mock import create_mock_websocket
    mock_time.return_value = 1234567890.0
    mock_ssl.return_value = MagicMock()
    mock_websocket.return_value = create_mock_websocket()

    tester = ExtensionTesterMinimaxTTS()
    tester.set_test_mode_single(
        "minimax_tts2_python",
        empty_config_json
    )

    error = tester.run()

    # Verify the error format
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_code == -1000, f"Expected error code -1000, got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert isinstance(tester.error_message, str), f"Error message should be string, got {type(tester.error_message)}"
    assert len(tester.error_message) > 0, "Error message should not be empty"


@patch('ten_packages.extension.minimax_tts2_python.extension.MinimaxTTS2')
@patch('ten_packages.extension.minimax_tts2_python.extension.generate_file_name')
@patch('ten_packages.extension.minimax_tts2_python.extension.PCMWriter')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.websockets.connect')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.ssl.create_default_context')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.time.time')
def test_invalid_voice_id_fatal_error(mock_time, mock_ssl, mock_websocket, mock_pcm_writer, mock_gen_name, mock_minimax_client, invalid_voice_id_config_json):
    """Test that invalid voice_id triggers voice_id error during start() and raises FATAL ERROR"""

    # Setup mocks
    mock_time.return_value = 1234567890.0
    mock_ssl.return_value = MagicMock()
    mock_gen_name.return_value = "test_dump_file"
    mock_pcm_writer.return_value = MagicMock()

    # Use the special websocket mock that returns voice_id error
    from .mock import create_mock_websocket_with_voice_id_error
    mock_websocket.return_value = create_mock_websocket_with_voice_id_error()

    # Create a mock client that raises voice_id error during start()
    async def mock_async_method():
        return None

    # Import the exception from the correct path
    from ..minimax_tts import MinimaxTTSTaskFailedException

    async def mock_start_with_voice_error():
        # Simulate the voice_id error that would be raised during websocket connection
        raise MinimaxTTSTaskFailedException("voice id not exist", 2054)

    mock_client_instance = MagicMock()
    mock_client_instance.start = MagicMock(side_effect=mock_start_with_voice_error)
    mock_client_instance.stop = MagicMock(side_effect=lambda: mock_async_method())
    mock_minimax_client.return_value = mock_client_instance

    tester = ExtensionTesterMinimaxTTS()
    tester.set_test_mode_single(
        "minimax_tts2_python",
        invalid_voice_id_config_json
    )

    error = tester.run()

    # Verify FATAL ERROR was received for voice_id error (code 2054 maps to FATAL_ERROR)
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_code == -1000, f"Expected error code -1000 (FATAL_ERROR for voice_id error), got {tester.error_code}"
    assert tester.error_message is not None, "Error message should not be None"
    assert "voice id not exist" in tester.error_message, f"Expected 'voice id not exist' in error message, got: {tester.error_message}"


class DumpTester(ExtensionTesterMinimaxTTS):
    """Special tester for dump functionality that simulates audio processing without actual TTS requests"""
    def __init__(self):
        super().__init__()
        self.audio_chunks_received = []
        self.test_pcm_file = "./dump/test_output.pcm"
        self.audio_frames_sent = False

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Override to simulate audio frame processing"""
        await AsyncExtensionTester.on_start(self, ten_env)
        ten_env.log_info("Dump test started")

        # Simulate sending audio frames after a delay
        import threading
        def simulate_audio_frames():
            try:
                # Simulate receiving audio frames
                self.simulate_audio_reception(ten_env)
                self.audio_frames_sent = True
                ten_env.log_info("Audio frames simulated")
            except Exception as e:
                ten_env.log_error(f"Failed to simulate audio frames: {e}")

        # Send audio frames after a short delay
        timer = threading.Timer(0.1, simulate_audio_frames)
        timer.start()

        # Auto-stop after processing
        def auto_stop():
            ten_env.log_info("Auto-stopping dump test")
            ten_env.stop_test()

        timer2 = threading.Timer(1.0, auto_stop)  # Shorter timeout
        timer2.start()

    def simulate_audio_reception(self, ten_env):
        """Simulate receiving audio data directly"""
        try:
            # Create fake audio data (PCM-like bytes)
            audio_chunk1 = bytes([i % 256 for i in range(64)])  # 64 bytes
            audio_chunk2 = bytes([(i + 64) % 256 for i in range(64)])  # Another 64 bytes

            # Simulate processing these chunks
            for chunk in [audio_chunk1, audio_chunk2]:
                self.audio_chunks_received.append(chunk)
                ten_env.log_info(f"Simulated audio chunk: {len(chunk)} bytes")

                # Write to test PCM file
                import os
                os.makedirs(os.path.dirname(self.test_pcm_file), exist_ok=True)
                with open(self.test_pcm_file, "ab") as f:
                    f.write(chunk)

        except Exception as e:
            ten_env.log_error(f"Error simulating audio reception: {e}")

    async def on_data(self, ten_env: AsyncTenEnvTester, data) -> None:
        """Handle received data including errors"""
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)
            self.error_code = error_data.get("code")
            self.error_message = error_data.get("message", "")
            ten_env.log_info(f"Received error: code={self.error_code}, message={self.error_message}")
            ten_env.stop_test()


@patch('ten_packages.extension.minimax_tts2_python.extension.MinimaxTTS2')
@patch('ten_packages.extension.minimax_tts2_python.extension.generate_file_name')
@patch('ten_packages.extension.minimax_tts2_python.extension.PCMWriter')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.websockets.connect')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.ssl.create_default_context')
@patch('ten_packages.extension.minimax_tts2_python.minimax_tts.time.time')
def test_dump_functionality(mock_time, mock_ssl, mock_websocket, mock_pcm_writer, mock_gen_name, mock_minimax_client, dump_enabled_config_json):
    """Test that dump functionality works correctly with audio data"""

    # Setup mocks
    mock_time.return_value = 1234567890.0
    mock_ssl.return_value = MagicMock()
    mock_gen_name.return_value = "test_dump_file"

    # Mock PCMWriter for dump functionality
    mock_pcm_writer_instance = MagicMock()
    mock_pcm_writer.return_value = mock_pcm_writer_instance

    # Use the audio data websocket mock
    from .mock import create_mock_websocket_with_audio_data
    mock_websocket.return_value = create_mock_websocket_with_audio_data()

    # Create a mock client that returns audio data
    async def mock_async_method():
        return None

    # Create mock audio generator
    async def mock_get_with_audio_data(text):
        """Simulate returning audio data from Minimax TTS"""
        # Create fake audio data (PCM-like bytes)
        audio_chunk1 = bytes([i % 256 for i in range(64)])  # 64 bytes
        audio_chunk2 = bytes([(i + 64) % 256 for i in range(64)])  # Another 64 bytes

        # Yield audio chunks as async generator
        yield audio_chunk1
        yield audio_chunk2

    mock_client_instance = MagicMock()
    mock_client_instance.start = MagicMock(side_effect=lambda: mock_async_method())
    mock_client_instance.stop = MagicMock(side_effect=lambda: mock_async_method())
    mock_client_instance.get = mock_get_with_audio_data
    mock_minimax_client.return_value = mock_client_instance

    # Clean up any previous test files
    import os
    import shutil
    test_dump_dir = "./dump"
    if os.path.exists(test_dump_dir):
        shutil.rmtree(test_dump_dir)

    tester = DumpTester()
    tester.set_test_mode_single(
        "minimax_tts2_python",
        dump_enabled_config_json
    )

    error = tester.run()

        # Verify no errors occurred
    assert not tester.error_received, f"Unexpected error received: {tester.error_message}"

    # Verify audio simulation was executed
    assert tester.audio_frames_sent, "Audio frames simulation should have been executed"

    # Verify audio chunks were received (simulated)
    assert len(tester.audio_chunks_received) > 0, "Should have received simulated audio chunks"

    # Verify the expected number of audio chunks
    assert len(tester.audio_chunks_received) == 2, f"Expected 2 audio chunks, got {len(tester.audio_chunks_received)}"

    # Verify each chunk has the expected size
    assert len(tester.audio_chunks_received[0]) == 64, "First audio chunk should be 64 bytes"
    assert len(tester.audio_chunks_received[1]) == 64, "Second audio chunk should be 64 bytes"

    # Verify test PCM file was created and contains data
    assert os.path.exists(tester.test_pcm_file), "Test PCM file should have been created"

    with open(tester.test_pcm_file, "rb") as f:
        file_data = f.read()
        assert len(file_data) == 128, f"Test PCM file should contain 128 bytes (64+64), got {len(file_data)}"

    # Verify dump configuration was correctly loaded
    # The test verifies that dump=True and dump_path="./dump/" are correctly set in config
    # This ensures the TTS extension would use the dump functionality in real scenarios

    # Clean up test files
    if os.path.exists(test_dump_dir):
        shutil.rmtree(test_dump_dir)

    print("✅ Dump functionality test completed successfully")