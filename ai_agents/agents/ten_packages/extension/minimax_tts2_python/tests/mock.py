#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json
from types import SimpleNamespace
from unittest.mock import MagicMock, patch
import pytest


class MockWebSocketResponse:
    def __init__(self):
        self.headers = {
            "Trace-Id": "test-trace-id",
            "alb_request_id": "test-alb-id"
        }


class MockWebSocket:
    def __init__(self):
        self.response = MockWebSocketResponse()
        self.recv_counter = 0
        self.recv_responses = [
            json.dumps({
                "event": "connected_success",
                "session_id": "test-session-id"
            }),
            json.dumps({
                "event": "task_started"
            }),
            json.dumps({
                "event": "audio_chunk",
                "data": {
                    "audio": "48656c6c6f"  # "Hello" in hex
                },
                "is_final": True
            })
        ]

    async def send(self, message):
        """Mock send method"""
        pass

    async def recv(self):
        """Mock recv method that returns predefined responses"""
        if self.recv_counter < len(self.recv_responses):
            response = self.recv_responses[self.recv_counter]
            self.recv_counter += 1
            return response
        return json.dumps({"event": "task_finished"})

    async def close(self):
        """Mock close method"""
        pass


def create_mock_websocket():
    """Create a properly configured websocket mock for tests"""
    mock_ws = MagicMock()

    # Mock response object with headers
    mock_response = MagicMock()
    mock_response.headers = {
        "Trace-Id": "test-trace-id",
        "alb_request_id": "test-alb-id"
    }
    mock_ws.response = mock_response

    # Mock send and recv methods - use MagicMock instead of AsyncMock
    mock_ws.send = MagicMock()
    mock_ws.recv = MagicMock()
    mock_ws.close = MagicMock()

    # Setup recv to return expected websocket responses
    mock_ws.recv.side_effect = [
        # Initial connection response
        json.dumps({
            "event": "connected_success",
            "session_id": "test-session-id"
        }),
        # Task start response
        json.dumps({
            "event": "task_started"
        }),
        # Audio data response (optional for config tests)
        json.dumps({
            "event": "audio_chunk",
            "data": {
                "audio": "48656c6c6f"  # "Hello" in hex
            },
            "is_final": True
        })
    ]

    return mock_ws


@pytest.fixture(scope="function", autouse=True)
def patch_all_external_dependencies():
    """Mock all external dependencies automatically for all tests"""

    # Mock websockets.connect
    with patch("ten_packages.extension.minimax_tts2_python.minimax_tts.websockets.connect") as mock_connect:
        mock_ws = create_mock_websocket()
        mock_connect.return_value = mock_ws

        # Mock ssl module
        with patch("ten_packages.extension.minimax_tts2_python.minimax_tts.ssl") as mock_ssl:
            mock_ssl_context = MagicMock()
            mock_ssl.create_default_context.return_value = mock_ssl_context

            # Mock time module calls
            with patch("ten_packages.extension.minimax_tts2_python.minimax_tts.time") as mock_time:
                mock_time.time.return_value = 1234567890.0

                # Mock PCMWriter and generate_file_name
                with patch("ten_packages.extension.minimax_tts2_python.extension.PCMWriter") as mock_pcm_writer, \
                     patch("ten_packages.extension.minimax_tts2_python.extension.generate_file_name") as mock_gen_name:

                    mock_gen_name.return_value = "test_dump_file"
                    mock_pcm_writer_instance = MagicMock()
                    mock_pcm_writer.return_value = mock_pcm_writer_instance

                                                            # Mock MinimaxTTS2 class completely
                    with patch("ten_packages.extension.minimax_tts2_python.extension.MinimaxTTS2") as mock_minimax_class:
                        mock_client_instance = MagicMock()

                                                                                                # Create async mock methods using coroutines
                        async def mock_async_method():
                            return None

                        mock_client_instance.start = MagicMock(side_effect=lambda: mock_async_method())
                        mock_client_instance.stop = MagicMock(side_effect=lambda: mock_async_method())
                        mock_minimax_class.return_value = mock_client_instance

                        fixture_obj = SimpleNamespace(
                            mock_websocket=mock_ws,
                            mock_connect=mock_connect,
                            mock_ssl=mock_ssl,
                            mock_time=mock_time,
                            mock_pcm_writer=mock_pcm_writer,
                            mock_gen_name=mock_gen_name,
                            mock_minimax_class=mock_minimax_class,
                            mock_client_instance=mock_client_instance
                        )

                        yield fixture_obj


@pytest.fixture
def patch_minimax_websocket():
    """Patch websockets.connect to return MockWebSocket"""
    with patch("ten_packages.extension.minimax_tts2_python.minimax_tts.websockets.connect") as mock_connect:
        mock_ws = MockWebSocket()
        mock_connect.return_value = mock_ws
        yield mock_connect


@pytest.fixture
def mock_ten_env():
    """Mock AsyncTenEnvTester for testing"""
    mock_env = MagicMock()
    mock_env.log_info = MagicMock()
    mock_env.log_error = MagicMock()
    mock_env.log_warn = MagicMock()
    mock_env.log_debug = MagicMock()
    mock_env.get_property_to_json = MagicMock()

    return mock_env


# Configuration fixtures for different test scenarios
@pytest.fixture
def empty_config_json():
    """Empty configuration that should trigger FATAL ERROR"""
    return "{}"


@pytest.fixture
def missing_key_config_json():
    """Configuration missing API key that should trigger FATAL ERROR"""
    return json.dumps({
        "group_id": "1837474834917900459",
        "voice_id": "male-qn-qingse"
    })


@pytest.fixture
def missing_group_id_config_json():
    """Configuration missing group_id that should trigger FATAL ERROR"""
    return json.dumps({
        "api_key": "test-api-key-value",
        "voice_id": "male-qn-qingse"
    })


@pytest.fixture
def valid_config_json():
    """Valid configuration that should not trigger error"""
    return json.dumps({
        "api_key": "test-api-key-value",
        "group_id": "1837474834917900459",
        "voice_id": "male-qn-qingse",
        "url": "wss://api.minimax.chat/v1/t2a_v2/stream",
        "sample_rate": 32000,
        "dump_path": "/tmp/ten_logs"
    })