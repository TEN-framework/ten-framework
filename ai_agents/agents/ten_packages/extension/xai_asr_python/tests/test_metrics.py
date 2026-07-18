import asyncio
import time
from unittest.mock import AsyncMock, MagicMock

from xai_asr_python.extension import XAIASRExtension


class FakeTimeline:
    def get_total_user_audio_duration(self) -> int:
        return 320

    def reset(self) -> None:
        return None


def test_on_open_sends_connect_delay_metrics():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.audio_timeline = FakeTimeline()
        extension.buffered_frames = asyncio.Queue()
        extension.send_connect_delay_metrics = AsyncMock()
        extension.reconnect_manager = MagicMock()
        extension.connection_start_timestamp = int(time.time() * 1000) - 75

        await extension.on_open()

        extension.send_connect_delay_metrics.assert_awaited_once()
        delay_ms = extension.send_connect_delay_metrics.await_args.args[0]
        assert delay_ms >= 50
        extension.reconnect_manager.mark_connection_successful.assert_called_once()

    asyncio.run(_run())
