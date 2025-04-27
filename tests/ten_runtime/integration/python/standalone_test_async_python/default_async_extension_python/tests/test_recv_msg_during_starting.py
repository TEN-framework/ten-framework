#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
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

        # At any stage, we can always receive the result of the command we sent
        # out.
        await ten_env.send_cmd(flush_cmd)

        assert self.recv_flush_cmd
        ten_env.stop_test()

    async def on_cmd(self, ten_env: AsyncTenEnvTester, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info("tester on_cmd name {}".format(cmd_name))

        if cmd_name == "flush":
            self.recv_flush_cmd = True
            await ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))


def test_basic():
    tester = AsyncExtensionTesterBasic()
    tester.set_test_mode_single("default_async_extension_python")
    tester.run()


if __name__ == "__main__":
    test_basic()
