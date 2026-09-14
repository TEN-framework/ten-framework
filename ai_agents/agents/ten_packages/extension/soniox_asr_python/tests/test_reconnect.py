#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json
from unittest.mock import AsyncMock, patch

from ten_ai_base.message import ModuleErrorCode
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
)
from typing_extensions import override

from ..reconnect_manager import ReconnectManager

FATAL_ERROR_CODE = int(ModuleErrorCode.FATAL_ERROR.value)
NON_FATAL_ERROR_CODE = int(ModuleErrorCode.NON_FATAL_ERROR.value)


class ReconnectAttemptTester(AsyncExtensionTester):
    def __init__(
        self,
        *,
        min_non_fatal_errors: int = 0,
        fatal_observation_seconds: float = 0.0,
    ):
        super().__init__()
        self.min_non_fatal_errors = min_non_fatal_errors
        self.fatal_observation_seconds = fatal_observation_seconds
        self.error_codes: list[int] = []
        self.fatal_error_received = False
        self.fatal_observation_task: asyncio.Task[None] | None = None

    async def _observe_after_fatal(
        self, ten_env_tester: AsyncTenEnvTester
    ) -> None:
        await asyncio.sleep(self.fatal_observation_seconds)
        ten_env_tester.stop_test()

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        if data.get_name() != "error":
            return

        payload = json.loads(data.get_property_to_json()[0])
        raw_error_code = payload.get("code")
        if raw_error_code is None:
            return
        error_code = int(raw_error_code)

        self.error_codes.append(error_code)
        if error_code == FATAL_ERROR_CODE:
            self.fatal_error_received = True
            if self.fatal_observation_task is None:
                self.fatal_observation_task = asyncio.create_task(
                    self._observe_after_fatal(ten_env_tester)
                )
            return

        non_fatal_errors = self.error_codes.count(NON_FATAL_ERROR_CODE)
        if non_fatal_errors >= self.min_non_fatal_errors:
            ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        task = self.fatal_observation_task
        if task and task is not asyncio.current_task():
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass


def test_invalid_url_non_fatal_error_keeps_reconnecting(patch_soniox_ws):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    connect_count = {"count": 0}

    async def custom_connect():
        connect_count["count"] += 1
        await asyncio.sleep(0.05)
        await patch_soniox_ws.websocket_client.trigger_exception(
            OSError("[Errno -2] Name or service not known")
        )
        await patch_soniox_ws.websocket_client.trigger_close(0, "closed")

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws, on_connect=custom_connect
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    property_json = (
        '{"url":"wss://invalid.soniox.test/transcribe-websocket",'
        '"params":{"api_key":"valid_key"}}'
    )
    tester = ReconnectAttemptTester(min_non_fatal_errors=3)
    tester.set_test_mode_single("soniox_asr_python", property_json)
    err = tester.run()
    assert err is None, (
        "test_invalid_url_non_fatal_error_keeps_reconnecting " f"err: {err}"
    )
    assert len(tester.error_codes) >= 3
    assert all(code == NON_FATAL_ERROR_CODE for code in tester.error_codes)
    assert connect_count["count"] >= 3


def test_invalid_api_key_fatal_error_stops_reconnect(patch_soniox_ws):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    connect_count = {"count": 0}

    async def custom_connect():
        connect_count["count"] += 1
        await patch_soniox_ws.websocket_client.trigger_open(0)
        await asyncio.sleep(0.05)
        await patch_soniox_ws.websocket_client.trigger_error(
            401, "Unauthorized"
        )
        await patch_soniox_ws.websocket_client.trigger_close(
            401, "Unauthorized"
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws, on_connect=custom_connect
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    property_json = '{"params":{"api_key":"invalid_key"}}'
    tester = ReconnectAttemptTester(fatal_observation_seconds=1.0)
    tester.set_test_mode_single("soniox_asr_python", property_json)
    err = tester.run()
    assert err is None, (
        "test_invalid_api_key_fatal_error_stops_reconnect " f"err: {err}"
    )
    assert tester.fatal_error_received
    assert connect_count["count"] == 1
    assert tester.error_codes == [FATAL_ERROR_CODE]


def test_reconnect_backoff_stays_capped_for_large_attempt_count():
    manager = ReconnectManager()
    manager.attempts = 10_000
    connection_func = AsyncMock()

    with patch(
        "soniox_asr_python.reconnect_manager.asyncio.sleep",
        new_callable=AsyncMock,
    ) as sleep_mock:
        result = asyncio.run(manager.handle_reconnect(connection_func))

    assert result
    sleep_mock.assert_awaited_once_with(manager.max_delay)
    connection_func.assert_awaited_once_with()
