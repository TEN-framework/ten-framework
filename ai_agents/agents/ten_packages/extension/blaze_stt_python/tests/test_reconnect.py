#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.message import ModuleErrorCode

from .. import extension as extension_module
from ..config import BlazeSTTConfig
from ..extension import BlazeSTTExtension
from ..recognition import BlazeSTTAuthError
from ..reconnect_manager import ReconnectManager


def _manager(max_attempts: int = 4) -> ReconnectManager:
    return ReconnectManager(
        base_delay=0, max_delay=0, max_attempts=max_attempts, logger=MagicMock()
    )


def _extension(max_attempts: int = 4) -> BlazeSTTExtension:
    extension = BlazeSTTExtension("blaze_stt_python")
    extension.ten_env = MagicMock()
    # The base class reports connection status through send_data.
    extension.ten_env.send_data = AsyncMock()
    extension.config = BlazeSTTConfig(params={"api_key": "k"})
    extension.config.apply_defaults()
    extension.reconnect_manager = _manager(max_attempts)
    extension.send_asr_error = AsyncMock()
    return extension


class FakeRecognition:
    """Stands in for BlazeSTTRecognition; close() fires on_close like the
    real message handler does when its socket ends."""

    instances: list["FakeRecognition"] = []
    fail_start: Exception | None = None

    def __init__(self, config, audio_timeline, ten_env, callback):
        del config, audio_timeline, ten_env
        self.callback = callback
        self.closed = False
        FakeRecognition.instances.append(self)

    async def start(self):
        if FakeRecognition.fail_start is not None:
            raise FakeRecognition.fail_start

    async def close(self):
        self.closed = True
        await self.callback.on_close()

    def is_connected(self):
        return not self.closed


def _patch_recognition(fail_start: Exception | None = None):
    FakeRecognition.instances = []
    FakeRecognition.fail_start = fail_start
    return patch.object(
        extension_module, "BlazeSTTRecognition", FakeRecognition
    )


def test_reconnect_manager_escalates_after_max_attempts():
    async def _run():
        errors = []
        manager = _manager(max_attempts=4)

        async def failing_connect():
            raise RuntimeError("disconnect")

        async def error_handler(error, vendor_info=None):
            errors.append((error.code, vendor_info))

        for _ in range(4):
            await manager.handle_reconnect(
                failing_connect,
                error_handler,
                vendor_name="blaze",
                vendor_code="connect_failed",
            )

        assert [code for code, _ in errors] == [
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.FATAL_ERROR.value),
        ]
        assert all(info.vendor == "blaze" for _, info in errors)
        assert not manager.can_retry()

    asyncio.run(_run())


def test_reconnect_counter_resets_after_success():
    async def _run():
        manager = _manager(max_attempts=2)

        async def failing_connect():
            raise RuntimeError("disconnect")

        await manager.handle_reconnect(failing_connect)
        assert manager.attempts == 1
        manager.mark_connection_successful()
        assert manager.attempts == 0
        assert manager.can_retry()

    asyncio.run(_run())


def test_unexpected_close_reconnects_with_fresh_socket():
    async def _run():
        extension = _extension()
        with _patch_recognition():
            await extension.start_connection()
            first = FakeRecognition.instances[0]

            # Server drops the socket.
            await first.close()

            assert len(FakeRecognition.instances) == 2
            assert extension.recognition is FakeRecognition.instances[1]
            assert extension.reconnect_manager.attempts == 1
            extension.send_asr_error.assert_not_awaited()

    asyncio.run(_run())


def test_replacing_socket_does_not_trigger_reconnect_loop():
    async def _run():
        extension = _extension()
        with _patch_recognition():
            await extension.start_connection()
            await extension._connect_recognition()

            # Old socket closed once, exactly one replacement, no retries.
            assert len(FakeRecognition.instances) == 2
            assert FakeRecognition.instances[0].closed
            assert extension.reconnect_manager.attempts == 0

    asyncio.run(_run())


def test_close_after_stop_connection_does_not_reconnect():
    async def _run():
        extension = _extension()
        with _patch_recognition():
            await extension.start_connection()
            await extension.stop_connection()

            assert len(FakeRecognition.instances) == 1
            assert extension.recognition is None
            assert extension.reconnect_manager.attempts == 0

    asyncio.run(_run())


def test_connect_failure_retries_until_ceiling_then_fatal():
    async def _run():
        extension = _extension(max_attempts=3)
        with _patch_recognition(fail_start=OSError("network down")):
            await extension.start_connection()

        # First failure is reported by start_connection, then 3 retries.
        codes = [
            int(call.args[0].code)
            for call in extension.send_asr_error.await_args_list
        ]
        assert codes == [
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.NON_FATAL_ERROR.value),
            int(ModuleErrorCode.FATAL_ERROR.value),
        ]
        assert not extension.reconnect_manager.can_retry()

    asyncio.run(_run())


def test_auth_failure_is_fatal_and_never_retries():
    async def _run():
        extension = _extension()
        with _patch_recognition(fail_start=BlazeSTTAuthError("bad token")):
            await extension.start_connection()

        error, vendor_info = extension.send_asr_error.await_args.args
        assert extension.send_asr_error.await_count == 1
        assert int(error.code) == int(ModuleErrorCode.FATAL_ERROR.value)
        assert vendor_info.code == "401"
        assert extension.reconnect_manager.attempts == 0

    asyncio.run(_run())
