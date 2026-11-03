#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_runtime import (  # pylint: disable=import-error
    AsyncExtension,
    AsyncTenEnv,
    AudioFrame,
    Cmd,
    CmdResult,
    Data,
    StatusCode,
)


class SpatialRealAvatarExtension(AsyncExtension):
    """TEN glue for the SpatialReal avatar.

    Scaffold only: every hook logs at debug level and ``on_cmd`` answers OK.
    Config loading, dispatch and the session wiring land in later tasks.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv | None = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")
        self.ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug(f"on_cmd name={cmd_name}")
        await ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        ten_env.log_debug(f"on_data name={data.get_name()}")

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        ten_env.log_debug(f"on_audio_frame name={audio_frame.get_name()}")
