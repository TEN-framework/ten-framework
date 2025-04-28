#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
from typing import Optional
from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
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

    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_start")

        ten_env.set_property_from_json("testKey2", '"testValue2"')
        testValue, _ = ten_env.get_property_to_json("testKey")
        testValue2, _ = ten_env.get_property_to_json("testKey2")
        ten_env.log_info(f"testValue: {testValue}, testValue2: {testValue2}")

        ten_env.on_start_done()

    def on_stop(self, ten_env: TenEnv) -> None:
        ten_env.log_info("on_stop")
        ten_env.on_stop_done()

    def on_deinit(self, ten_env: TenEnv) -> None:
        ten_env.log_info("on_deinit")
        ten_env.on_deinit_done()

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

        cmd_json, _ = cmd.get_property_to_json(None)
        ten_env.log_info("on_cmd json: " + cmd_json)

        new_cmd = Cmd.create("hello")
        new_cmd.set_property_from_json(
            "tool",
            json.dumps(
                {
                    "name": "get_current_weather",
                    "description": (
                        "Determine current weather in user's location."
                    ),
                    "parameters": [
                        {
                            "name": "location",
                            "type": "string",
                            "description": (
                                "The city and state (use only English) "
                                "e.g. San Francisco, CA"
                            ),
                            "required": "true",
                        }
                    ],
                }
            ),
        )

        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error, cmd
            ),
        )
