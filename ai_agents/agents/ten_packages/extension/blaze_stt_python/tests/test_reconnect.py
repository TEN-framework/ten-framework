#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

from ten_ai_base.message import ModuleErrorCode

from ..recognition import BlazeSTTAuthError
from .mock import (
    FakeClient,
    make_extension,
    outputs,
    patch_client,
    wait_until,
)

NON_FATAL = int(ModuleErrorCode.NON_FATAL_ERROR.value)
FATAL = int(ModuleErrorCode.FATAL_ERROR.value)


def test_transient_failures_keep_retrying_without_going_fatal():
    async def _run():
        failures = [OSError("network down") for _ in range(6)]
        with patch_client(connect_errors=failures):
            extension, env = make_extension()
            await extension._ensure_connection()
            await wait_until(extension.is_connected, timeout=5.0)

            codes = [error["code"] for error in outputs(env, "error")]
            assert codes == [NON_FATAL] * 6
            assert len(FakeClient.instances) == 7

    asyncio.run(_run())


def test_unexpected_close_reconnects_on_a_fresh_socket():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()
            first = FakeClient.instances[0]

            await first.server_close(1006)
            await wait_until(extension.is_connected)

            assert extension.client is FakeClient.instances[1]
            (error,) = outputs(env, "error")
            assert error["code"] == NON_FATAL
            assert error["vendor_info"]["code"] == "1006"

    asyncio.run(_run())


def test_auth_failure_is_fatal_and_never_retries():
    async def _run():
        with patch_client(connect_errors=[BlazeSTTAuthError("bad token")]):
            extension, env = make_extension()
            await extension._ensure_connection()
            await asyncio.sleep(0.05)

            (error,) = outputs(env, "error")
            assert error["code"] == FATAL
            assert error["vendor_info"]["code"] == "401"
            assert len(FakeClient.instances) == 1
            assert extension._reconnect_task is None

    asyncio.run(_run())


def test_policy_violation_close_is_fatal_and_never_retries():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()
            await FakeClient.instances[0].server_close(1008)
            await asyncio.sleep(0.05)

            (error,) = outputs(env, "error")
            assert error["code"] == FATAL
            assert len(FakeClient.instances) == 1

    asyncio.run(_run())


def test_send_failure_and_later_close_start_one_recovery():
    async def _run():
        with patch_client():
            extension, env = make_extension()
            await extension._ensure_connection()
            first = FakeClient.instances[0]
            first.send_error = ConnectionError("broken pipe")

            from .mock import FakeFrame

            assert not await extension.send_audio(
                FakeFrame(b"\x00" * 320), "s1"
            )
            # The receive loop reports the same dead socket afterwards.
            await first.server_close(1006)
            await wait_until(extension.is_connected)

            assert len(outputs(env, "error")) == 1
            assert len(FakeClient.instances) == 2

    asyncio.run(_run())


def test_http_forbidden_upgrade_is_fatal_with_its_status():
    async def _run():
        rejected = BlazeSTTAuthError("HTTP 403", status_code=403)
        with patch_client(connect_errors=[rejected]):
            extension, env = make_extension()
            await extension._ensure_connection()
            await asyncio.sleep(0.05)

            (error,) = outputs(env, "error")
            assert error["code"] == FATAL
            assert error["vendor_info"]["code"] == "403"
            assert len(FakeClient.instances) == 1

    asyncio.run(_run())
