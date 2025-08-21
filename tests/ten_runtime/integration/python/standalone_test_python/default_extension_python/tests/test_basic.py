#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import time
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
    LogLevel,
    TenErrorCode
)


class ExtensionTesterBasic(ExtensionTester):
    def check_hello(
        self,
        ten_env: TenEnvTester,
        result: CmdResult | None,
        error: TenError | None,
    ):
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        ten_env.log(
            LogLevel.INFO, "receive hello_world, status:" + str(statusCode)
        )

        if statusCode == StatusCode.OK:
            ten_env.stop_test()

    def on_start(self, ten_env: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        ten_env.log(LogLevel.INFO, "send hello_world")
        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error
            ),
        )

        ten_env.send_data(Data.create("test"))
        ten_env.send_audio_frame(AudioFrame.create("test"))
        ten_env.send_video_frame(VideoFrame.create("test"))

        ten_env.log(LogLevel.INFO, "tester on_start_done")
        ten_env.on_start_done()

    def on_stop(self, ten_env: TenEnvTester) -> None:
        ten_env.log(LogLevel.INFO, "tester on_stop")
        ten_env.on_stop_done()


class ExtensionTesterFail(ExtensionTester):
    def check_hello(
        self,
        ten_env: TenEnvTester,
        result: CmdResult | None,
        error: TenError | None,
    ):
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        if statusCode == StatusCode.ERROR:
            test_result = TenError.create(
                TenErrorCode.ErrorCodeGeneric, "error response"
            )
            ten_env.stop_test(test_result)

    def on_start(self, ten_env: TenEnvTester) -> None:
        unknown_cmd = Cmd.create("unknown_cmd")
        ten_env.send_cmd(
            unknown_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error
            ),
        )

        ten_env.on_start_done()


class ExtensionTesterFail2(ExtensionTester):
    def check_hello(
        self,
        ten_env: TenEnvTester,
        result: CmdResult | None,
        error: TenError | None,
    ):
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        if statusCode == StatusCode.ERROR:
            test_result = TenError.create(TenErrorCode.ErrorCodeGeneric)
            ten_env.stop_test(test_result)

    def on_start(self, ten_env: TenEnvTester) -> None:
        unknown_cmd = Cmd.create("unknown_cmd")
        ten_env.send_cmd(
            unknown_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error
            ),
        )

        ten_env.on_start_done()


class ExtensionTesterTimeout(ExtensionTester):
    def on_start(self, ten_env: TenEnvTester) -> None:
        time.sleep(3)
        ten_env.on_start_done()
        ten_env.stop_test()


def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("default_extension_python")
    tester.run()


def test_failure():
    """
    Test the case where the error message is provided.
    """
    tester = ExtensionTesterFail()
    tester.set_test_mode_single("default_extension_python")
    err = tester.run()

    assert err is not None
    assert err.error_code() == TenErrorCode.ErrorCodeGeneric
    assert err.error_message() == "error response"


def test_failure2():
    """
    Test the case where the error message is not provided.
    """
    tester = ExtensionTesterFail2()
    tester.set_test_mode_single("default_extension_python")
    err = tester.run()

    assert err is not None
    assert err.error_code() == TenErrorCode.ErrorCodeGeneric

    # The error message is not provided, so it should be empty.
    assert err.error_message() == ""


def test_timeout():
    tester = ExtensionTesterTimeout()
    tester.set_test_mode_single("default_extension_python")
    tester.set_timeout(1000 * 1000)  # 1s
    err = tester.run()

    assert err is not None
    assert err.error_code() == TenErrorCode.ErrorCodeTimeout
