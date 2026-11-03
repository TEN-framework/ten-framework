#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from typing import Optional
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Cmd,
    CmdResult,
    Data,
    ExtensionTester,
    StatusCode,
    TenEnvTester,
    TenError,
)


class ExtensionTesterBasic(ExtensionTester):
    def check_hello(
        self,
        ten_env: TenEnvTester,
        result: Optional[CmdResult],
        error: Optional[TenError],
    ):
        if error is not None:
            assert False, error.error_message()

        assert result is not None

        statusCode = result.get_status_code()
        ten_env.log_debug(f"receive hello_world, status: {statusCode}")

        if statusCode == StatusCode.OK:
            ten_env.stop_test()

    def on_start(self, ten_env: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")

        ten_env.log_debug("send hello_world")
        ten_env.send_cmd(
            new_cmd,
            lambda ten_env, result, error: self.check_hello(
                ten_env, result, error
            ),
        )

        ten_env.log_debug("tester on_start_done")
        ten_env.on_start_done()


def test_basic():
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("spatialreal_avatar_python")
    tester.run()


class ExtensionTesterConfigError(AsyncExtensionTester):
    """Starts the extension with empty params and waits for data ``error``."""

    def __init__(self):
        super().__init__()
        self.error: Optional[dict] = None

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_debug("tester on_start: waiting for the config error")

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        if data.get_name() != "error":
            return
        json_str, _ = data.get_property_to_json(None)
        self.error = json.loads(json_str)
        ten_env.log_debug(f"received error: {self.error}")
        ten_env.stop_test()


def test_config_error_uses_module_error():
    # Every key the operator must set is empty, so on_start reports the
    # design D8 configuration error and leaves the extension idle; the
    # extension must not raise and must not try to connect.
    empty_params = {
        "channel": "",
        "params": {
            "spatialreal_api_key": "",
            "spatialreal_avatar_id": "",
            "agora_appid": "",
            "agora_uid": "",
        },
    }
    tester = ExtensionTesterConfigError()
    tester.set_test_mode_single(
        "spatialreal_avatar_python", json.dumps(empty_params)
    )
    error = tester.run()
    assert error is None, error.error_message()

    assert tester.error is not None
    assert tester.error["module"] == "avatar"
    assert tester.error["code"] == -1012
    assert "spatialreal_api_key" in tester.error["message"]
    vendor_info = tester.error["vendor_info"]
    assert vendor_info["vendor"] == "spatialreal"
    assert vendor_info["code"] == "ValueError"
    metadata = tester.error["metadata"]
    assert metadata["session_id"] == ""
    vendor_metadata = metadata["vendor_metadata"]
    assert vendor_metadata["name"] == "spatialreal"
    for key in vendor_metadata:
        assert "key" not in key
        assert "url" not in key
    # No empty values travel in vendor_metadata (D8).
    assert all(value not in ("", None) for value in vendor_metadata.values())
