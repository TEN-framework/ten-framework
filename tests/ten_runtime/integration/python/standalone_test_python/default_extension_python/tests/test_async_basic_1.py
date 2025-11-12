#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
from ten_runtime import (
    Cmd,
    Data,
    AudioFrame,
    VideoFrame,
    StatusCode,
    AsyncExtensionTester,
    AsyncTenEnvTester,
    LogLevel,
    TenError, TenErrorCode
)


class AsyncExtensionTesterBasic(AsyncExtensionTester):
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        await ten_env.send_data(Data.create("test"))
        await ten_env.send_audio_frame(AudioFrame.create("test"))
        await ten_env.send_video_frame(VideoFrame.create("test"))

        new_cmd = Cmd.create("hello_world")

        ten_env.log(LogLevel.INFO, "send hello_world")
        result, error = await ten_env.send_cmd(
            new_cmd,
        )
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        ten_env.log(
            LogLevel.INFO, "receive hello_world, status:" + str(statusCode)
        )

        ten_env.log(LogLevel.INFO, "tester on_start_done")

        if statusCode == StatusCode.OK:
            ten_env.stop_test()

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log(LogLevel.INFO, "tester on_stop")


class AsyncExtensionTesterFail(AsyncExtensionTester):
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        unknown_cmd = Cmd.create("unknown_cmd")

        ten_env.log(LogLevel.INFO, "send unknown_cmd")
        result, error = await ten_env.send_cmd(
            unknown_cmd,
        )
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        ten_env.log(
            LogLevel.INFO, "receive hello_world, status:" + str(statusCode)
        )

        ten_env.log(LogLevel.INFO, "tester on_start_done")

        if statusCode == StatusCode.OK:
            ten_env.stop_test()
        else:
            test_result = TenError.create(
                TenErrorCode.ErrorCodeGeneric, "error response"
            )
            ten_env.stop_test(test_result)

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log(LogLevel.INFO, "tester on_stop")


class AsyncExtensionTesterTimeout(AsyncExtensionTester):
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        await asyncio.sleep(3)
        ten_env.stop_test()


def test_basic():
    tester = AsyncExtensionTesterBasic()
    tester.set_test_mode_single("default_extension_python")
    tester.run()


def test_failure():
    tester = AsyncExtensionTesterFail()
    tester.set_test_mode_single("default_extension_python")
    err = tester.run()

    assert err is not None
    assert err.error_code() == TenErrorCode.ErrorCodeGeneric
    assert err.error_message() == "error response"


def test_timeout():
    tester = AsyncExtensionTesterTimeout()
    tester.set_test_mode_single("default_extension_python")
    tester.set_timeout(1000 * 1000)  # 1s
    err = tester.run()

    assert err is not None
    assert err.error_code() == TenErrorCode.ErrorCodeTimeout


def test_timeout2():
    tester = AsyncExtensionTesterTimeout()
    tester.set_test_mode_single("default_extension_python")
    tester.set_timeout(50 * 1000)  # 50ms
    err = tester.run()

    assert err is not None
    assert err.error_code() == TenErrorCode.ErrorCodeTimeout
