#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import pytest
from unittest.mock import AsyncMock, patch


@pytest.fixture(scope="function")
def patch_deepgram_ws():
    """
    Automatically patch Recognition globally before any test runs.
    """
    patch_target = "ten_packages.extension.aliyun_asr_bigmodel_python.extension.Recognition"

    with patch(patch_target) as MockWSClient:
        print(f"✅ Patching {patch_target} before test session.")

        mock_ws = AsyncMock()
        mock_ws.start.return_value = True
        mock_ws.send.return_value = None
        mock_ws.finish.return_value = None

        mock_ws._handlers = {}

        def mock_on(event_name, callback):
            event_str = (
                str(event_name)
                if not isinstance(event_name, str)
                else event_name
            )
            mock_ws._handlers[event_str] = callback

        mock_ws.on = mock_on

        MockWSClient.return_value = mock_ws
        yield mock_ws
        # patch stays active through the whole session
