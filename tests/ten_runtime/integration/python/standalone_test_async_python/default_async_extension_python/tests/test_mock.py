#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import Optional
from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    TenError,
)


class ExtensionTesterMock(ExtensionTester):
    def check_weather(
        self,
        ten_env: TenEnvTester,
        result: Optional[CmdResult],
        error: Optional[TenError],
    ):
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        ten_env.log_info("receive weather, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            detail, _ = result.get_property_string("detail")
            assert detail == "sunny"

            ten_env.stop_test()

    def on_start(self, ten_env: TenEnvTester) -> None:
        cmd = Cmd.create("query_weather")

        ten_env.send_cmd(
            cmd,
            lambda ten_env, result, error: self.check_weather(
                ten_env, result, error
            ),
        )

        ten_env.on_start_done()

    def on_cmd(self, ten_env: TenEnvTester, cmd: Cmd) -> None:
        ten_env.log_info("ExtensionTesterMock on_cmd: " + cmd.get_name())

        if cmd.get_name() == "query_weather":
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            cmd_result.set_property_string("detail", "sunny")
            ten_env.return_result(cmd_result)


def test_mock_cmd_result():
    tester = ExtensionTesterMock()
    tester.set_test_mode_single("default_async_extension_python")
    tester.run()


if __name__ == "__main__":
    test_mock_cmd_result()
