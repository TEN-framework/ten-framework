#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

import json
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
    StartGraphCmd,
    StopGraphCmd,
)


class TestExtension1(Extension):
    start_and_stop_graph_is_completed: bool
    received_data_from_new_graph: bool
    test_cmd: Cmd | None

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.start_and_stop_graph_is_completed = False
        self.received_data_from_new_graph = False
        self.test_cmd = None

    def on_start(self, ten_env: TenEnv) -> None:
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
        }

        start_graph_cmd.set_graph_from_json(json.dumps(graph_json))

        def start_graph_callback(
            ten_env: TenEnv,
            cmd_result: CmdResult | None,
            error: TenError | None,
        ):
            if error is not None:
                ten_env.log(LogLevel.ERROR, f"Start graph failed: {error}")
                return

            if cmd_result is None:
                ten_env.log(LogLevel.ERROR, "Start graph cmd_result is None")
                return

            status_code = cmd_result.get_status_code()
            if status_code != StatusCode.OK:
                ten_env.log(
                    LogLevel.ERROR,
                    f"Start graph failed with status: {status_code}",
                )
                return

            # Get the graph ID of the newly created graph.
            new_graph_id, _ = cmd_result.get_property_string("graph_id")
            print(f"new_graph_id: {new_graph_id}")

            # Send a 'set_original_graph_info' command to the specified
            # extension in the newly created graph.
            cmd_set_original_graph_info = Cmd.create("set_original_graph_info")

            # Set the original graph receiver extension.
            cmd_set_original_graph_info.set_property_string(
                "original_graph_receiver_extension", "test_extension_1"
            )

            # This is important. Specify the destination of the
            # 'set_original_graph_info' command to the specified extension
            # in the newly created graph.
            cmd_set_original_graph_info.set_dests(
                [Loc("", new_graph_id, "test_extension_2")]
            )

            def set_original_graph_info_callback(
                ten_env: TenEnv,
                _cmd_result: CmdResult | None,
                _error: TenError | None,
            ):
                # Send start command to test_extension_2
                cmd_start = Cmd.create("start")
                cmd_start.set_dests([Loc("", new_graph_id, "test_extension_2")])

                def start_callback(
                    ten_env: TenEnv,
                    _cmd_result: CmdResult | None,
                    _error: TenError | None,
                ):
                    # Shut down the graph; otherwise, the app won't be able
                    # to close because there is still a running engine/graph.
                    print(f"stop_graph_cmd: {new_graph_id}")

                    stop_graph_cmd = StopGraphCmd.create()
                    stop_graph_cmd.set_dests([Loc("")])
                    stop_graph_cmd.set_graph_id(new_graph_id)

                    def stop_graph_callback(
                        ten_env: TenEnv,
                        _cmd_result: CmdResult | None,
                        _error: TenError | None,
                    ):
                        self.start_and_stop_graph_is_completed = True

                        if (
                            self.test_cmd is not None
                            and self.received_data_from_new_graph
                        ):
                            self._reply_to_client(ten_env)

                    ten_env.send_cmd(stop_graph_cmd, stop_graph_callback)

                ten_env.send_cmd(cmd_start, start_callback)

            ten_env.send_cmd(
                cmd_set_original_graph_info, set_original_graph_info_callback
            )

        ten_env.send_cmd(start_graph_cmd, start_graph_callback)
        ten_env.on_start_done()

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "test":
            print("received test_cmd")
            self.test_cmd = cmd

            if (
                self.start_and_stop_graph_is_completed
                and self.received_data_from_new_graph
            ):
                # Send the response to the client.
                self._reply_to_client(ten_env)
        else:
            ten_env.log(
                LogLevel.ERROR,
                f"Should not happen - unknown command: {cmd_name}",
            )

    def on_data(self, ten_env: TenEnv, data: Data) -> None:
        data_name = data.get_name()

        if data_name == "data_from_new_graph":
            self.received_data_from_new_graph = True

            if (
                self.test_cmd is not None
                and self.start_and_stop_graph_is_completed
            ):
                self._reply_to_client(ten_env)
        else:
            ten_env.log(
                LogLevel.ERROR, f"Should not happen - unknown data: {data_name}"
            )

    def _reply_to_client(self, ten_env: TenEnv) -> None:
        print("reply to client")

        assert self.test_cmd is not None
        cmd_result = CmdResult.create(StatusCode.OK, self.test_cmd)

        detail = {"id": 1, "name": "a"}
        cmd_result.set_property_string("detail", json.dumps(detail))

        ten_env.return_result(cmd_result)
