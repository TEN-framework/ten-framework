#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
from typing import Any
from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    TenError,
    LogLevel,
)


class Params:
    def __init__(self, key: str = "", model: str = ""):
        self.key: str = key
        self.model: str = model

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Params":
        params_data: dict[str, Any] = data.get("params", {})
        return cls(
            key=params_data.get("key", ""), model=params_data.get("model", "")
        )


class DefaultExtension(Extension):
    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_start")

        property_json, _ = ten_env.get_property_to_json()
        ten_env.log_info(f"property_json: {property_json}")

        # Deserialize the property json to Params
        try:
            property_data: dict[str, Any] = json.loads(property_json)
            params = Params.from_dict(property_data)
        except (json.JSONDecodeError, KeyError) as e:
            ten_env.log_error(f"Failed to deserialize property_json: {e}")
            params = Params()

        key = params.key
        model = params.model
        ten_env.log_info(f"key: {key}, model: {model}")

        ten_env.on_start_done()

    def check_hello(
        self,
        ten_env: TenEnv,
        result: CmdResult | None,
        error: TenError | None,
        receivedCmd: Cmd,
    ):
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        detail, _ = result.get_property_string("detail")
        ten_env.log(
            LogLevel.INFO,
            "check_hello: status:" + str(statusCode) + " detail:" + detail,
        )

        respCmd = CmdResult.create(StatusCode.OK, receivedCmd)
        respCmd.set_property_string("detail", detail + " nbnb")
        ten_env.log(LogLevel.INFO, "create respCmd")

        ten_env.return_result(respCmd)

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        ten_env.log(LogLevel.INFO, "on_cmd")
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log(LogLevel.INFO, "on_cmd json: " + cmd_json)

        new_cmd = Cmd.create("hello")
        new_cmd.set_property_from_json("test", '"testValue2"')
        test_value, _ = new_cmd.get_property_to_json("test")
        assert test_value == '"testValue2"'

        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error, cmd
            ),
        )
