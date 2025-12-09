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
    LogLevel,
)
from pytest_ten import TenTestContext, ten_test


class AsyncExtensionTesterBasic(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.receive_goodbye_cmd_event: asyncio.Event = asyncio.Event()

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        flush_cmd = Cmd.create("flush")
        asyncio.create_task(ten_env.send_cmd(flush_cmd))

    async def on_cmd(self, ten_env: AsyncTenEnvTester, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log(LogLevel.INFO, "tester on_cmd name {}".format(cmd_name))

        if cmd_name == "flush":
            ten_env.stop_test()

            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            await ten_env.return_result(cmd_result)
        elif cmd_name == "goodbye":
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            await ten_env.return_result(cmd_result)

            self.receive_goodbye_cmd_event.set()

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        # If the tester stop before the goodbye cmd is received, the
        # default_async_extension_python will hang in on_stop(). So we need to
        # wait for the goodbye cmd to be received.
        await self.receive_goodbye_cmd_event.wait()


def test_basic():
    tester = AsyncExtensionTesterBasic()

    properties = {
        "send_goodbye_cmd": True,
    }

    tester.set_test_mode_single(
        "default_async_extension_python", json.dumps(properties)
    )
    tester.run()


@ten_test(
    "default_async_extension_python", json.dumps({"send_goodbye_cmd": True})
)
async def test_basic_echo(ctx: TenTestContext):
    flush_cmd = Cmd.create("flush")
    asyncio.create_task(ctx.send_cmd(flush_cmd))

    # Expect the extension's echo
    await ctx.expect_cmd("flush")


if __name__ == "__main__":
    test_basic()
