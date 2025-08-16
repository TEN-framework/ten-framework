import asyncio
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    TenError,
    TenErrorCode,
)
import json
from ten_ai_base.tts2 import TTSTextInput


class PollyTTSExtensionTester(AsyncExtensionTester):
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

    async def wait_for_test(self, ten_env: AsyncTenEnvTester):
        await asyncio.sleep(10)
        ten_env.stop_test(
            TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message="test timeout",
            )
        )

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env.log_info("Dump test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        await ten_env.send_data(data)
        asyncio.create_task(self.wait_for_test(ten_env))

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        ten_env.log_info(f"on_data: {data}")
        name = data.get_name()
        if name == "error":
            ten_env.log_info("Received error, stopping test.")
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            self.stop_test_if_checking_failed(
                ten_env,
                "code" in data_dict,
                f"error_code is not in data_dict: {data_dict}",
            )
            self.stop_test_if_checking_failed(
                ten_env,
                data_dict["code"] == -1000,
                f"error_code is not -1000: {data_dict}",
            )
            # success stop test
            ten_env.stop_test()


def test_polly_tts():
    property_json = {
        "log_level": "DEBUG",
        "params": {
            "region_name_invalid": "us-west-2",  # invalid name
            "aws_access_key_id": "fake_access_key_id",
            "aws_secret_access_key": "fake_secret_access_key",
        },
    }
    tester = PollyTTSExtensionTester()
    tester.set_test_mode_single("polly_tts", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"{__file__} err: {err}"
