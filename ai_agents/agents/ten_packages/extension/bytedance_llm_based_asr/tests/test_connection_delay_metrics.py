#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import os
import sys
import time
import types
from unittest.mock import AsyncMock, MagicMock

extension_dir = os.path.join(os.path.dirname(__file__), "..")
sys.path.insert(0, extension_dir)

package = types.ModuleType("bytedance_llm_based_asr")
package.__path__ = [extension_dir]
sys.modules["bytedance_llm_based_asr"] = package

from bytedance_llm_based_asr import extension as extension_module

sys.modules["bytedance_llm_based_asr.extension"] = extension_module

from bytedance_llm_based_asr.extension import BytedanceASRLLMExtension


def test_send_connection_cnt_metrics_reports_current_count():
    async def _run():
        extension = BytedanceASRLLMExtension("bytedance_llm_based_asr")
        extension._send_asr_metrics = AsyncMock()

        await extension._send_connection_cnt_metrics()

        extension._send_asr_metrics.assert_awaited_once()
        metrics = extension._send_asr_metrics.await_args.args[0]
        assert metrics.metrics == {"connection_cnt": 0}

    asyncio.run(_run())


def test_on_connected_sends_connect_delay_metrics():
    async def _run():
        extension = BytedanceASRLLMExtension("bytedance_llm_based_asr")
        extension.ten_env = MagicMock()
        extension.send_connect_delay_metrics = AsyncMock()
        extension._send_connection_cnt_metrics = AsyncMock()
        extension.connection_start_timestamp = int(time.time() * 1000) - 75

        extension._on_connected()

        await asyncio.sleep(0)
        extension.send_connect_delay_metrics.assert_awaited_once()
        delay_ms = extension.send_connect_delay_metrics.await_args.args[0]
        assert delay_ms >= 50
        assert extension.connection_cnt == 1
        extension._send_connection_cnt_metrics.assert_awaited_once()

    asyncio.run(_run())


def test_connection_cnt_on_connect_and_disconnect():
    async def _run():
        extension = BytedanceASRLLMExtension("bytedance_llm_based_asr")
        extension.ten_env = MagicMock()
        extension.send_connect_delay_metrics = AsyncMock()
        extension._send_connection_cnt_metrics = AsyncMock()
        extension.connection_start_timestamp = int(time.time() * 1000)

        extension._on_connected()
        await asyncio.sleep(0)
        assert extension.connection_cnt == 1
        assert extension.connected is True

        extension._on_disconnected()
        await asyncio.sleep(0)
        assert extension.connection_cnt == 0
        assert extension.connected is False
        assert extension._send_connection_cnt_metrics.await_count == 2

        extension._on_connected()
        await asyncio.sleep(0)
        assert extension.connection_cnt == 1
        assert extension._send_connection_cnt_metrics.await_count == 3

    asyncio.run(_run())
