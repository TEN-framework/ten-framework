import asyncio
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
from .mock import patch_azure_ws  # noqa: F401


class AzureAsrExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass

    def stop_test_if_checking_failed(
        self, ten_env_tester: AsyncTenEnvTester, success: bool, error_message: str
    ) -> None:
        if not success:
            err = TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message=error_message,
            )
            ten_env_tester.stop_test(err)

    @override
    async def on_data(self, ten_env_tester: AsyncTenEnvTester, data: Data) -> None:
        data_name = data.get_name()
        if data_name == "error":
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            ten_env_tester.log_info(f"tester recv error, data_dict: {data_dict}")
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
                vendor_info_dict["vendor"] == "microsoft",
                f"vendor is not microsoft: {vendor_info_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                vendor_info_dict["code"] == "123",
                f"code is not 123: {vendor_info_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                vendor_info_dict["message"] == "mock error details",
                f"message is not mock error message: {vendor_info_dict}",
            )

            ten_env_tester.stop_test()


def test_vendor_error(patch_azure_ws):
    def fake_start_continuous_recognition():
        def triggerSessionStarted():
            event = SimpleNamespace(session_id="123")
            patch_azure_ws.event_handlers["session_started"](event)
            threading.Timer(1.0, triggerCanceled).start()

        def triggerCanceled():
            evt = SimpleNamespace(
                cancellation_details=SimpleNamespace(
                    code=123,
                    reason=1,
                    error_details="mock error details",
                )
            )
            patch_azure_ws.event_handlers["canceled"](evt)

        threading.Timer(0.2, triggerSessionStarted).start()
        return None

    def fake_stop_continuous_recognition():
        return None

    # Inject into recognizer
    patch_azure_ws.recognizer_instance.start_continuous_recognition.side_effect = (
        fake_start_continuous_recognition
    )

    patch_azure_ws.recognizer_instance.stop_continuous_recognition.side_effect = (
        fake_stop_continuous_recognition
    )

    property_json = {
        "params": {
            "key": "fake_key",
            "region": "fake_region",
        }
    }

    tester = AzureAsrExtensionTester()
    tester.set_test_mode_single("azure_asr_python", json.dumps(property_json))
    err = tester.run()
    assert (
        err is None
    ), f"test_asr_result err code: {err.error_code()} message: {err.error_message()}"
