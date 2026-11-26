#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from unittest.mock import AsyncMock, MagicMock

from .mock import (
    FakeClient,
    FakeFrame,
    finalize_data,
    make_extension,
    patch_client,
)


def test_dump_holds_exact_input_bytes_without_finalize_silence():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            extension.audio_dumper = MagicMock()
            extension.audio_dumper.push_bytes = AsyncMock()
            await extension._ensure_connection()

            pcm = bytes(range(256)) * 4
            frame = FakeFrame(pcm)
            assert await extension.send_audio(frame, "s1") is True
            await extension.on_data(env, finalize_data("f-1", "s1"))

            extension.audio_dumper.push_bytes.assert_awaited_once_with(pcm)
            assert FakeClient.instances[0].sent[0] == pcm
            assert frame.unlocked == 1

    asyncio.run(_run())


def test_send_audio_when_disconnected_returns_false():
    async def _run():
        extension, _ = make_extension()
        extension.audio_dumper = MagicMock()
        extension.audio_dumper.push_bytes = AsyncMock()
        frame = FakeFrame(b"\x00\x01" * 160)

        assert await extension.send_audio(frame, "s1") is False
        extension.audio_dumper.push_bytes.assert_not_awaited()
        assert frame.unlocked == 0

    asyncio.run(_run())
