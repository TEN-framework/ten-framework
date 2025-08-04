#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from types import SimpleNamespace
from unittest.mock import MagicMock, patch

import pytest


@pytest.fixture(scope="function")
def patch_soniox_ws():
    patch_target = (
        "ten_packages.extension.soniox_asr_python.extension.SonioxWebsocketClient"
    )

    with patch(patch_target) as MockWebsocketClient:
        websocket_client_instance = MagicMock()
        
        # Mock the event handling methods
        websocket_client_instance.on = MagicMock()
        websocket_client_instance.connect = MagicMock()
        websocket_client_instance.send_audio = MagicMock()
        websocket_client_instance.finalize = MagicMock()
        websocket_client_instance.stop = MagicMock()
        
        # Add handle methods that can be called by tests
        websocket_client_instance._handle_open = MagicMock()
        websocket_client_instance._handle_close = MagicMock()
        websocket_client_instance._handle_transcript = MagicMock()
        websocket_client_instance._handle_error = MagicMock()
        websocket_client_instance._handle_exception = MagicMock()
        websocket_client_instance._handle_finished = MagicMock()

        MockWebsocketClient.return_value = websocket_client_instance

        fixture_obj = SimpleNamespace(
            websocket_client=websocket_client_instance,
        )

        yield fixture_obj