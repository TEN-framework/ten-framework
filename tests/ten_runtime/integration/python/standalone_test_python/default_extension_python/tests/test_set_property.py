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


class ExtensionTesterSetProperty(ExtensionTester):
    def check_greeting(
        self,
        ten_env: TenEnvTester,
        result: Optional[CmdResult],
        error: Optional[TenError],
    ):
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        ten_env.log_info("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            detail, _ = result.get_property_string("detail")
            assert detail == "hola"

            ten_env.stop_test()

    def on_start(self, ten_env: TenEnvTester) -> None:
        cmd = Cmd.create("greeting")

        ten_env.send_cmd(
            cmd,
            lambda ten_env, result, error: self.check_greeting(
                ten_env, result, error
            ),
        )

        ten_env.log_info("tester on_start_done")
        ten_env.on_start_done()


def test_set_property():
    tester = ExtensionTesterSetProperty()
    tester.set_test_mode_single(
        "default_extension_python", '{"greeting": "hola"}'
    )
    tester.run()


if __name__ == "__main__":
    test_set_property()
