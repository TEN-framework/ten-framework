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
    StatusCode,
    CmdResult,
    Data,
)


class DefaultExtension(Extension):
    def on_init(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_init")
        ten_env.on_init_done()

    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_start")

        # TODO: read properties, initialize resources

        ten_env.on_start_done()

    def on_stop(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_stop")

        # TODO: clean up resources

        ten_env.on_stop_done()

    def on_deinit(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_deinit")
        ten_env.on_deinit_done()

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        # TODO: process cmd

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        ten_env.return_result(cmd_result)

    def on_data(self, ten_env: TenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug("on_data name {}".format(data_name))

        # TODO: process data
        pass

    def on_audio_frame(self, ten_env: TenEnv, audio_frame: AudioFrame) -> None:
        audio_frame_name = audio_frame.get_name()
        ten_env.log_debug("on_audio_frame name {}".format(audio_frame_name))

        # TODO: process audio frame
        pass

    def on_video_frame(self, ten_env: TenEnv, video_frame: VideoFrame) -> None:
        video_frame_name = video_frame.get_name()
        ten_env.log_debug("on_video_frame name {}".format(video_frame_name))

        # TODO: process video frame
        pass
