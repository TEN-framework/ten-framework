#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from unittest.mock import AsyncMock, MagicMock

from ..config import BlazeSTTConfig
from ..extension import BlazeSTTExtension


class FakeFrame:
    def __init__(self, data: bytes):
        self.buf = bytearray(data)
        self.unlocked = 0

    def lock_buf(self):
        return self.buf

    def unlock_buf(self, _buf):
        self.unlocked += 1


def _extension(connected: bool) -> BlazeSTTExtension:
    extension = BlazeSTTExtension("blaze_stt_python")
    extension.ten_env = MagicMock()
    extension.audio_timeline = MagicMock()
    extension.config = BlazeSTTConfig(params={"language": "vi"})
    extension.config.apply_defaults()
    extension.audio_dumper = MagicMock()
    extension.audio_dumper.push_bytes = AsyncMock()
    extension.recognition = MagicMock()
    extension.recognition.is_connected.return_value = connected
    extension.recognition.send_audio_frame = AsyncMock()
    return extension


def test_send_audio_dumps_exact_bytes():
    async def _run():
        extension = _extension(connected=True)
        pcm = bytes(range(256)) * 4
        frame = FakeFrame(pcm)

        assert await extension.send_audio(frame, "session-1") is True

        extension.audio_dumper.push_bytes.assert_awaited_once_with(pcm)
        extension.recognition.send_audio_frame.assert_awaited_once_with(pcm)
        assert frame.unlocked == 1

    asyncio.run(_run())


def test_send_audio_when_disconnected_returns_false():
    async def _run():
        extension = _extension(connected=False)
        frame = FakeFrame(b"\x00\x01" * 160)

        assert await extension.send_audio(frame, "session-1") is False

        extension.audio_dumper.push_bytes.assert_not_awaited()
        extension.recognition.send_audio_frame.assert_not_awaited()
        assert frame.unlocked == 0

    asyncio.run(_run())


def test_send_audio_without_recognition_returns_false():
    async def _run():
        extension = _extension(connected=True)
        extension.recognition = None

        assert await extension.send_audio(FakeFrame(b"\x00\x00"), None) is False

    asyncio.run(_run())
