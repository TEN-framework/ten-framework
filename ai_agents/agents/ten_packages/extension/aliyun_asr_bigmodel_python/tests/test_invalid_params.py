from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    AudioFrame,
)
import json


class AliyunASRBigmodelExtensionTester(AsyncExtensionTester):

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        ten_env_tester.log_info("on_start")

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        # Expect to receive an error data.
        data_name = data.get_name()
        if data_name == "error":
            ten_env_tester.stop_test()
            return

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass


def test_invalid_params():
    property_json = {
        "api_key": "invalid",
    }
    tester = AliyunASRBigmodelExtensionTester()
    tester.set_test_mode_single(
        "aliyun_asr_bigmodel_python", json.dumps(property_json)
    )
    err = tester.run()
