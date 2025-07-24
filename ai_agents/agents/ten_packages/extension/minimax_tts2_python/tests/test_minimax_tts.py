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