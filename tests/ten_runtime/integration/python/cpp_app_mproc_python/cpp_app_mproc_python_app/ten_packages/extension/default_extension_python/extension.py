#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import multiprocessing as mp
import os
import sys
import time
from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    TenError,
    LogLevel,
)


class DefaultExtension(Extension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.name = name

        mp.set_start_method("spawn", force=True)

        # Set the Python interpreter path for child processes when embedded.
        # When Python is embedded in a C++ application, sys.executable points
        # to the C++ executable instead of python.exe. This causes
        # multiprocessing.Process() to restart the entire C++ application
        # instead of just spawning a Python subprocess on Windows.
        # See: https://docs.python.org/3/library/multiprocessing.html#multiprocessing.set_executable
        if sys.platform == "win32":
            python_exe = os.path.join(sys.exec_prefix, "python.exe")
            if os.path.exists(python_exe):
                mp.set_executable(python_exe)

        self.ready = mp.Value("b", False)

    def on_configure(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_init")
        assert self.name == "default_extension_python"

        ten_env.init_property_from_json('{"testKey": "testValue"}')
        ten_env.on_configure_done()

    def on_start(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_start")

        ten_env.set_property_from_json("testKey2", '"testValue2"')
        testValue, _ = ten_env.get_property_to_json("testKey")
        testValue2, _ = ten_env.get_property_to_json("testKey2")
        ten_env.log(
            LogLevel.INFO, f"testValue: {testValue}, testValue2: {testValue2}"
        )

        ten_env.on_start_done()

    def on_stop(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_stop")
        ten_env.on_stop_done()

    def on_deinit(self, ten_env: TenEnv) -> None:
        ten_env.log(LogLevel.DEBUG, "on_deinit")
        ten_env.on_deinit_done()

    def check_hello(
        self,
        ten_env: TenEnv,
        result: CmdResult | None,
        error: TenError | None,
        receivedCmd: Cmd,
    ):
        if error is not None:
            assert False, error.error_message()

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
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log(LogLevel.DEBUG, "on_cmd json: " + cmd_json)

        new_cmd = Cmd.create("hello")
        new_cmd.set_property_from_json("test", '"testValue2"')
        test_value, _ = new_cmd.get_property_to_json("test")
        ten_env.log(LogLevel.DEBUG, f"on_cmd test_value: {test_value}")

        # We set the LD_PRELOAD environment variable to libasan.so so that
        # the subprocess can be run with the AddressSanitizer library.
        os.environ["LD_PRELOAD"] = (
            "ten_packages/system/ten_runtime/lib/libasan.so"
        )

        p = mp.Process(target=f, args=("subproc", self.ready))
        p.start()

        success = True
        count = 0
        while not self.ready.value:
            ten_env.log(LogLevel.DEBUG, "Waiting for server to become ready...")
            count += 1
            if count > 5:
                success = False
                break
            time.sleep(1)
        assert success

        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error, cmd
            ),
        )


def info(title):
    print(title)
    print("module name:", __name__)
    print("parent process:", os.getppid())
    print("process id:", os.getpid())


def f(name, ready):
    info("function f")
    print("hello", name)
    ready.value = True
