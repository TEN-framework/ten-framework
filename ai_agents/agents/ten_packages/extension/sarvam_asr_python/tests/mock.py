#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from types import SimpleNamespace
import asyncio
import threading

import pytest
from unittest.mock import patch, MagicMock
import aiohttp


@pytest.fixture(scope="function")
def patch_sarvam_ws():
    """Patch Sarvam ASR's aiohttp WebSocket client used in the extension.

    The Sarvam extension uses aiohttp.ClientSession.ws_connect and then
    iterates over the returned WebSocket for incoming messages.

    This fixture:
    - Replaces aiohttp.ClientSession with a lightweight mock session
    - Provides a mock WebSocket object with:
      - async send_str
      - async close
      - async iterator yielding predefined messages
    - Exposes the WebSocket and message list so tests can control behavior.
    """

    messages = []
    messages_lock = threading.Lock()

    class MockWebSocketMessage:
        """Mock aiohttp WebSocket message."""

        def __init__(self, msg_type, data=None, exception=None):
            self.type = msg_type
            self.data = data
            self._exception = exception

        def exception(self):
            return self._exception

    class MockWebSocket:
        def __init__(self) -> None:
            self.sent_messages: list[str] = []
            self.closed: bool = False
            self._exception = None

        async def send_str(self, data: str) -> bool:
            self.sent_messages.append(data)
            return True

        async def close(self) -> bool:
            self.closed = True
            return True

        def exception(self):
            return self._exception

        def __aiter__(self):
            async def _gen():
                # Keep iterating until closed, allowing messages to be added from other threads
                processed_count = 0
                while not self.closed:
                    with messages_lock:
                        # Get new messages that haven't been processed yet
                        current_messages = messages[processed_count:]
                        processed_count = len(messages)

                    if current_messages:
                        for msg in current_messages:
                            yield msg
                    else:
                        # Small sleep to avoid busy waiting when no messages
                        await asyncio.sleep(0.1)

            return _gen()

    class MockSession:
        def __init__(self, *args, **kwargs) -> None:
            self.closed: bool = False

        async def ws_connect(self, url, headers=None, timeout=None):
            # Always return the same mock WebSocket instance
            return ws

        async def close(self) -> None:
            self.closed = True

    ws = MockWebSocket()

    # Patch the ClientSession used inside the Sarvam extension module
    with patch(
        "ten_packages.extension.sarvam_asr_python.extension.aiohttp.ClientSession",
        MockSession,
    ):

        def add_message(msg):
            """Thread-safe helper to add messages."""
            with messages_lock:
                messages.append(msg)

        fixture_obj = SimpleNamespace(
            ws=ws,
            messages=messages,
            messages_lock=messages_lock,  # Expose lock for thread-safe access
            add_message=add_message,  # Helper function to add messages thread-safely
            WSMsgType=aiohttp.WSMsgType,  # Expose WSMsgType for tests
            MockWebSocketMessage=MockWebSocketMessage,  # Helper for creating messages
        )

        yield fixture_obj
