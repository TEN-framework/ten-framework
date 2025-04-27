#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import Optional
from ten_runtime import Extension, TenEnv, Cmd, StatusCode, CmdResult, TenError


class DefaultExtension(Extension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.name = name
        self.__counter = 0

    def on_init(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_init")
        ten_env.on_init_done()

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

        self.__counter += 1

        if self.__counter == 1:
            assert result.is_final() is False
            ten_env.log_info("receive 1 cmd result")
        elif self.__counter == 2:
            assert result.is_final() is True
            ten_env.log_info("receive 2 cmd result")

            respCmd = CmdResult.create(StatusCode.OK, receivedCmd)
            respCmd.set_property_string("detail", "nbnb")
            ten_env.return_result(respCmd)

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_debug(f"on_cmd json: {cmd_json}")

        if self.name == "default_extension_python_1":
            new_cmd = Cmd.create("hello")
            ten_env.send_cmd_ex(
                new_cmd,
                lambda ten_env, result, error: self.check_hello(
                    ten_env, result, error, cmd
                ),
            )
        elif self.name == "default_extension_python_2":
            ten_env.log_info("create respCmd 1")
            respCmd = CmdResult.create(StatusCode.OK, cmd)
            # The following line is the key.
            respCmd.set_final(False)
            ten_env.return_result(respCmd)

            ten_env.log_info("create respCmd 2")
            respCmd = CmdResult.create(StatusCode.OK, cmd)
            ten_env.return_result(respCmd)
