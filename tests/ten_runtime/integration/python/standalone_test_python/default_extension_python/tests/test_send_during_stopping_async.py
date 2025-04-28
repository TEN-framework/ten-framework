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
)


class AsyncExtensionTesterBasic(AsyncExtensionTester):
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        register_cmd = Cmd.create("register")

        ten_env.log_info("send register cmd")
        await ten_env.send_cmd(register_cmd)

        ten_env.stop_test()

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        unregister_cmd = Cmd.create("unregister")

        ten_env.log_info("send unregister cmd")
        await ten_env.send_cmd(unregister_cmd)

        ten_env.log_info("tester on_stop_done")


def test_basic():
    tester = AsyncExtensionTesterBasic()
    tester.set_test_mode_single("default_extension_python")
    tester.run()


if __name__ == "__main__":
    test_basic()
