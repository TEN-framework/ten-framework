#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
from typing import Optional
from ten_runtime import Extension, TenEnv, Cmd, StatusCode, CmdResult, TenError


class DefaultExtension(Extension):
    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_start")

        env_value, _ = ten_env.get_property_to_json()
        env_json = json.loads(env_value)
        assert env_json["test_prop"] == 32

        env_value, _ = ten_env.get_property_to_json("")
        env_json = json.loads(env_value)
        assert env_json["test_prop"] == 32

        env_value, _ = ten_env.get_property_to_json("test_prop")
        env_json = json.loads(env_value)
        assert env_json == 32

        ten_env.on_start_done()

    def check_hello(
        self,
        ten_env: TenEnv,
        result: Optional[CmdResult],
        error: Optional[TenError],
        receivedCmd: Cmd,
    ):
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        detail, _ = result.get_property_string("detail")
        ten_env.log_info(
            "check_hello: status:" + str(statusCode) + " detail:" + detail
        )

        respCmd = CmdResult.create(StatusCode.OK, receivedCmd)
        respCmd.set_property_string("detail", detail + " nbnb")
        ten_env.log_info("create respCmd")

        ten_env.return_result(respCmd)

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        ten_env.log_info("on_cmd")
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_info("on_cmd json: " + cmd_json)

        new_cmd = Cmd.create("hello")

        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error, cmd
            ),
        )
