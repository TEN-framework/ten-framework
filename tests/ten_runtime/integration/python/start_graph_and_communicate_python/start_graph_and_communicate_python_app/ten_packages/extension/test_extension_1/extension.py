#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

import json
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
    StatusCode,
    CmdResult,
    LogLevel,
    Loc,
    StartGraphCmd,
    StopGraphCmd,
)


class TestExtension1(AsyncExtension):
    received_data_from_new_graph: bool
    new_graph_id: str
    test_cmd: Cmd | None

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.received_data_from_new_graph = False
        self.new_graph_id = ""
        self.test_cmd = None

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        # Start a new graph
        start_graph_cmd = StartGraphCmd.create()

        # The destination of the 'start_graph' command is the current app,
        # using "" to represent current app.
        start_graph_cmd.set_dests([Loc("")])

        # The new graph contains 3 extensions.
        graph_json = {
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_extension_2",
                    "addon": "test_extension_2",
                },
                {
                    "type": "extension",
                    "name": "test_extension_3",
                    "addon": "test_extension_3",
                },
                {
                    "type": "extension",
                    "name": "test_extension_4",
                    "addon": "test_extension_4",
                },
            ],
            "connections": [
                {
                    "extension": "test_extension_2",
                    "cmd": [
                        {
                            "name": "A",
                            "dest": [
                                {
                                    "extension": "test_extension_3",
                                    "msg_conversion": {
                                        "keep_original": True,
                                        "type": "per_property",
                                        "rules": [
                                            {
                                                "path": "ten.name",
                                                "conversion_mode": "fixed_value",
                                                "value": "B",
                                            }
                                        ],
                                    },
                                },
                                {"extension": "test_extension_4"},
                            ],
                        },
                        {
                            "name": "set_original_graph_info",
                            "dest": [{"extension": "test_extension_4"}],
                        },
                    ],
                }
            ],
            "exposed_messages": [
                {
                    "type": "cmd_in",
                    "name": "start",
                    "extension": "test_extension_2",
                },
                {
                    "type": "data_out",
                    "name": "data_from_new_graph",
                    "extension": "test_extension_4",
                },
            ],
        }

        start_graph_cmd.set_graph_from_json(json.dumps(graph_json))

        cmd_result, error = await ten_env.send_cmd(start_graph_cmd)
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"Start graph failed: {error}")
            return

        if cmd_result is None:
            ten_env.log(LogLevel.ERROR, "Start graph cmd_result is None")
            return

        self.new_graph_id, _ = cmd_result.get_property_string("graph_id")
        ten_env.log_info(f"new_graph_id: {self.new_graph_id}")

        # Send a 'start' command to test_extension_2
        cmd_start = Cmd.create("start")
        cmd_start.set_dests([Loc("", self.new_graph_id)])
        result, error = await ten_env.send_cmd(cmd_start)
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"Start command failed: {error}")
            return

        if result is None:
            ten_env.log(LogLevel.ERROR, "Start command result is None")
            return

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_stop")

        # Stop the started graph

        stop_graph_cmd = StopGraphCmd.create()
        stop_graph_cmd.set_dests([Loc("")])
        stop_graph_cmd.set_graph_id(self.new_graph_id)

        result, error = await ten_env.send_cmd(stop_graph_cmd)
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"Stop graph failed: {error}")
            return

        if result is None:
            ten_env.log(LogLevel.ERROR, "Stop graph result is None")
            return

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "test":
            self.test_cmd = cmd

            if self.received_data_from_new_graph:
                # Send the response to the client.
                await self._reply_to_client(ten_env)
        else:
            ten_env.log(
                LogLevel.ERROR,
                f"Should not happen - unknown command: {cmd_name}",
            )

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()

        if data_name == "data_from_new_graph":
            self.received_data_from_new_graph = True

            if self.test_cmd is not None:
                await self._reply_to_client(ten_env)
        else:
            ten_env.log(
                LogLevel.ERROR, f"Should not happen - unknown data: {data_name}"
            )

    async def _reply_to_client(self, ten_env: AsyncTenEnv) -> None:
        print("reply to client")

        assert self.test_cmd is not None
        cmd_result = CmdResult.create(StatusCode.OK, self.test_cmd)

        detail = {"id": 1, "name": "a"}
        cmd_result.set_property_string("detail", json.dumps(detail))

        await ten_env.return_result(cmd_result)
