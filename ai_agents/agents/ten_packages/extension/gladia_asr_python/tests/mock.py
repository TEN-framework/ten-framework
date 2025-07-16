#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json
import pytest
from unittest.mock import AsyncMock, MagicMock, patch


@pytest.fixture(scope="function")
def patch_gladia_ws():
    with patch(
        "ten_packages.extension.gladia_asr_python.extension.requests.post"
    ) as mock_post, patch(
        "ten_packages.extension.gladia_asr_python.extension.websockets.connect"
    ) as mock_connect:

        # Mock POST /v2/live
        mock_post.return_value = MagicMock(
            ok=True,
            json=lambda: {"url": "wss://mock.gladia.io/stream"},
            status_code=200,
        )

        # Mock websocket
        mock_ws = AsyncMock()
        mock_ws.send.return_value = None
        mock_ws.close.return_value = None

        async def mock_connect_async(*args, **kwargs):
            return mock_ws

        mock_connect.side_effect = mock_connect_async

        yield mock_ws
