#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from types import SimpleNamespace
import pytest
from unittest.mock import MagicMock, patch

@pytest.fixture(scope="function")
def patch_speechmatics_ws():
    patch_target = "ten_packages.extension.speechmatics_asr_python.asr_client.speechmatics.client"

    with patch(patch_target) as MockClient:

        recognizer_instance = MagicMock()
        event_handlers = {}
        patch_speechmatics_ws.event_handlers = event_handlers

        def connect_mock(handler):
            event_handlers["recognized"] = handler

        recognizer_instance.recognized.connect.side_effect = connect_mock

        MockClient.return_value = recognizer_instance


        fixture_obj = SimpleNamespace(
            recognizer_instance=recognizer_instance,
            event_handlers=event_handlers
        )

        yield fixture_obj