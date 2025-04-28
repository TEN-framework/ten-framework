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
    Data,
    StatusCode,
    CmdResult,
)


class DefaultExtension(Extension):
    def on_configure(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_init")
        ten_env.init_property_from_json('{"testKey": "testValue"}')
        ten_env.on_configure_done()

    def on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        cmd_json, _ = cmd.get_property_to_json()
        print("DefaultExtension on_cmd json: " + cmd_json)

        new_data = Data.create("data")

        test_bytes = bytearray(b"hello world")

        new_data.alloc_buf(len(test_bytes))
        buf = new_data.lock_buf()
        buf[:] = test_bytes
        print("send buf length: " + str(len(buf)))
        new_data.unlock_buf(buf)
        ten_env.send_data(new_data)

        data = Data.create("data2")
        data.set_property_string("test_key", "test_value")

        ten_env.send_data(data)

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        cmd_result.set_property_string("detail", "send data done")
        ten_env.return_result(cmd_result)

    def on_data(self, ten_env: TenEnv, data: Data) -> None:
        if data.get_name() == "data2":
            value, err = data.get_property_string("test_key")
            if err is not None:
                assert False, "get_property_string failed"
            assert value == "test_value"
            return

        buf = data.get_buf()
        print("DefaultExtension on_data buf: " + str(len(buf)))

        for i in range(len(buf)):
            buf[i] = i + 2

        borrowed_buf = data.lock_buf()

        test_bytes = bytearray(b"hello world")
        if (borrowed_buf is None) or (len(borrowed_buf) != len(test_bytes)):
            raise Exception("buffer is not correct size")

        for i in range(len(test_bytes)):
            if borrowed_buf[i] != test_bytes[i]:
                raise Exception("buffer is not correct")

        data.unlock_buf(borrowed_buf)

        print("DefaultExtension on_data buf: " + str(len(buf)))
