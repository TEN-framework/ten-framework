from typing_extensions import override
from ten_runtime import AsyncExtensionTester, AsyncTenEnvTester, Data, AudioFrame, TenError, TenErrorCode
import json


class AzureAsrExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        ten_env_tester.log_info("on_start")

    def stop_test_if_checking_failed(self, ten_env_tester: AsyncTenEnvTester, success: bool, error_message: str) -> None:
        if not success:
            err = TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message=error_message,
            )
            ten_env_tester.stop_test(err)

    @override
    async def on_data(self, ten_env_tester: AsyncTenEnvTester, data: Data) -> None:
        # Expect to receive an error data.
        data_name = data.get_name()
        if data_name == "error":
            # Check the error.
            error_json, _ = data.get_property_to_json()
            error_data = json.loads(error_json)

            self.stop_test_if_checking_failed(
                ten_env_tester, error_data["id"] == "user.transcription", f"error id is not user.transcription: {error_data['id']}")
            self.stop_test_if_checking_failed(
                ten_env_tester, "code" in error_data, f"error code is not in error_data: {error_data}")
            self.stop_test_if_checking_failed(
                ten_env_tester, "message" in error_data, f"error message is not in error_data: {error_data}")
            self.stop_test_if_checking_failed(
                ten_env_tester, "vendor_info" in error_data, f"error vendor_info is not in error_data: {error_data}")
            self.stop_test_if_checking_failed(
                ten_env_tester, "metadata" in error_data, f"error metadata is not in error_data: {error_data}")
            self.stop_test_if_checking_failed(
                ten_env_tester, "session_id" in error_data["metadata"], f"error session_id is not in error_data: {error_data}")

            ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass


def test_error_check():
    property_json = {
        "key": "invalid_key",
        "region": "invalid_region",
    }
    tester = AzureAsrExtensionTester()
    tester.set_test_mode_single("azure_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None
