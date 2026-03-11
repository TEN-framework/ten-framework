#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
# test_extension_1: orchestrator living in the predefined "default" graph.
#
# Flow:
#   1. on_start: launch a dynamic graph with sync_stop_before_deinit=True
#      (contains test_extension_2 and test_extension_3), then immediately
#      send stop_graph to trigger the on_stop phase.
#   2. During the on_start await for stop_graph, the asyncio event loop can
#      concurrently dispatch on_cmd("on_stop_notify") coming from
#      test_extension_2 (which sleeps 2s then sends the notification).
#   3. We reply to the result of on_stop_notify so test_extension_2 can call
#      on_stop_done.
#   4. Once stop_graph completes AND the notification is received, we reply to
#      the client.  The client assertion proves the message was delivered while
#      the dynamic graph was still in its stop phase.
#

import json
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Loc,
    StartGraphCmd,
    StopGraphCmd,
)


class TestExtension1(AsyncExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self._test_cmd: Cmd | None = None
        self._stop_graph_done: bool = False
        self._got_on_stop_notify: bool = False

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        # Build the dynamic graph with sync_stop_before_deinit enabled.
        start_graph_cmd = StartGraphCmd.create()
        start_graph_cmd.set_dests([Loc("")])
        start_graph_cmd.set_sync_stop_before_deinit(True)

        # extension_2 (slow stopper) exposes on_stop_notify as cmd_out so the
        # message can cross the graph boundary and reach test_extension_1.
        graph_json = {
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_extension_2",
                    "addon": "test_extension_2",
                    "extension_group": "group_2",
                },
                {
                    "type": "extension",
                    "name": "test_extension_3",
                    "addon": "test_extension_3",
                    "extension_group": "group_3",
                },
            ],
            "exposed_messages": [
                {
                    "type": "cmd_out",
                    "name": "on_stop_notify",
                    "extension": "test_extension_2",
                }
            ],
        }
        start_graph_cmd.set_graph_from_json(json.dumps(graph_json))

        cmd_result, error = await ten_env.send_cmd(start_graph_cmd)
        if error is not None:
            ten_env.log_error(f"start_graph failed: {error}")
            return
        if cmd_result is None:
            ten_env.log_error("start_graph cmd_result is None")
            return

        graph_id, _ = cmd_result.get_property_string("graph_id")
        ten_env.log_info(f"Dynamic graph started, id={graph_id}")

        # Immediately stop the dynamic graph to trigger on_stop for both
        # extensions.  While we await this, the asyncio event loop can
        # concurrently run on_cmd("on_stop_notify").
        stop_graph_cmd = StopGraphCmd.create()
        stop_graph_cmd.set_dests([Loc("")])
        stop_graph_cmd.set_graph_id(graph_id)

        _, error = await ten_env.send_cmd(stop_graph_cmd)
        if error is not None:
            ten_env.log_error(f"stop_graph failed: {error}")
            return

        ten_env.log_info("stop_graph done")
        self._stop_graph_done = True
        await self._try_reply(ten_env)

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "test":
            self._test_cmd = cmd
            await self._try_reply(ten_env)

        elif cmd_name == "on_stop_notify":
            ten_env.log_info("Received on_stop_notify from dynamic graph")
            self._got_on_stop_notify = True

            # Must reply so test_extension_2's send_cmd callback fires and it
            # can call on_stop_done.
            await ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

            await self._try_reply(ten_env)

        else:
            ten_env.log_error(f"Unexpected cmd: {cmd_name}")

    async def _try_reply(self, ten_env: AsyncTenEnv) -> None:
        if (
            self._test_cmd is None
            or not self._stop_graph_done
            or not self._got_on_stop_notify
        ):
            return

        ten_env.log_info("All conditions met, replying to client")
        cmd_result = CmdResult.create(StatusCode.OK, self._test_cmd)
        cmd_result.set_property_string(
            "detail", json.dumps({"id": 1, "name": "a"})
        )
        await ten_env.return_result(cmd_result)
        self._test_cmd = None
