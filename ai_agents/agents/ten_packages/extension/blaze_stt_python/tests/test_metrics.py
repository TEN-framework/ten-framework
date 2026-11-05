#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import time
from unittest.mock import AsyncMock, MagicMock

from ..extension import BlazeSTTExtension


class FakeTimeline:
    def __init__(self):
        self.reset_calls = 0

    def get_total_user_audio_duration(self) -> int:
        return 320

    def reset(self) -> None:
        self.reset_calls += 1


def test_on_open_sends_connect_delay_metrics():
    async def _run():
        extension = BlazeSTTExtension("blaze_stt_python")
        extension.ten_env = MagicMock()
        extension.audio_timeline = FakeTimeline()
        extension.send_connect_delay_metrics = AsyncMock()
        extension.reconnect_manager = MagicMock()
        extension.connection_start_timestamp = int(time.time() * 1000) - 75

        await extension.on_open()

        extension.send_connect_delay_metrics.assert_awaited_once()
        delay_ms = extension.send_connect_delay_metrics.await_args.args[0]
        assert delay_ms >= 50
        extension.reconnect_manager.mark_connection_successful.assert_called_once()
        # Timestamps continue across reconnects: audio sent before this
        # socket is carried forward and the per-socket timeline restarts.
        assert extension.sent_user_audio_duration_ms_before_last_reset == 320
        assert extension.audio_timeline.reset_calls == 1

    asyncio.run(_run())
