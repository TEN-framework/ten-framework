#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from types import SimpleNamespace
import pytest
from unittest.mock import MagicMock, patch


@pytest.fixture(scope="function")
def patch_ezai_ws():
    patch_target = (
        "ten_packages.extension.ezai_asr.extension.CustomWebSocketClient"
    )

    with patch(patch_target) as MockClient:
        client_instance = MagicMock()
        event_handlers = {}
        patch_ezai_ws.event_handlers = event_handlers

        # Define mock event registration function
        def on_mock(event_type, callback):
            print(f"register_event_handler: {event_type} -> {callback}")
            event_handlers[event_type] = callback
            return True

        # Define mock start function
        async def start_mock():
            print(f"start_mock called")
            return True

        # Define mock send function
        async def send_mock(data):
            print(f"send_mock data length: {len(data)}")
            return True

        # Define mock finish/finalize functions
        async def finish_mock():
            print("finish_mock called")
            return True

        async def finalize_mock():
            print("finalize_mock called")
            return True

        # Assign mock methods to client instance
        client_instance.on.side_effect = on_mock
        client_instance.start.side_effect = start_mock
        client_instance.send.side_effect = send_mock
        client_instance.finish.side_effect = finish_mock
        client_instance.finalize.side_effect = finalize_mock

        MockClient.return_value = client_instance

        fixture_obj = SimpleNamespace(
            client_instance=client_instance,
            event_handlers=event_handlers,
        )

        yield fixture_obj
