#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_runtime import (
    AudioFrame,
    VideoFrame,
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)


class SimpleEchoExtension(AsyncExtension):
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

        # TODO: read properties, initialize resources

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")

        # TODO: clean up resources

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        cmd_result.set_property_string("detail", cmd_name + ", too")

        await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug("on_data name {}".format(data_name))

        buf = data.get_buf()

        new_data = Data.create(data_name)
        new_data.alloc_buf(len(buf))
        new_buf = new_data.lock_buf()
        new_buf[:] = buf
        new_data.unlock_buf(new_buf)

        await ten_env.send_data(new_data)

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        audio_frame_name = audio_frame.get_name()
        ten_env.log_debug("on_audio_frame name {}".format(audio_frame_name))

        buf = audio_frame.get_buf()
        new_audio_frame = AudioFrame.create(audio_frame_name)
        new_audio_frame.alloc_buf(len(buf))
        new_buf = new_audio_frame.lock_buf()
        new_buf[:] = buf
        new_audio_frame.unlock_buf(new_buf)

        new_audio_frame.set_bytes_per_sample(audio_frame.get_bytes_per_sample())
        new_audio_frame.set_sample_rate(audio_frame.get_sample_rate())
        new_audio_frame.set_data_fmt(audio_frame.get_data_fmt())
        new_audio_frame.set_eof(audio_frame.is_eof())
        new_audio_frame.set_line_size(audio_frame.get_line_size())
        new_audio_frame.set_number_of_channels(
            audio_frame.get_number_of_channels()
        )
        new_audio_frame.set_timestamp(audio_frame.get_timestamp())

        await ten_env.send_audio_frame(new_audio_frame)

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        video_frame_name = video_frame.get_name()
        ten_env.log_debug("on_video_frame name {}".format(video_frame_name))

        buf = video_frame.get_buf()
        new_video_frame = VideoFrame.create(video_frame_name)
        new_video_frame.alloc_buf(len(buf))
        new_buf = new_video_frame.lock_buf()
        new_buf[:] = buf
        new_video_frame.unlock_buf(new_buf)

        new_video_frame.set_eof(video_frame.is_eof())
        new_video_frame.set_height(video_frame.get_height())
        new_video_frame.set_width(video_frame.get_width())
        new_video_frame.set_timestamp(video_frame.get_timestamp())
        new_video_frame.set_pixel_fmt(video_frame.get_pixel_fmt())

        await ten_env.send_video_frame(new_video_frame)
