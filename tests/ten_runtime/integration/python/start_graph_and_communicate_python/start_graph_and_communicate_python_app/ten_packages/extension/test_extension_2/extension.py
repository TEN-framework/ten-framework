#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    TenError,
    LogLevel,
)


class TestExtension2(Extension):
    def __init__(self, name: str) -> None:
        super().__init__(name)

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "start":
            
            self._handle_start_cmd(ten_env, cmd)
        else:
            ten_env.log(
                LogLevel.ERROR,
                f"test_extension_2 received unexpected cmd: {cmd_name}",
            )

    def _handle_start_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_a = Cmd.create("A")

        def callback(
            ten_env: TenEnv,
            _cmd_result: CmdResult | None,
            _error: TenError | None,
        ):
            # Return result for the start command
            result = CmdResult.create(StatusCode.OK, cmd)
            ten_env.return_result(result)

        ten_env.send_cmd(cmd_a, callback)
