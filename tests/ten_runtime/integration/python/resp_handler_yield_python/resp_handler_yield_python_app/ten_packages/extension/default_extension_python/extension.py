#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import queue
import threading
from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
    CmdResult,
)


class DefaultExtension(Extension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.name = name

    def on_configure(self, ten_env: TenEnv) -> None:
        ten_env.log_info(f"on_init, name: {self.name}")
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

    def echo_cmd_result_generator(self, ten_env: TenEnv, cmd: Cmd):
        ten_env.log_info("send_cmd_yeild")

        q = queue.Queue(maxsize=1)

        def task():
            ten_env.send_cmd(
                cmd,
                lambda ten_env, result, error: q.put(
                    error if error is not None else result
                ),
            )

        t = threading.Thread(target=task)
        t.start()

        yield q.get()

    def __handle_cmd(self, ten_env: TenEnv, cmd: Cmd):
        ten_env.log_info("__handle_cmd")

        cmd_hello = Cmd.create("hello")

        generator = self.echo_cmd_result_generator(ten_env, cmd_hello)

        result = next(generator)

        if isinstance(result, Exception):
            raise result

        cmd_result_json, _ = result.get_property_to_json()

        new_result = CmdResult.create(result.get_status_code(), cmd)
        new_result.set_property_from_json(None, cmd_result_json)

        ten_env.return_result(new_result)

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        ten_env.log_info("on_cmd")

        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_info("on_cmd json: " + cmd_json)

        self.thread = threading.Thread(
            target=self.__handle_cmd, args=(ten_env, cmd)
        )
        self.thread.start()
