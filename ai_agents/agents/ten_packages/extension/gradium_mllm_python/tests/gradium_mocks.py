"""Shared helpers for mocking the streaming GradiumS2SClient in tests."""

import asyncio
from typing import Any
from unittest.mock import AsyncMock, MagicMock


def make_streaming_mock_client(
    *,
    messages: list[dict[str, Any]] | None = None,
    connect_error: Exception | None = None,
    sent_audio: list[bytes] | None = None,
) -> MagicMock:
    """Build a mock GradiumS2SClient mimicking the real websocket client's API.

    - ``connect()`` raises ``connect_error`` if given, else succeeds
      immediately (mirrors the real client's connect() only returning after
      Gradium's "ready" has already been received and validated).
    - ``messages()`` yields the canned ``messages`` list in order, then
      blocks forever -- keeps the extension's receive loop "connected" for
      the rest of the test instead of exiting and triggering a reconnect.
    - ``send_audio()`` records bytes into ``sent_audio`` if provided.
    - ``close()`` / ``send_end_of_stream()`` are no-op AsyncMocks.
    """
    mock = MagicMock()

    if connect_error is not None:
        mock.connect = AsyncMock(side_effect=connect_error)
    else:
        mock.connect = AsyncMock()

    mock.close = AsyncMock()
    mock.send_end_of_stream = AsyncMock()

    async def _send_audio(pcm_bytes: bytes) -> None:
        if sent_audio is not None:
            sent_audio.append(pcm_bytes)

    mock.send_audio = AsyncMock(side_effect=_send_audio)

    async def _messages():
        for m in messages or []:
            yield m
        await asyncio.Event().wait()

    mock.messages.side_effect = _messages
    return mock
