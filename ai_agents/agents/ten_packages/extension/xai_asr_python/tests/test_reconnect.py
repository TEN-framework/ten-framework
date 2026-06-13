import asyncio
from unittest.mock import AsyncMock, MagicMock

from ten_ai_base.message import ModuleErrorCode

from xai_asr_python.extension import XAIASRExtension
from xai_asr_python.reconnect_manager import ReconnectManager


def test_reconnect_manager_escalates_after_max_attempts():
    async def _run():
        errors = []
        manager = ReconnectManager(
            base_delay=0,
            max_delay=0,
            max_attempts=4,
            logger=MagicMock(),
        )

        async def failing_connect():
            raise RuntimeError("disconnect")

        async def error_handler(error, vendor_info=None):
            errors.append((error.code, vendor_info))

        for _ in range(4):
            await manager.handle_reconnect(
                failing_connect,
                error_handler,
                vendor_name="xai",
                vendor_code="connect_failed",
            )

        assert [code for code, _ in errors] == [
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.FATAL_ERROR.value),
        ]
        assert all(vendor_info is not None for _, vendor_info in errors)
        assert all(vendor_info.vendor == "xai" for _, vendor_info in errors)
        assert all(
            vendor_info.code == "connect_failed" for _, vendor_info in errors
        )

    asyncio.run(_run())


def test_reconnect_counter_resets_after_success():
    manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=4)
    manager.attempts = 3
    manager.mark_connection_successful()
    assert manager.attempts == 0


def test_on_close_retries_until_retry_ceiling():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.reconnect_manager = ReconnectManager(
            base_delay=0,
            max_delay=0,
            max_attempts=4,
            logger=MagicMock(),
        )
        extension.send_asr_error = AsyncMock()
        extension._connect_recognition = AsyncMock(
            side_effect=RuntimeError("disconnect")
        )

        await extension.on_close()

        assert extension._connect_recognition.await_count == 4
        observed_codes = [
            call.args[0].code
            for call in extension.send_asr_error.await_args_list
        ]
        assert observed_codes == [
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.FATAL_ERROR.value),
        ]
        observed_vendor_infos = [
            call.args[1] for call in extension.send_asr_error.await_args_list
        ]
        assert all(
            vendor_info.vendor == "xai" for vendor_info in observed_vendor_infos
        )
        assert all(
            vendor_info.code == "connect_failed"
            for vendor_info in observed_vendor_infos
        )

    asyncio.run(_run())
