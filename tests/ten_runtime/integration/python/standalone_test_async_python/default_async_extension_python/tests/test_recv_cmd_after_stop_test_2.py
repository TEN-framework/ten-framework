#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import json
from ten_runtime import (
    Cmd,
    AsyncExtensionTester,
    AsyncTenEnvTester,
    CmdResult,
    StatusCode,
)


class AsyncExtensionTesterBasic(AsyncExtensionTester):
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        flush_cmd = Cmd.create("flush")
        asyncio.create_task(ten_env.send_cmd(flush_cmd))

    async def on_cmd(self, ten_env: AsyncTenEnvTester, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        # ten_env.log_info("tester on_cmd name {}".format(cmd_name))

        if cmd_name == "flush":
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            await ten_env.return_result(cmd_result)

            ten_env.stop_test()
        elif cmd_name == "goodbye":
            assert False, "should not receive goodbye cmd"


def test_recv_cmd_after_stop_2():
    tester = AsyncExtensionTesterBasic()

    # Since the tester deinit before receiving the goodbye cmd,
    # the extension will receive an error, but we consider this is expected.
    properties = {
        "send_goodbye_cmd": True,
        "sleep_ms_before_goodbye": 1000,
        "assert_goodbye_result_success": False,
    }

    tester.set_test_mode_single(
        "default_async_extension_python", json.dumps(properties)
    )
    tester.run()


if __name__ == "__main__":
    test_recv_cmd_after_stop_2()
