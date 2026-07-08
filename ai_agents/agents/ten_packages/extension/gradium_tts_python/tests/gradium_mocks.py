#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Shared helpers for mocking the streaming GradiumTTSClient in tests."""

import asyncio
from unittest.mock import AsyncMock, MagicMock

from gradium_tts_python.gradium_tts import (
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
)


def make_streaming_mock_client(
    *,
    sent_texts: list | None = None,
    audio_chunks: tuple[bytes, ...] = (b"\x00\x01" * 100,),
    ttfb_ms: int | None = 125,
    ready_sample_rate: int = 24000,
    extra_metadata: dict | None = None,
    error: bytes | None = None,
) -> MagicMock:
    """Build a mock GradiumTTSClient mimicking the streaming session API.

    - ``start_session()`` seeds a fresh per-request event queue (ttfb + audio,
      or an error event).
    - ``send_text()`` records text into ``sent_texts`` if provided.
    - ``end_input()`` pushes the terminal END event, so the reader finalizes
      only after the caller signals end-of-input (matching the real vendor,
      which closes the stream after ``end_of_stream``).
    - ``audio_events()`` drains the current queue.
    """
    mock = MagicMock()
    mock.start = AsyncMock()
    mock.clean = AsyncMock()
    mock.cancel = AsyncMock()
    mock.get_ready_sample_rate.return_value = ready_sample_rate
    mock.get_extra_metadata.return_value = extra_metadata or {}

    state: dict = {"queue": None}

    async def _start_session() -> None:
        queue: asyncio.Queue = asyncio.Queue()
        if ttfb_ms is not None:
            queue.put_nowait((ttfb_ms, EVENT_TTS_TTFB_METRIC))
        for chunk in audio_chunks:
            queue.put_nowait((chunk, EVENT_TTS_RESPONSE))
        if error is not None:
            queue.put_nowait((error, EVENT_TTS_ERROR))
        state["queue"] = queue

    mock.start_session = AsyncMock(side_effect=_start_session)

    async def _send_text(text: str) -> None:
        if sent_texts is not None:
            sent_texts.append(text)

    mock.send_text = AsyncMock(side_effect=_send_text)

    async def _end_input() -> None:
        if state["queue"] is not None:
            state["queue"].put_nowait((None, EVENT_TTS_END))

    mock.end_input = AsyncMock(side_effect=_end_input)

    def _audio_events():
        queue = state["queue"]

        async def _gen():
            while True:
                item = await queue.get()
                yield item
                if item[1] in (EVENT_TTS_END, EVENT_TTS_ERROR):
                    return

        return _gen()

    mock.audio_events.side_effect = _audio_events
    return mock
