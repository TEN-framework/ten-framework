#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import time
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


class DefaultAsyncExtension(AsyncExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)

        self.send_goodbye_cmd = None
        self.sleep_ms_before_goodbye = None
        self.assert_goodbye_result_success = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")
        self.send_goodbye_cmd, err = await ten_env.get_property_bool(
            "send_goodbye_cmd"
        )
        if err is not None:
            ten_env.log_error(
                "Could not read 'send_goodbye_cmd' from properties." + str(err)
            )
            self.send_goodbye_cmd = False

        self.sleep_ms_before_goodbye, err = await ten_env.get_property_int(
            "sleep_ms_before_goodbye"
        )
        if err is not None:
            ten_env.log_error(
                "Could not read 'sleep_ms_before_goodbye' from properties."
                + str(err)
            )
            self.sleep_ms_before_goodbye = 0

        self.assert_goodbye_result_success, err = (
            await ten_env.get_property_bool("assert_goodbye_result_success")
        )
        if err is not None:
            ten_env.log_error(
                (
                    "Could not read 'assert_goodbye_result_success' from "
                    "properties." + str(err)
                )
            )
            self.assert_goodbye_result_success = False

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        if cmd_name == "query_weather":
            # Send a command to query weather.
            query_cmd = Cmd.create("query_weather")
            cmd_result, error = await ten_env.send_cmd(query_cmd)
            assert error is None
            assert cmd_result is not None

            # Get the weather detail.
            weather, _ = cmd_result.get_property_string("detail")

            # Return the weather detail.
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            cmd_result.set_property_string("detail", weather)
            await ten_env.return_result(cmd_result)
        elif cmd_name == "flush":
            # Bypass the flush command. The purpose of bypassing this `flush`
            # command here is to allow the extension tester to call `stop_test`.
            await ten_env.send_cmd(Cmd.create("flush"))

            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug("on_data name {}".format(data_name))

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        audio_frame_name = audio_frame.get_name()
        ten_env.log_debug("on_audio_frame name {}".format(audio_frame_name))

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        video_frame_name = video_frame.get_name()
        ten_env.log_debug("on_video_frame name {}".format(video_frame_name))

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.send_goodbye_cmd:
            assert self.sleep_ms_before_goodbye is not None
            if self.sleep_ms_before_goodbye > 0:
                await asyncio.sleep(self.sleep_ms_before_goodbye / 1000)

            current_time = time.time()

            cmd = Cmd.create("goodbye")
            result, error = await ten_env.send_cmd(cmd)

            if self.assert_goodbye_result_success:
                assert error is None
                assert result is not None
                assert result.get_status_code() == StatusCode.OK

            cost_time = time.time() - current_time
            ten_env.log_info("goodbye cost time {} ms".format(cost_time * 1000))

            # To rule out that the result reply was triggered by path_timeout,
            # we need to ensure cost_time is less than 1 second.
            assert cost_time < 1
