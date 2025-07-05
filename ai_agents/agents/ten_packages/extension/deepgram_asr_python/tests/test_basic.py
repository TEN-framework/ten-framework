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
    TenError,
    TenErrorCode,
)


class ExtensionTesterBasic(AsyncExtensionTester):
    def check_hello(self, ten_env: AsyncTenEnvTester, result: CmdResult):
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            ten_env.stop_test()
        else:
            ten_env.log_error("receive hello_world, but status is not OK")
            test_result = TenError.create(
                TenErrorCode.ErrorCodeGeneric,
                "receive hello_world, but status is not OK",
            )
            ten_env.stop_test(test_result)

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        print("send hello_world")
        result, _ = await ten_env.send_cmd(new_cmd)
        if result is not None:
            self.check_hello(ten_env, result)
        else:
            ten_env.log_error("receive hello_world, but result is None")
            test_result = TenError.create(
                TenErrorCode.ErrorCodeGeneric,
                "receive hello_world, but result is None",
            )
            ten_env.stop_test(test_result)


def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("deepgram_asr_python")

    error = tester.run()
    assert error is None
