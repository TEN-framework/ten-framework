#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
from types import SimpleNamespace
import pytest
from unittest.mock import AsyncMock, MagicMock, patch


@pytest.fixture(scope="function")
def patch_transcribe():
    with patch(
        "ten_packages.extension.transcribe_asr_python.extension.amazon_transcribe.client.TranscribeStreamingClient"
    ) as MockClient, patch(
        "ten_packages.extension.transcribe_asr_python.extension.TranscribeEventHandler"
    ) as MockHandler:

        # Create mock client as AsyncMock to support await
        mock_client_instance = AsyncMock()
        MockClient.return_value = mock_client_instance

        # Mock stream and handler
        event_stream_mock = AsyncMock()
        output_stream_mock = MagicMock()
        handler_instance = AsyncMock()

        # Setup input stream with async methods
        fake_input_stream = MagicMock()
        fake_input_stream.send_audio_event = AsyncMock()
        fake_input_stream.end_stream = AsyncMock()

        event_stream_mock.output_stream = output_stream_mock
        event_stream_mock.input_stream = fake_input_stream

        # Prepare handler and its callback
        handler_instance = AsyncMock()
        handler_instance.handle_events = AsyncMock()
        handler_instance.on_transcript_event_cb = AsyncMock()

        async def handle_transcript_event(evt):
            await asyncio.sleep(1)
            print(f"Simulating recognition event... {handler_instance}")
            # simulate an Amazon transcript event
            evt = SimpleNamespace(
                transcript=SimpleNamespace(
                    results=[
                        SimpleNamespace(
                            is_partial=False,
                            alternatives=[
                                SimpleNamespace(transcript="hello world")
                            ],
                        )
                    ]
                )
            )
            # simulate event triggering by handler
            await handler_instance.on_transcript_event_cb(evt)

        # Simulate `start_stream_transcription`
        async def start_stream_side_effect(*args, **kwargs):
            await asyncio.sleep(0.1)
            asyncio.create_task(handle_transcript_event(None))
            return event_stream_mock

        mock_client_instance.start_stream_transcription.side_effect = (
            start_stream_side_effect
        )
        MockHandler.return_value = handler_instance

        yield SimpleNamespace(
            client=mock_client_instance,
            stream=event_stream_mock,
            input_stream=fake_input_stream,
            output_stream=output_stream_mock,
            handler=handler_instance,
        )
