#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Cmd,
    StatusCode,
    LogLevel,
    TenError,
    TenErrorCode,
    Data,
)
from ten_ai_base.message import ModuleError, ModuleErrorCode


class ExtensionTesterBasic(AsyncExtensionTester):
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        ten_env.log(LogLevel.DEBUG, "send hello_world")
        result, err = await ten_env.send_cmd(new_cmd)
        if (
            err is not None
            or result is None
            or result.get_status_code() != StatusCode.OK
        ):
            ten_env.stop_test(
                TenError.create(
                    TenErrorCode.ErrorCodeGeneric,
                    "Failed to send hello_world",
                )
            )
        else:
            ten_env.stop_test()

        ten_env.log(LogLevel.DEBUG, "tester on_start_done")


def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("spatius_avatar_python")
    err = tester.run()
    if err is not None:
        assert False, err.error_message()


class ExtensionTesterError(AsyncExtensionTester):
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info("waiting for config error")

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        if data.get_name() != "error":
            return

        payload_json, error = data.get_property_to_json("")
        assert error is None
        payload = ModuleError.model_validate_json(payload_json)
        assert payload.id == "0"
        assert payload.module == "avatar"
        assert payload.code == int(ModuleErrorCode.FATAL_ERROR.value)
        assert payload.vendor_info.vendor == "spatius"
        assert payload.vendor_info.code == "ValueError"
        vendor_metadata = payload.metadata["vendor_metadata"]
        assert vendor_metadata["name"] == "spatius"
        assert all(
            value not in ("", None) for value in vendor_metadata.values()
        )
        assert "url" not in vendor_metadata
        assert "key" not in vendor_metadata
        ten_env.stop_test()


def test_config_error_uses_module_error():
    tester = ExtensionTesterError()
    tester.set_test_mode_single("spatius_avatar_python")
    err = tester.run()
    if err is not None:
        assert False, err.error_message()
