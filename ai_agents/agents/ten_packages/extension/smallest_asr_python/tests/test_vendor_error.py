import threading
from types import SimpleNamespace
from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    AudioFrame,
    TenError,
    TenErrorCode,
)
import json

# We must import it, which means this test fixture will be automatically executed
from .mock import patch_smallest_ws  # noqa: F401


class SmallestAsrExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass

    def stop_test_if_checking_failed(
        self,
        ten_env_tester: AsyncTenEnvTester,
        success: bool,
        error_message: str,
    ) -> None:
        if not success:
            err = TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message=error_message,
            )
            ten_env_tester.stop_test(err)

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        data_name = data.get_name()
        if data_name == "error":
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            ten_env_tester.log_info(
                f"tester recv error, data_dict: {data_dict}"
            )
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "id" in data_dict,
                f"id is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                data_dict["code"] == 1000,
                f"code is not NON_FATAL_ERROR: {data_dict}",
            )

            vendor_info_json, _ = data.get_property_to_json("vendor_info")
            vendor_info_dict = json.loads(vendor_info_json)
            self.stop_test_if_checking_failed(
                ten_env_tester,
                vendor_info_dict["vendor"] == "smallest",
                f"vendor is not smallest: {vendor_info_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "code" in vendor_info_dict,
                f"code is not in vendor_info: {vendor_info_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "message" in vendor_info_dict,
                f"message is not in vendor_info: {vendor_info_dict}",
            )

            ten_env_tester.stop_test()


def test_vendor_error(patch_smallest_ws):
    def trigger_error_message():
        """Add WebSocket error message to simulate Smallest AI Pulse API error."""
        error_message = {
            "type": "error",
            "message": "mock error details",
            "code": "123",
        }

        msg = patch_smallest_ws.MockWebSocketMessage(
            msg_type=patch_smallest_ws.WSMsgType.TEXT,
            data=json.dumps(error_message),
        )
        patch_smallest_ws.add_message(msg)

    # Schedule message to be added after connection is established
    def delayed_message_sender():
        import time

        time.sleep(1)  # Wait for connection
        trigger_error_message()

    # Start the message sender in a separate thread
    sender_thread = threading.Thread(target=delayed_message_sender, daemon=True)
    sender_thread.start()

    property_json = {
        "params": {
            "api_key": "fake_api_key",
            "sample_rate": 16000,
        }
    }

    tester = SmallestAsrExtensionTester()
    tester.set_test_mode_single(
        "smallest_asr_python", json.dumps(property_json)
    )
    err = tester.run()
    assert (
        err is None
    ), f"test_vendor_error err code: {err.error_code()} message: {err.error_message()}"
