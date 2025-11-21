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

        if cmd_name == "set_original_graph_info":
            self._handle_set_original_graph_info_cmd(ten_env, cmd)
        elif cmd_name == "start":
            self._handle_start_cmd(ten_env, cmd)
        else:
            ten_env.log(
                LogLevel.ERROR,
                f"test_extension_2 received unexpected cmd: {cmd_name}",
            )

    def _handle_set_original_graph_info_cmd(
        self, ten_env: TenEnv, cmd: Cmd
    ) -> None:
        # Get the original graph receiver extension and ID.
        original_graph_receiver_extension, _ = cmd.get_property_string(
            "original_graph_receiver_extension"
        )
        loc = cmd.get_source()
        original_graph_id = loc.graph_id
        assert original_graph_id is not None

        cmd_set_original_graph_info = Cmd.create("set_original_graph_info")
        cmd_set_original_graph_info.set_property_string(
            "original_graph_receiver_extension",
            original_graph_receiver_extension,
        )
        cmd_set_original_graph_info.set_property_string(
            "original_graph_id", original_graph_id
        )

        def callback(
            ten_env: TenEnv,
            _cmd_result: CmdResult | None,
            _error: TenError | None,
        ):
            # Return result for the original command
            result = CmdResult.create(StatusCode.OK, cmd)
            ten_env.return_result(result)

        ten_env.send_cmd(cmd_set_original_graph_info, callback)

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
