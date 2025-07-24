from typing_extensions import override
from ten_runtime import AsyncExtensionTester, AsyncTenEnvTester, Data, AudioFrame
import json


class AzureAsrExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.recv_error_count = 0

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        ten_env_tester.log_info("on_start")

    @override
    async def on_data(self, ten_env_tester: AsyncTenEnvTester, data: Data) -> None:
        # Expect to receive an error data.
        data_name = data.get_name()
        if data_name == "error":
            self.recv_error_count += 1
            if self.recv_error_count >= 3:
                ten_env_tester.stop_test()
            return

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass


def test_reconnect():
    property_json = {
        "key": "invalid_key",
        "region": "invalid_region",
    }
    tester = AzureAsrExtensionTester()
    tester.set_test_mode_single("azure_asr_python", json.dumps(property_json))
    err = tester.run()
