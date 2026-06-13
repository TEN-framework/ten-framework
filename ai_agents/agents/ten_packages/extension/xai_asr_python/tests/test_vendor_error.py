import asyncio
from unittest.mock import AsyncMock, MagicMock

from xai_asr_python.extension import XAIASRExtension


def test_vendor_error_reports_non_fatal_with_vendor_info():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.send_asr_error = AsyncMock()

        await extension.on_error("temporary websocket error", 499)

        error = extension.send_asr_error.await_args.args[0]
        vendor_info = extension.send_asr_error.await_args.args[1]

        assert error.code == 1000
        assert vendor_info.vendor == "xai"
        assert vendor_info.code == "499"
        assert vendor_info.message == "temporary websocket error"

    asyncio.run(_run())


def test_vendor_error_reports_fatal_for_unauthorized():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.send_asr_error = AsyncMock()

        await extension.on_error("401 Unauthorized", 401)

        error = extension.send_asr_error.await_args.args[0]
        vendor_info = extension.send_asr_error.await_args.args[1]

        assert error.code == -1000
        assert vendor_info.vendor == "xai"
        assert vendor_info.code == "401"
        assert vendor_info.message == "401 Unauthorized"

    asyncio.run(_run())
