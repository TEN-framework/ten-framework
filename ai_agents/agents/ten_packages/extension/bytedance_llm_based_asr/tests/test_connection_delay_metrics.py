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


def test_on_connected_sends_connect_delay_metrics():
    async def _run():
        extension = BytedanceASRLLMExtension("bytedance_llm_based_asr")
        extension.ten_env = MagicMock()
        extension.send_connect_delay_metrics = AsyncMock()
        extension.connection_start_timestamp = int(time.time() * 1000) - 75

        extension._on_connected()

        await asyncio.sleep(0)
        extension.send_connect_delay_metrics.assert_awaited_once()
        delay_ms = extension.send_connect_delay_metrics.await_args.args[0]
        assert delay_ms >= 50

    asyncio.run(_run())
