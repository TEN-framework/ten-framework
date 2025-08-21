#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock, patch

import pytest


@pytest.fixture(scope="function")
def patch_soniox_ws():
    patch_target = "ten_packages.extension.soniox_asr_python.extension.SonioxWebsocketClient"

    with patch(patch_target) as MockWebsocketClient:
        websocket_client_instance = MagicMock()

        # Store callbacks registered via on() method
        websocket_client_instance._callbacks = {}

        def mock_on(event, callback):
            # Handle both enum values and string values
            event_key = event.value if hasattr(event, "value") else event
            websocket_client_instance._callbacks[event_key] = callback

        websocket_client_instance.on = mock_on

        # Mock async methods with AsyncMock
        websocket_client_instance.connect = AsyncMock()
        websocket_client_instance.send_audio = AsyncMock()
        websocket_client_instance.finalize = AsyncMock()
        websocket_client_instance.stop = AsyncMock()

        # Add helper methods that can be called by tests to trigger events
        async def trigger_open():
            if "open" in websocket_client_instance._callbacks:
                await websocket_client_instance._callbacks["open"]()

        async def trigger_close():
            if "close" in websocket_client_instance._callbacks:
                await websocket_client_instance._callbacks["close"]()

        async def trigger_transcript(
            tokens, final_audio_proc_ms, total_audio_proc_ms
        ):
            if "transcript" in websocket_client_instance._callbacks:
                await websocket_client_instance._callbacks["transcript"](
                    tokens, final_audio_proc_ms, total_audio_proc_ms
                )

        async def trigger_error(error_code, error_message):
            if "error" in websocket_client_instance._callbacks:
                await websocket_client_instance._callbacks["error"](
                    error_code, error_message
                )

        async def trigger_exception(exception):
            if "exception" in websocket_client_instance._callbacks:
                await websocket_client_instance._callbacks["exception"](
                    exception
                )

        async def trigger_finished(final_audio_proc_ms, total_audio_proc_ms):
            if "finished" in websocket_client_instance._callbacks:
                await websocket_client_instance._callbacks["finished"](
                    final_audio_proc_ms, total_audio_proc_ms
                )

        websocket_client_instance.trigger_open = trigger_open
        websocket_client_instance.trigger_close = trigger_close
        websocket_client_instance.trigger_transcript = trigger_transcript
        websocket_client_instance.trigger_error = trigger_error
        websocket_client_instance.trigger_exception = trigger_exception
        websocket_client_instance.trigger_finished = trigger_finished

        MockWebsocketClient.return_value = websocket_client_instance

        fixture_obj = SimpleNamespace(
            websocket_client=websocket_client_instance,
        )

        yield fixture_obj
