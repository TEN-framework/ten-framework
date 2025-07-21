from typing_extensions import override
from ten_runtime import AsyncExtensionTester, AsyncTenEnvTester, Data
import json
import pytest


class AzureAsrExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        ten_env_tester.log_info("on_start")

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        ten_env_tester.log_info(f"on_data")
        ten_env_tester.stop_test()


def test_basic(extension_name):
    property_json = {
        "language": "en-US",
    }
    tester = AzureAsrExtensionTester()
    tester.set_test_mode_single(extension_name, json.dumps(property_json))
    tester.run()
