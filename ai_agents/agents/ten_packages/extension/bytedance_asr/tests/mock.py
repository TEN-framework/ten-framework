#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json
import sys
from types import SimpleNamespace
import types
import pytest
from unittest.mock import AsyncMock, MagicMock, patch


@pytest.fixture(scope="function")
def patch_bytedance_ws():
    # Patch the Bytedance AsrWsClient constructor
    with patch(
        "ten_packages.extension.bytedance_asr.extension.AsrWsClient"
    ) as MockClient:
        mock_client = MagicMock()
        callback_store = {}

        # Capture the handle_received_message callback
        def mock_constructor(*args, **kwargs):
            callback_store["on_message"] = kwargs.get("handle_received_message")
            return mock_client

        MockClient.side_effect = mock_constructor

        # Simulate start() behavior
        async def mock_start():
            print("[mock] Bytedance client start() called")

            async def delayed_message():
                await asyncio.sleep(1)
                if callback_store.get("on_message"):
                    await callback_store["on_message"](
                        [
                            {
                                "text": "hello world",
                                "utterances": [
                                    {
                                        "definite": True,
                                        "start_time": 0,
                                        "end_time": 1705,
                                    }
                                ],
                            }
                        ]
                    )

            asyncio.get_event_loop().create_task(delayed_message())

        mock_client.start.side_effect = mock_start
        mock_client.send = AsyncMock()
        mock_client.finish = AsyncMock()

        yield mock_client
