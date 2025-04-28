#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from ten_runtime import (
    Cmd,
    Data,
    AudioFrame,
    VideoFrame,
    StatusCode,
    AsyncExtensionTester,
    AsyncTenEnvTester,
)


class AsyncExtensionTesterBasic(AsyncExtensionTester):
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        await ten_env.send_data(Data.create("test"))
        await ten_env.send_audio_frame(AudioFrame.create("test"))
        await ten_env.send_video_frame(VideoFrame.create("test"))

        new_cmd = Cmd.create("hello_world")

        ten_env.log_info("send hello_world")
        result, error = await ten_env.send_cmd(
            new_cmd,
        )
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        ten_env.log_info("receive hello_world, status:" + str(statusCode))

        ten_env.log_info("tester on_start_done")

        if statusCode == StatusCode.OK:
            ten_env.stop_test()

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info("tester on_stop")


def test_basic():
    tester = AsyncExtensionTesterBasic()
    tester.set_test_mode_single("default_extension_python")
    tester.run()


if __name__ == "__main__":
    test_basic()
