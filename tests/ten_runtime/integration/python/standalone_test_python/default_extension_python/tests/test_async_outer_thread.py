#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Cmd,
    Data,
    AudioFrame,
    VideoFrame,
    StatusCode,
)


class AsyncExtensionTesterOuterThread(AsyncExtensionTester):
    async def send_msgs(self, ten_env: AsyncTenEnvTester):
        await ten_env.send_data(Data.create("test"))
        await ten_env.send_audio_frame(AudioFrame.create("test"))
        await ten_env.send_video_frame(VideoFrame.create("test"))

        new_cmd = Cmd.create("hello_world")
        result, error = await ten_env.send_cmd(
            new_cmd,
        )
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        ten_env.log_info("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            ten_env.stop_test()

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        asyncio.create_task(self.send_msgs(ten_env))

        ten_env.log_info("tester on_start_done")


def test_basic():
    tester = AsyncExtensionTesterOuterThread()
    tester.set_test_mode_single("default_extension_python")
    tester.run()


if __name__ == "__main__":
    test_basic()
