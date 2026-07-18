import asyncio
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock

from ten_ai_base.dumper import Dumper
from xai_asr_python.extension import XAIASRExtension


class FakeFrame:
    def __init__(self, payload: bytes):
        self.payload = bytearray(payload)

    def lock_buf(self):
        return self.payload

    def unlock_buf(self, _buf):
        return None


def test_send_audio_writes_dump_and_vendor_stream(tmp_path):
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        dump_path = Path(tmp_path) / "dump.pcm"
        extension.audio_dumper = Dumper(str(dump_path))
        await extension.audio_dumper.start()
        extension.recognition = MagicMock()
        extension.recognition.is_connected.return_value = True
        extension.recognition.send_audio_frame = AsyncMock()
        payload = b"\x00\x01\x02\x03"

        result = await extension.send_audio(FakeFrame(payload), None)

        assert result is True
        extension.recognition.send_audio_frame.assert_awaited_once_with(payload)
        await extension.audio_dumper.stop()
        assert dump_path.exists()
        assert dump_path.read_bytes() == payload

    asyncio.run(_run())
