#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    CmdResult,
    StatusCode,
    TriggerLifeCycleCmd,
)
from ten_runtime.loc import Loc


class MainExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("MainExtension on_init")

        await asyncio.sleep(1.0)

        # Send cmd to biz extension to check if it is started.
        cmd = Cmd.create("check_start")

        result, _ = await ten_env.send_cmd(cmd)
        assert result is not None

        started, _ = result.get_property_bool("started")
        assert (
            not started
        ), "biz extension should not be started as it has been set manual trigger life cycle"

        # Send manual trigger life cycle cmd to biz extension.
        life_cycle_cmd = TriggerLifeCycleCmd.create()
        life_cycle_cmd.set_stage("start")
        life_cycle_cmd.set_dests([Loc("", "", "biz")])

        trigger_life_cycle_result, _ = await ten_env.send_cmd(life_cycle_cmd)
        assert (
            trigger_life_cycle_result is not None
        ), "trigger_life_cycle_result is None"
        assert (
            trigger_life_cycle_result.get_status_code() == StatusCode.OK
        ), "trigger_life_cycle_result status code is not OK"

        # Send cmd to biz extension to check if it is started.
        check_start_cmd = Cmd.create("check_start")

        check_start_result, _ = await ten_env.send_cmd(check_start_cmd)
        assert check_start_result is not None, "check_start_result is None"
        assert (
            check_start_result.get_status_code() == StatusCode.OK
        ), "check_start_result status code is not OK"

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("MainExtension on_stop")

        await asyncio.sleep(1.0)

        # Send cmd to biz extension to check if it is stopped.
        check_stop_cmd = Cmd.create("check_stop")

        check_stop_result, _ = await ten_env.send_cmd(check_stop_cmd)
        assert check_stop_result is not None

        stopped, _ = check_stop_result.get_property_bool("stopped")
        assert (
            not stopped
        ), "biz extension should not be stopped as it has been set manual trigger life cycle"

        # Send manual trigger life cycle cmd to biz extension.
        life_cycle_cmd = TriggerLifeCycleCmd.create()
        life_cycle_cmd.set_stage("stop")
        life_cycle_cmd.set_dests([Loc("", "", "biz")])

        trigger_life_cycle_result, _ = await ten_env.send_cmd(life_cycle_cmd)
        assert trigger_life_cycle_result is not None
        assert (
            trigger_life_cycle_result.get_status_code() == StatusCode.OK
        ), "trigger_life_cycle_result status code is not OK"

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("MainExtension on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"cmdName: {cmd_name}")

        if cmd_name == "test":
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            cmd_result.set_property_string("detail", "ok")
            await ten_env.return_result(cmd_result)
        else:
            cmd_result = CmdResult.create(StatusCode.ERROR, cmd)
            cmd_result.set_property_string("detail", "unknown command")
            await ten_env.return_result(cmd_result)


class BizExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.started: bool = False
        self.stopped: bool = False

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        self.started = True
        ten_env.log_info("BizExtension on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        self.stopped = True
        ten_env.log_info("BizExtension on_stop")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"cmdName: {cmd_name}")

        if cmd_name == "check_start":
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            cmd_result.set_property_bool("started", self.started)
            await ten_env.return_result(cmd_result)
        elif cmd_name == "check_stop":
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            cmd_result.set_property_bool("stopped", self.stopped)
            await ten_env.return_result(cmd_result)
        else:
            cmd_result = CmdResult.create(StatusCode.ERROR, cmd)
            cmd_result.set_property_string("detail", "unknown command")
            await ten_env.return_result(cmd_result)
