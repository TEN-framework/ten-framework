#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import Optional
from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    Data,
    AudioFrame,
    VideoFrame,
    CmdResult,
    StatusCode,
    TenError,
)


class ExtensionTesterBasic(ExtensionTester):
    def check_hello(
        self,
        ten_env: TenEnvTester,
        result: Optional[CmdResult],
        error: Optional[TenError],
    ):
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        ten_env.log_info("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            ten_env.stop_test()

    def on_start(self, ten_env: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        ten_env.log_info("send hello_world")
        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error
            ),
        )

        ten_env.send_data(Data.create("test"))
        ten_env.send_audio_frame(AudioFrame.create("test"))
        ten_env.send_video_frame(VideoFrame.create("test"))

        ten_env.log_info("tester on_start_done")
        ten_env.on_start_done()

    def on_stop(self, ten_env: TenEnvTester) -> None:
        ten_env.log_info("tester on_stop")
        ten_env.on_stop_done()


def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("default_extension_python")
    tester.run()


if __name__ == "__main__":
    test_basic()
