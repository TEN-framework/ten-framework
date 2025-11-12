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
    Data,
    StatusCode,
    CmdResult,
    TenError,
    LogLevel,
)


class TestExtension4(Extension):

    def __init__(self, name: str) -> None:
        super().__init__(name)

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "A":
            self._handle_a_cmd(ten_env, cmd)
        else:
            ten_env.log(
                LogLevel.ERROR,
                f"test_extension_4 received unexpected cmd: {cmd_name}",
            )

    def _handle_a_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        # Return OK result for command A
        result = CmdResult.create(StatusCode.OK, cmd)
        ten_env.return_result(result)

        # Send data back to the original graph
        data = Data.create("data_from_new_graph")

        def data_callback(ten_env: TenEnv, error: TenError | None):
            if error is not None:
                ten_env.log(LogLevel.ERROR, f"Failed to send data: {error}")

        ten_env.send_data(data, data_callback)
