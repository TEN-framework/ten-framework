#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

# import debugpy
# debugpy.listen(5678)
# debugpy.wait_for_client()

import threading
import time
from typing import Optional
from ten_runtime import Extension, TenEnv, Cmd, StatusCode, CmdResult, TenError


class DefaultExtension(Extension):
    def on_configure(self, ten_env: TenEnv) -> None:
        self.stop_flag = False
        self.log_count = 0
        ten_env.log_debug("on_init")

        ten_env.init_property_from_json('{"testKey": "testValue"}')
        ten_env.on_configure_done()

    def log_routine(self, ten_env: TenEnv) -> None:
        while not self.stop_flag:
            self.log_count += 1
            ten_env.log_info(f"log message {self.log_count}")
            time.sleep(0.05)

    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_start")

        ten_env.set_property_from_json("testKey2", '"testValue2"')
        testValue, _ = ten_env.get_property_to_json("testKey")
        testValue2, _ = ten_env.get_property_to_json("testKey2")
        ten_env.log_info(f"testValue: {testValue}, testValue2: {testValue2}")

        # Start a thread to log messages.
        self.log_thread = threading.Thread(
            target=self.log_routine, args=(ten_env,)
        )
        self.log_thread.start()

        ten_env.on_start_done()

    def stop_routine(self, ten_env: TenEnv) -> None:
        self.stop_flag = True
        self.log_thread.join()

        ten_env.on_stop_done()

    def on_stop(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_stop")

        # Because `stop_routine` is a blocking function, and to avoid blocking
        # the extension thread, we need to create a new thread to stop the log
        # thread/routine.
        self.stop_thread = threading.Thread(
            target=self.stop_routine, args=(ten_env,)
        )
        self.stop_thread.start()

    def on_deinit(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_deinit")
        self.stop_thread.join()

        ten_env.on_deinit_done()

    def check_greeting(
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
        ten_env.log_info(f"check_greeting: status: {str(statusCode)}")

        respCmd = CmdResult.create(StatusCode.OK, receivedCmd)
        respCmd.set_property_string("detail", "received response")
        ten_env.log_info("create respCmd")

        ten_env.return_result(respCmd)

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
        ten_env.log_info(
            f"check_hello: status: {str(statusCode)}, detail: {detail}"
        )

        # Send a command to go extension.
        new_cmd = Cmd.create("greeting")
        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_greeting(
                ten_env, result, error, receivedCmd
            ),
        )

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_debug("on_cmd: " + cmd_json)

        new_cmd = Cmd.create("hello")
        new_cmd.set_property_from_json("test", '"testValue2"')
        test_value, _ = new_cmd.get_property_to_json("test")
        ten_env.log_info(f"on_cmd test_value: {test_value}")

        # Send command to a cpp extension.
        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error, cmd
            ),
        )
