#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import pytest
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.struct import ASRResult


@pytest.fixture(scope="function")
def patch_speechmatics_ws():
    with patch(
        "ten_packages.extension.speechmatics_asr_python.extension.SpeechmaticsASRClient"
    ) as MockClient:
        mock_client = MagicMock()

        # We mock only the client logic; transcription is now handled by event
        def mock_constructor(config, ten_env):
            mock_client.on_asr_result = None  # Will be set by extension
            return mock_client

        MockClient.side_effect = mock_constructor

        async def mock_start():
            async def delayed_transcription():
                await asyncio.sleep(1)
                if mock_client.on_asr_result:
                    await mock_client.on_asr_result(
                        ASRResult(
                            text="hello world",
                            final=True,
                            start_ms=0,
                            duration_ms=1000,
                            language="en-US",
                            words=[],
                            metadata={"session_id": "test"},
                        )
                    )

            asyncio.get_event_loop().create_task(delayed_transcription())

        from types import SimpleNamespace

        mock_client.start = AsyncMock(side_effect=mock_start)
        mock_client.stop = AsyncMock()
        mock_client.recv_audio_frame = AsyncMock()
        mock_client.internal_drain_mute_pkg = AsyncMock()
        mock_client.internal_drain_disconnect = AsyncMock()
        mock_client.client = MagicMock(session_running=True)

        yield mock_client
