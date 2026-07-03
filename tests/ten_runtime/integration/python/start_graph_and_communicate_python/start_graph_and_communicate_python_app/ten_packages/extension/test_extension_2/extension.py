#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

import json
from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    TenError,
    LogLevel,
)


class TestExtension2(Extension):
    def __init__(self, name: str) -> None:
        super().__init__(name)

    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_start")

        header, _ = ten_env.get_property_to_json("header")
        header_obj = json.loads(header)
        ten_env.log(LogLevel.INFO, f"header_obj: {header_obj}")
        assert (
            header_obj["x-sagemaker-custom-attributes"]
            == "X-Path:/v1/chat/completions"
        )

        _123_value, _ = ten_env.get_property_string("123")
        ten_env.log(LogLevel.INFO, f"_123_value: {_123_value}")
        assert _123_value == "aaa"

        aaa_bbb_value, _ = ten_env.get_property_string("aaa-bbb")
        ten_env.log(LogLevel.INFO, f"aaa_bbb_value: {aaa_bbb_value}")
        assert aaa_bbb_value == "ccc"

        ten_env.on_start_done()

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()

        if cmd_name == "start":

            self._handle_start_cmd(ten_env, cmd)
        else:
            ten_env.log(
                LogLevel.ERROR,
                f"test_extension_2 received unexpected cmd: {cmd_name}",
            )

    def _handle_start_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_a = Cmd.create("A")

        def callback(
            ten_env: TenEnv,
            _cmd_result: CmdResult | None,
            _error: TenError | None,
        ):
            # Return result for the start command
            result = CmdResult.create(StatusCode.OK, cmd)
            ten_env.return_result(result)

        ten_env.send_cmd(cmd_a, callback)
