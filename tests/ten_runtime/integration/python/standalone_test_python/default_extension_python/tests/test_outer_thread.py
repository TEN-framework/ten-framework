#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import threading
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


class ExtensionTesterOuterThread(ExtensionTester):
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

    def send_msgs(self, ten_env: TenEnvTester):
        new_cmd = Cmd.create("hello_world")
        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error
            ),
        )

        ten_env.send_data(Data.create("test"))
        ten_env.send_audio_frame(AudioFrame.create("test"))
        ten_env.send_video_frame(VideoFrame.create("test"))

    def on_start(self, ten_env: TenEnvTester) -> None:
        self.thread = threading.Thread(target=self.send_msgs, args=(ten_env,))
        self.thread.start()

        ten_env.log_info("tester on_start_done")
        ten_env.on_start_done()


def test_basic():
    tester = ExtensionTesterOuterThread()
    tester.set_test_mode_single("default_extension_python")
    tester.run()


if __name__ == "__main__":
    test_basic()
