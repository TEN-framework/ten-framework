#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
# test_extension_2: slow stopper in the dynamic graph.
#
# In on_stop it sleeps for 2 seconds via asyncio.sleep (non-blocking), then
# sends an "on_stop_notify" command to the outside world (test_extension_1).
# Returning from on_stop automatically calls on_stop_done (AsyncExtension).
#
# With sync_stop_before_deinit enabled, test_extension_3 (the fast stopper)
# cannot proceed to on_deinit until this extension also calls on_stop_done —
# which only happens after the 2-second sleep and the on_stop_notify
# round-trip.
#

import asyncio
from ten_runtime import AsyncExtension, AsyncTenEnv, Cmd, StatusCode, CmdResult


class TestExtension2(AsyncExtension):
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_stop: sleeping 2s before sending on_stop_notify")

        # Non-blocking sleep: yields control to the asyncio event loop so that
        # other coroutines (e.g. test_extension_1's on_cmd) can run concurrently.
        await asyncio.sleep(2)

        ten_env.log_info("on_stop: sending on_stop_notify after 2s sleep")

        notify_cmd = Cmd.create("on_stop_notify")
        cmd_result, error = await ten_env.send_cmd(notify_cmd)

        if error is not None:
            ten_env.log_error(f"on_stop_notify failed: {error}")
        else:
            ten_env.log_info("on_stop_notify ack received")

        # Returning from on_stop triggers on_stop_done automatically
        # (AsyncExtension framework behaviour).
