#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_runtime import (
    AudioFrame,
    VideoFrame,
    Extension,
    TenEnv,
    Cmd,
    LogLevel,
    StatusCode,
    CmdResult,
    Data,
)


class DefaultExtension(Extension):
    def on_init(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_init")
        ten_env.on_init_done()

    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_start")

        # IMPLEMENT: read properties, initialize resources

        ten_env.on_start_done()

    def on_stop(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_stop")

        # IMPLEMENT: clean up resources

        ten_env.on_stop_done()

    def on_deinit(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_deinit")
        ten_env.on_deinit_done()

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log(LogLevel.DEBUG, "on_cmd name {}".format(cmd_name))

        # IMPLEMENT: process cmd

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        ten_env.return_result(cmd_result)

    def on_data(self, ten_env: TenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log(LogLevel.DEBUG, "on_data name {}".format(data_name))

        # IMPLEMENT: process data

    def on_audio_frame(self, ten_env: TenEnv, audio_frame: AudioFrame) -> None:
        audio_frame_name = audio_frame.get_name()
        ten_env.log(
            LogLevel.DEBUG, "on_audio_frame name {}".format(audio_frame_name)
        )

        # IMPLEMENT: process audio frame

    def on_video_frame(self, ten_env: TenEnv, video_frame: VideoFrame) -> None:
        video_frame_name = video_frame.get_name()
        ten_env.log(
            LogLevel.DEBUG, "on_video_frame name {}".format(video_frame_name)
        )

        # IMPLEMENT: process video frame
