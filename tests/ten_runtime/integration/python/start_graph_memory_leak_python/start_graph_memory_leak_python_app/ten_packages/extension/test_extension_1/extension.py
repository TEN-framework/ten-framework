#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

import asyncio
import gc
import json
import sys
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    LogLevel,
    Loc,
    StartGraphCmd,
    StopGraphCmd,
)

# Number of start/stop graph cycles to run.
CYCLE_COUNT = 50

# Memory growth threshold in kilobytes. If RSS grows more than this amount
# across all cycles, the test is considered a failure, indicating a memory leak.
MEMORY_GROWTH_THRESHOLD_KB = 50 * 1024  # 50 MB


def _get_rss_kb() -> int:
    """Returns the current Resident Set Size (RSS) of this process in KB.

    On Linux, reads from /proc/self/status for accuracy.
    On other platforms, falls back to the resource module.
    """
    if sys.platform == "linux":
        try:
            with open("/proc/self/status", "r") as f:
                for line in f:
                    if line.startswith("VmRSS:"):
                        return int(line.split()[1])
        except OSError:
            pass

    try:
        import resource

        # On Linux ru_maxrss is in KB; on macOS it's in bytes.
        rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        if sys.platform == "darwin":
            rss = rss // 1024
        return rss
    except ImportError:
        return 0


class TestExtension1(AsyncExtension):
    """Main extension that orchestrates the memory leak test.

    On receiving a 'test' command from the client, this extension:
    1. Records initial memory usage (RSS).
    2. Runs CYCLE_COUNT iterations of: start subgraph -> stop subgraph.
    3. Records final memory usage.
    4. Returns OK if memory growth is within threshold, ERROR otherwise.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.test_cmd: Cmd | None = None

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "test":
            self.test_cmd = cmd
            await self._run_memory_leak_test(ten_env)
        else:
            ten_env.log(
                LogLevel.ERROR,
                f"Unexpected command received: {cmd_name}",
            )

    async def _run_memory_leak_test(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info(
            f"Starting memory leak test: {CYCLE_COUNT} start/stop graph cycles."
        )

        # The graph JSON for the subgraph containing test_extension_2.
        graph_json = {
            "nodes": [
                {
                    "type": "extension",
                    "name": "test_extension_2",
                    "addon": "test_extension_2",
                    "extension_group": "test_extension_2_group",
                }
            ]
        }

        # Run a few warm-up cycles to let Python's memory allocator stabilize
        # before taking the baseline measurement.
        WARMUP_CYCLES = 5
        ten_env.log_info(f"Running {WARMUP_CYCLES} warm-up cycles...")
        for _ in range(WARMUP_CYCLES):
            graph_id = await self._start_subgraph(ten_env, graph_json)
            if graph_id:
                await self._stop_subgraph(ten_env, graph_id)
            await asyncio.sleep(0.01)
        gc.collect()
        await asyncio.sleep(0.05)

        initial_rss_kb = _get_rss_kb()
        ten_env.log_info(f"Initial RSS (post warm-up): {initial_rss_kb} KB")

        for i in range(CYCLE_COUNT):
            graph_id = await self._start_subgraph(ten_env, graph_json)
            if graph_id is None:
                ten_env.log(
                    LogLevel.ERROR,
                    f"Cycle {i + 1}/{CYCLE_COUNT}: Failed to start subgraph.",
                )
                await self._return_error(ten_env, "Failed to start subgraph.")
                return

            success = await self._stop_subgraph(ten_env, graph_id)
            if not success:
                ten_env.log(
                    LogLevel.ERROR,
                    f"Cycle {i + 1}/{CYCLE_COUNT}: Failed to stop subgraph "
                    f"(graph_id={graph_id}).",
                )
                await self._return_error(ten_env, "Failed to stop subgraph.")
                return

            # Allow the runtime to settle and trigger Python GC periodically.
            await asyncio.sleep(0.01)
            if (i + 1) % 10 == 0:
                gc.collect()
                await asyncio.sleep(0.05)
                current_rss_kb = _get_rss_kb()
                ten_env.log_info(
                    f"After cycle {i + 1}/{CYCLE_COUNT}: RSS={current_rss_kb} KB "
                    f"(delta={current_rss_kb - initial_rss_kb} KB)"
                )

        final_rss_kb = _get_rss_kb()
        memory_growth_kb = final_rss_kb - initial_rss_kb

        ten_env.log_info(
            f"Memory leak test completed. "
            f"Initial RSS: {initial_rss_kb} KB, "
            f"Final RSS: {final_rss_kb} KB, "
            f"Growth: {memory_growth_kb} KB "
            f"(threshold: {MEMORY_GROWTH_THRESHOLD_KB} KB)."
        )

        if memory_growth_kb > MEMORY_GROWTH_THRESHOLD_KB:
            msg = (
                f"Potential memory leak detected! "
                f"RSS grew by {memory_growth_kb} KB over {CYCLE_COUNT} cycles "
                f"(threshold: {MEMORY_GROWTH_THRESHOLD_KB} KB)."
            )
            ten_env.log(LogLevel.ERROR, msg)
            await self._return_error(ten_env, msg)
        else:
            ten_env.log_info("Memory usage is within acceptable bounds. Test passed.")
            assert self.test_cmd is not None
            result = CmdResult.create(StatusCode.OK, self.test_cmd)
            result.set_property_string(
                "detail",
                json.dumps(
                    {
                        "cycles": CYCLE_COUNT,
                        "initial_rss_kb": initial_rss_kb,
                        "final_rss_kb": final_rss_kb,
                        "memory_growth_kb": memory_growth_kb,
                        "threshold_kb": MEMORY_GROWTH_THRESHOLD_KB,
                    }
                ),
            )
            await ten_env.return_result(result)

    async def _start_subgraph(
        self, ten_env: AsyncTenEnv, graph_json: dict
    ) -> str | None:
        """Starts a new subgraph and returns its graph_id, or None on failure."""
        start_cmd = StartGraphCmd.create()
        start_cmd.set_dests([Loc("")])
        start_cmd.set_graph_from_json(json.dumps(graph_json))

        cmd_result, error = await ten_env.send_cmd(start_cmd)
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"StartGraphCmd failed: {error}")
            return None

        if cmd_result is None:
            ten_env.log(LogLevel.ERROR, "StartGraphCmd returned None result.")
            return None

        graph_id, err = cmd_result.get_property_string("graph_id")
        if err is not None:
            ten_env.log(LogLevel.ERROR, f"Failed to get graph_id: {err}")
            return None

        return graph_id

    async def _stop_subgraph(
        self, ten_env: AsyncTenEnv, graph_id: str
    ) -> bool:
        """Stops a subgraph by graph_id. Returns True on success."""
        stop_cmd = StopGraphCmd.create()
        stop_cmd.set_dests([Loc("")])
        stop_cmd.set_graph_id(graph_id)

        cmd_result, error = await ten_env.send_cmd(stop_cmd)
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"StopGraphCmd failed: {error}")
            return False

        if cmd_result is None:
            ten_env.log(LogLevel.ERROR, "StopGraphCmd returned None result.")
            return False

        return True

    async def _return_error(self, ten_env: AsyncTenEnv, msg: str) -> None:
        assert self.test_cmd is not None
        result = CmdResult.create(StatusCode.ERROR, self.test_cmd)
        result.set_property_string("detail", msg)
        await ten_env.return_result(result)
