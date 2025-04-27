#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import Optional
from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
    Data,
    AudioFrame,
    VideoFrame,
    StatusCode,
    CmdResult,
    TenError,
)


class DefaultExtension(Extension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.name = name

    def on_configure(self, ten_env: TenEnv) -> None:
        ten_env.log_debug(f"DefaultExtension on_init, name: {self.name}")
        assert self.name == "default_extension_python"

        ten_env.init_property_from_json('{"testKey": "testValue"}')
        ten_env.on_configure_done()

    def handle_error(self, ten_env: TenEnv, error: Optional[TenError]) -> None:
        assert error is not None
        ten_env.log_error(
            "DefaultExtension handle_error: " + error.error_message()
        )

        self.no_dest_error_recv_count += 1
        if self.no_dest_error_recv_count == 4:
            ten_env.on_start_done()

    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_start")

        self.no_dest_error_recv_count = 0

        ten_env.set_property_from_json("testKey2", '"testValue2"')
        testValue, _ = ten_env.get_property_to_json("testKey")
        testValue2, _ = ten_env.get_property_to_json("testKey2")
        print("testValue: ", testValue, " testValue2: ", testValue2)

        # Send an unconnected command
        cmd = Cmd.create("unconnected_cmd")
        ten_env.send_cmd(
            cmd,
            lambda ten_env, result, error: self.handle_error(ten_env, error),
        )

        # Send an unconnected data
        data = Data.create("unconnected_data")
        ten_env.send_data(
            data,
            lambda ten_env, error: self.handle_error(ten_env, error),
        )

        # Send an unconnected video frame
        video_frame = VideoFrame.create("unconnected_video_frame")
        ten_env.send_video_frame(
            video_frame,
            lambda ten_env, error: self.handle_error(ten_env, error),
        )

        # Send an unconnected audio frame
        audio_frame = AudioFrame.create("unconnected_audio_frame")
        ten_env.send_audio_frame(
            audio_frame,
            lambda ten_env, error: self.handle_error(ten_env, error),
        )

    def on_stop(self, ten_env: TenEnv) -> None:
        print("DefaultExtension on_stop")
        ten_env.on_stop_done()

    def on_deinit(self, ten_env: TenEnv) -> None:
        print("DefaultExtension on_deinit")
        ten_env.on_deinit_done()

    def check_hello(
        self,
        ten_env: TenEnv,
        result: Optional[CmdResult],
        error: Optional[TenError],
        receivedCmd: Cmd,
    ):
        if error is not None:
            assert False, error.error_message()

        assert result is not None

        statusCode = result.get_status_code()
        detail, _ = result.get_property_string("detail")
        print(
            "DefaultExtension check_hello: status:"
            + str(statusCode)
            + " detail:"
            + detail
        )

        respCmd = CmdResult.create(StatusCode.OK, receivedCmd)
        respCmd.set_property_string("detail", detail + " nbnb")
        print("DefaultExtension create respCmd")

        ten_env.return_result(respCmd)

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        print("DefaultExtension on_cmd")

        cmd_json, _ = cmd.get_property_to_json()
        print("DefaultExtension on_cmd json: " + cmd_json)

        new_cmd = Cmd.create("hello")
        new_cmd.set_property_from_json("test", '"testValue2"')
        test_value, _ = new_cmd.get_property_to_json("test")
        print("DefaultExtension on_cmd test_value: " + test_value)

        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error, cmd
            ),
        )
