#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
)


class ExtensionTesterBasic(AsyncExtensionTester):
    def check_hello(self, ten_env: AsyncTenEnvTester, result: CmdResult):
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            # TODO: move stop_test() to where the test passes
            ten_env.stop_test()

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        print("send hello_world")
        result, _ = await ten_env.send_cmd(new_cmd)
        self.check_hello(ten_env, result)



def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("deepgram_asr_python")
    tester.run()