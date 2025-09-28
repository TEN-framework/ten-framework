import asyncio
import json
from unittest.mock import patch, AsyncMock
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    TenError,
    TenErrorCode,
)

from ten_ai_base.tts2 import TTSTextInput
from ten_ai_base.message import ModuleErrorCode


class MockPlayHTTTSExtensionTester(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.expect_error_code = ModuleErrorCode.NON_FATAL_ERROR.value
        self.max_wait_time = 10

    def stop_test_if_checking_failed(
        self,
        ten_env_tester: AsyncTenEnvTester,
        success: bool,
        error_message: str,
    ) -> None:
        if not success:
            ten_env_tester.log_error(
                f"stop_test_if_checking_failed: {error_message}"
            )
            err = TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message=error_message,
            )
            ten_env_tester.stop_test(err)

    async def wait_for_test(self, ten_env: AsyncTenEnvTester):
        await asyncio.sleep(self.max_wait_time)
        ten_env.stop_test(
            TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message="test timeout",
            )
        )

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env.log_info("Mock test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello world, hello playht",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        await ten_env.send_data(data)
        asyncio.create_task(self.wait_for_test(ten_env))

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

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
                data_dict["code"] == int(self.expect_error_code),
                f"error_code is not {self.expect_error_code}: {data_dict}",
            )
            # success stop test
            ten_env.stop_test()
        elif name == "tts_audio_end":
            ten_env.log_info("Received TTS audio data, stopping test.")
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            self.stop_test_if_checking_failed(
                ten_env,
                "request_id" in data_dict,
                f"request_id is not in data_dict: {data_dict}",
            )
            self.stop_test_if_checking_failed(
                ten_env,
                data_dict["request_id"] == "tts_request_1",
                f"request_id is not tts_request_1: {data_dict}",
            )
            # success stop test
            ten_env.stop_test()


class MockAsyncIterator:
    def __init__(self, items):
        self.items = items
        self.index = 0

    def __iter__(self):
        return iter(self.items)

    def __aiter__(self):
        return self

    async def __anext__(self):
        if self.index >= len(self.items):
            raise StopAsyncIteration
        item = self.items[self.index]
        self.index += 1
        return item


@patch("ten_packages.extension.playht_tts_python.playht_tts.AsyncClient")
def test_playht_tts_extension_success(mock_client_class):
    """test playht tts extension success"""
    # create mock instance
    mock_client_instance = AsyncMock()
    mock_client_class.return_value = mock_client_instance

    # mock the tts method to return async iterator
    async def mock_tts_async_iterator(*args, **kwargs):
        audio_chunks = [
            b"mock_audio_chunk_1",
            b"mock_audio_chunk_2",
            b"mock_audio_chunk_3",
        ]
        for chunk in audio_chunks:
            yield chunk

    mock_client_instance.tts = mock_tts_async_iterator

    property_json = {
        "log_level": "DEBUG",
        "dump": False,
        "dump_path": "/tmp/playht_tts_test.pcm",
        "params": {
            "api_key": "fake_api_key",
            "user_id": "fake_user_id",
            "voice_engine": "PlayDialog",
            "protocol": "ws",
            "format": "FORMAT_PCM",
            "sample_rate": 16000,
            "voice": "s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        },
    }

    tester = MockPlayHTTTSExtensionTester()
    tester.set_test_mode_single("playht_tts_python", json.dumps(property_json))
    tester.max_wait_time = 30
    err = tester.run()

    # simple assert - as long as no exception is thrown, it is considered successful
    assert (
        err is None
    ), f"test_playht_tts_extension_success err: {err.error_message() if err else 'None'}"
