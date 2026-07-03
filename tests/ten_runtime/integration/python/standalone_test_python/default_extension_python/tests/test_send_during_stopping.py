#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    TenError,
    LogLevel,
)


class ExtensionTesterBasic(ExtensionTester):
    def on_register_response(
        self,
        ten_env: TenEnvTester,
        result: CmdResult | None,
        error: TenError | None,
    ):
        ten_env.log(LogLevel.INFO, "received register response")
        ten_env.stop_test()

    def on_unregister_response(
        self,
        ten_env: TenEnvTester,
        result: CmdResult | None,
        error: TenError | None,
    ):
        # All ten_env_tester methods will return error because on_deinit_done()
        # of the extension tester is called.
        err = ten_env.log(LogLevel.INFO, "received unregister response")
        assert err is not None

    def on_start(self, ten_env: TenEnvTester) -> None:
        new_cmd = Cmd.create("register")

        ten_env.log(LogLevel.INFO, "send register cmd")
        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.on_register_response(
                ten_env, result, error  # type: ignore
            ),
        )

        ten_env.on_start_done()

    def on_deinit(self, ten_env: TenEnvTester) -> None:
        ten_env.log(LogLevel.INFO, "tester on_deinit")

        new_cmd = Cmd.create("unregister")
        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.on_unregister_response(
                ten_env, result, error  # type: ignore
            ),
        )

        ten_env.on_deinit_done()


def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("default_extension_python")
    tester.run()


if __name__ == "__main__":
    test_basic()
