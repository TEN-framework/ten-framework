#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import threading
from typing import Optional
from ten_runtime import Extension, TenEnv, Cmd, StatusCode, CmdResult, TenError


class DefaultExtension(Extension):
    def on_configure(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_init")

        ten_env.init_property_from_json('{"testKey": "testValue"}')
        ten_env.on_configure_done()

    def __test_thread_routine(self, ten_env: TenEnv):
        i = 0
        for _ in range(0, 10000):
            _, err = ten_env.get_property_string("undefinedKey")
            if err is not None:
                i += 1

        assert i == 10000
        ten_env.log_info("__test_thread_routine done")

    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_start")

        assert ten_env.is_property_exist("env_not_set_has_default")[0] is True

        env_value, err = ten_env.get_property_string("env_not_set_has_default")
        assert env_value == ""
        assert err is None

        assert ten_env.is_property_exist("undefined_key")[0] is False

        ten_env.set_property_from_json("testKey2", '"testValue2"')

        assert ten_env.is_property_exist("testKey")[0] is True
        assert ten_env.is_property_exist("testKey2")[0] is True
        testValue, _ = ten_env.get_property_to_json("testKey")
        testValue2, _ = ten_env.get_property_to_json("testKey2")
        assert testValue == '"testValue"'
        assert testValue2 == '"testValue2"'

        ten_env.set_property_bool("testBoolTrue", True)
        ten_env.set_property_bool("testBoolFalse", False)
        assert ten_env.get_property_bool("testBoolTrue")[0] is True
        assert ten_env.get_property_bool("testBoolFalse")[0] is False

        ten_env.set_property_int("testInt", 123)
        assert ten_env.get_property_int("testInt")[0] == 123

        ten_env.set_property_float("testFloat", 123.456)
        assert ten_env.get_property_float("testFloat")[0] == 123.456

        ten_env.set_property_string("testString", "testString")
        assert ten_env.get_property_string("testString")[0] == "testString"

        self.thread_test = threading.Thread(
            target=self.__test_thread_routine, args=(ten_env,)
        )

        self.thread_test.start()

        i = 0
        for _ in range(0, 10000):
            _, err = ten_env.get_property_string("undefinedKey")
            if err is not None:
                i += 1

        assert i == 10000

        ten_env.on_start_done()

    def __join_thread(self, ten_env: TenEnv):
        if self.thread_test.is_alive():
            self.thread_test.join()

        ten_env.on_stop_done()

    def on_stop(self, ten_env: TenEnv) -> None:
        ten_env.log_info("on_stop")

        # Start a new thread to join the previous thread to avoid blocking the
        # TEN extension thread.
        threading.Thread(target=self.__join_thread, args=(ten_env,)).start()

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

        i = 0
        for _ in range(0, 10000):
            _, err = result.get_property_string("undefinedKey")
            if err is not None:
                i += 1

        assert i == 10000

        respCmd = CmdResult.create(StatusCode.OK, receivedCmd)
        respCmd.set_property_string("detail", detail + " nbnb")
        ten_env.log_info("create respCmd")

        ten_env.return_result(respCmd)

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        ten_env.log_info("on_cmd")
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_info("on_cmd json: " + cmd_json)

        new_cmd = Cmd.create("hello")
        new_cmd.set_property_from_json("test", '"testValue2"')
        test_value, _ = new_cmd.get_property_to_json("test")
        assert test_value == '"testValue2"'

        new_cmd.set_property_bool("testBoolTrue", True)
        new_cmd.set_property_bool("testBoolFalse", False)
        assert new_cmd.get_property_bool("testBoolTrue")[0] is True
        assert new_cmd.get_property_bool("testBoolFalse")[0] is False

        new_cmd.set_property_int("testInt", 123)
        assert new_cmd.get_property_int("testInt")[0] == 123

        new_cmd.set_property_float("testFloat", 123.456)
        assert new_cmd.get_property_float("testFloat")[0] == 123.456

        new_cmd.set_property_string("testString", "testString")
        assert new_cmd.get_property_string("testString")[0] == "testString"

        new_cmd.set_property_buf("testBuf", b"testBuf")
        assert new_cmd.get_property_buf("testBuf")[0] == b"testBuf"

        _, err = new_cmd.get_property_string("undefinedKey")
        assert err is not None

        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error, cmd
            ),
        )
