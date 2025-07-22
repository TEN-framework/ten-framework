#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

import asyncio
import json
import uuid
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    TenError,
    TenErrorCode,
)
from ten_runtime.data import Data


class ExtensionTesterBasic(AsyncExtensionTester):
    def __init__(self) -> None:
        super().__init__()
        self.received_data = []
        self.received_cmds = []

    async def on_init(self, ten_env: AsyncExtensionTester) -> None:
        self._assertion_task = asyncio.create_task(self._assertion(ten_env))

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        data = Data.create("asr_result")

        data.set_property_from_json(
            None,
            json.dumps(
                {
                    "id": str(uuid.uuid4()),
                    "text": "A sample final asr result",
                    "final": True,
                    "start_ms": 0,
                    "duration_ms": 100,
                    "language": "zh-CN",
                    "metadata": {"session_id": "20000"},
                }
            ),
        )

        await ten_env.send_data(data)

    async def on_cmd(self, ten_env: AsyncExtensionTester, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"on_cmd {cmd_name}")
        self.received_cmds.append(cmd.clone())

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncExtensionTester, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"on_data {data_name}")
        self.received_data.append(data.clone())

    async def on_stop(self, ten_env: AsyncExtensionTester) -> None:
        pass

    async def _assertion(self, ten_env: AsyncExtensionTester):
        await asyncio.sleep(1)
        expected_cmd_names = ["flush_llm", "flush_tts", "flush_rtc"]
        expected_data_names = ["llm_request", "pass_message"]
        try:
            assert len(expected_cmd_names) == len(self.received_cmds)
            for exp, got in zip(expected_cmd_names, self.received_cmds):
                assert exp == got.get_name()

            assert len(expected_data_names) == len(self.received_data)
            for exp, got in zip(expected_data_names, self.received_data):
                assert exp == got.get_name()
        except Exception as e:
            ten_env.log_error(str(e))
            test_result = TenError.create(
                TenErrorCode.ErrorCodeGeneric,
                str(e),
            )
            ten_env.stop_test(test_result)
        finally:
            ten_env.stop_test()

def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("main_control_python")

    error = tester.run()
    assert error is None
