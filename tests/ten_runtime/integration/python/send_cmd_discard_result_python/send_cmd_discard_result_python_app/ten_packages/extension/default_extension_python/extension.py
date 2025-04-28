#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
    StatusCode,
    CmdResult,
)


class DefaultExtension(Extension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.name = name

    def on_init(self, ten_env: TenEnv) -> None:
        ten_env.log_info("on_init")
        ten_env.on_init_done()

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_info(f"on_cmd json: {cmd_json}")

        if self.name == "default_extension_python_1":
            if cmd.get_name() == "test":
                self.cached_cmd = cmd
                new_cmd = Cmd.create("hello")
                ten_env.send_cmd(new_cmd, None)
            elif cmd.get_name() == "hello2":
                cmd_result = CmdResult.create(StatusCode.OK, self.cached_cmd)
                cmd_result.set_property_string("detail", "nbnb")
                ten_env.return_result(cmd_result)
        elif self.name == "default_extension_python_2":
            ten_env.log_info("create respCmd 1")
            ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

            ten_env.log_info("create respCmd 2")
            hello2_cmd = Cmd.create("hello2")
            ten_env.send_cmd(hello2_cmd, None)
