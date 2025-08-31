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
    Loc,
)


class TestExtension4(Extension):
    original_graph_receiver_extension: str
    original_graph_id: str

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.original_graph_receiver_extension = ""
        self.original_graph_id = ""

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "A":
            self._handle_a_cmd(ten_env, cmd)
        elif cmd_name == "set_original_graph_info":
            self._handle_set_original_graph_info_cmd(ten_env, cmd)
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
        data.set_dests(
            [
                Loc(
                    "",
                    self.original_graph_id,
                    self.original_graph_receiver_extension,
                )
            ]
        )

        def data_callback(ten_env: TenEnv, error: TenError | None):
            if error is not None:
                ten_env.log(LogLevel.ERROR, f"Failed to send data: {error}")

        ten_env.send_data(data, data_callback)

    def _handle_set_original_graph_info_cmd(
        self, ten_env: TenEnv, cmd: Cmd
    ) -> None:
        self.original_graph_receiver_extension, _ = cmd.get_property_string(
            "original_graph_receiver_extension"
        )
        self.original_graph_id, _ = cmd.get_property_string("original_graph_id")

        # Return OK result
        result = CmdResult.create(StatusCode.OK, cmd)
        ten_env.return_result(result)
