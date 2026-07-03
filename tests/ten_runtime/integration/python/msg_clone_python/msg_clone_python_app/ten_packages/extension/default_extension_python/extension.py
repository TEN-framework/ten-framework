#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
    VideoFrame,
    AudioFrame,
    CmdResult,
    StatusCode,
)


class ServerExtension(AsyncExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        if cmd.get_name() == "test":
            result = CmdResult.create(StatusCode.OK, cmd)
            result.set_property_string("detail", "ok")
            await ten_env.return_result(result)
        else:
            assert False

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        assert data.get_name() == "test"
        cloned_data = data.clone()
        await ten_env.send_data(cloned_data)

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        assert video_frame.get_name() == "test"
        cloned_video_frame = video_frame.clone()
        await ten_env.send_video_frame(cloned_video_frame)

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        assert audio_frame.get_name() == "test"
        cloned_audio_frame = audio_frame.clone()
        await ten_env.send_audio_frame(cloned_audio_frame)


class ClientExtension(AsyncExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.name = name
        self.test_cmd = None
        self.msg_count = 0

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        if cmd.get_name() == "test":
            self.test_cmd = cmd

            await ten_env.send_data(Data.create("test"))
            await ten_env.send_video_frame(VideoFrame.create("test"))
            await ten_env.send_audio_frame(AudioFrame.create("test"))

            cloned_cmd = cmd.clone()
            result, _ = await ten_env.send_cmd(cloned_cmd)
            assert result is not None
            assert result.get_status_code() == StatusCode.OK

            self.msg_count += 1
            await self.return_if_all_done(ten_env)
        else:
            assert False

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        assert data.get_name() == "test"
        self.msg_count += 1
        await self.return_if_all_done(ten_env)

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        assert video_frame.get_name() == "test"
        self.msg_count += 1
        await self.return_if_all_done(ten_env)

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        assert audio_frame.get_name() == "test"
        self.msg_count += 1
        await self.return_if_all_done(ten_env)

    async def return_if_all_done(self, ten_env: AsyncTenEnv) -> None:
        if self.test_cmd is None:
            return

        if self.msg_count == 4:
            result = CmdResult.create(StatusCode.OK, self.test_cmd)
            result.set_property_string("detail", "ok")
            await ten_env.return_result(result)
