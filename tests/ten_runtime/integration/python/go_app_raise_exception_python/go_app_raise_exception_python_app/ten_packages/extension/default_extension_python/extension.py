#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

# import debugpy
# debugpy.listen(5678)
# debugpy.wait_for_client()

import asyncio
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    LogLevel,
)


class DefaultExtension(AsyncExtension):
    async def on_configure(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_init")

        await ten_env.init_property_from_json('{"testKey": "testValue"}')

        await asyncio.sleep(0.5)

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_start")

        await asyncio.sleep(0.5)

        await ten_env.set_property_from_json("testKey2", '"testValue2"')
        testValue, _ = await ten_env.get_property_to_json("testKey")
        testValue2, _ = await ten_env.get_property_to_json("testKey2")
        ten_env.log(
            LogLevel.INFO, f"testValue: {testValue}, testValue2: {testValue2}"
        )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_stop")

        await asyncio.sleep(0.5)

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_deinit")

        await asyncio.sleep(0.5)

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log(LogLevel.DEBUG, "on_cmd: " + cmd_json)

        # Raise CancelledError (inherits from BaseException, not Exception)
        raise asyncio.CancelledError("Test CancelledError for BaseException catch verification")
