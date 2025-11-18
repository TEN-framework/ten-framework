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
    LogLevel,
)


class TestExtension3(Extension):
    def __init__(self, name: str) -> None:
        super().__init__(name)

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "B":
            self._handle_b_cmd(ten_env, cmd)
        else:
            ten_env.log(
                LogLevel.ERROR,
                f"test_extension_3 received unexpected cmd: {cmd_name}",
            )

    def _handle_b_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        # Simply return OK status for command B
        result = CmdResult.create(StatusCode.OK, cmd)
        ten_env.return_result(result)
