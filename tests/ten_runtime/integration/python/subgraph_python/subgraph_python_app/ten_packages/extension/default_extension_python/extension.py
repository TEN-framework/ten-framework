#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Data,
    Cmd,
    LogLevel,
    CmdResult,
    StatusCode,
)


class FunctionEntryExtension(AsyncExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.received_cmd = None

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log(
            LogLevel.INFO, f"function entry extension received cmd: {cmd_name}"
        )

        base, error = await ten_env.get_property_int("base")
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"failed to get base: {error}")
            await ten_env.return_result(CmdResult.create(StatusCode.ERROR, cmd))
            return

        self.received_cmd = cmd

        data = Data.create("data")
        data.set_property_int("data", base)
        await ten_env.send_data(data)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        number, error = data.get_property_int("data")
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"failed to get data: {error}")
            return

        if self.received_cmd is None:
            ten_env.log(LogLevel.ERROR, "received data but no cmd received")
            return

        cmd_result = CmdResult.create(StatusCode.OK, self.received_cmd)
        cmd_result.set_property_int("detail", number)
        await ten_env.return_result(cmd_result)


class PowerExtension(AsyncExtension):
    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        number, error = data.get_property_int("data")
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"failed to get data: {error}")
            return

        power, error = await ten_env.get_property_int("power")
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"failed to get power: {error}")
            return

        result = number**power

        data.set_property_int("data", result)
        await ten_env.send_data(data)


class MultiExtension(AsyncExtension):
    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        number, error = data.get_property_int("data")
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"failed to get data: {error}")
            return

        multi, error = await ten_env.get_property_int("multi")
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"failed to get multi: {error}")
            return

        result = number * multi
        data.set_property_int("data", result)
        await ten_env.send_data(data)


class SubstractExtension(AsyncExtension):
    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        number, error = data.get_property_int("data")
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"failed to get data: {error}")
            return

        substract, error = await ten_env.get_property_int("substract")
        if error is not None:
            ten_env.log(LogLevel.ERROR, f"failed to get substract: {error}")
            return

        result = number - substract
        data.set_property_int("data", result)
        await ten_env.send_data(data)
