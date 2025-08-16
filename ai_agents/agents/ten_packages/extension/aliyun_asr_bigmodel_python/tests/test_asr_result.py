import asyncio
import os
import pytest
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


class AliyunASRBigmodelExtensionTester(AsyncExtensionTester):

    def __init__(self, audio_file_path: str):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.audio_file_path: str = audio_file_path

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        print(f"audio_file_path: {self.audio_file_path}")
        with open(self.audio_file_path, "rb") as audio_file:
            chunk_size = 320
            while True:
                chunk = audio_file.read(chunk_size)
                if not chunk:
                    break
                audio_frame = AudioFrame.create("pcm_frame")
                audio_frame.set_property_int("stream_id", 123)
                audio_frame.set_property_string("remote_user_id", "123")
                audio_frame.alloc_buf(len(chunk))
                buf = audio_frame.lock_buf()
                buf[:] = chunk
                audio_frame.unlock_buf(buf)
                _ = await ten_env.send_audio_frame(audio_frame)
                await asyncio.sleep(0.01)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        ten_env_tester.log_info("on_start")
        self.sender_task = asyncio.create_task(
            self.audio_sender(ten_env_tester)
        )

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
        if data_name == "asr_result":
            # Check the data structure.

            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(f"tester on_data, data_dict: {data_dict}")

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "id" in data_dict,
                f"id is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "text" in data_dict,
                f"text is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "final" in data_dict,
                f"final is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "start_ms" in data_dict,
                f"start_ms is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "duration_ms" in data_dict,
                f"duration_ms is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "language" in data_dict,
                f"language is not in data_dict: {data_dict}",
            )

            if data_dict["final"] == True:
                ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


# Skip this test module by default unless a real vendor key is provided.
ALIYUN_API_ENV = "ALIYUN_ASR_BIGMODEL_API_KEY"
pytestmark = pytest.mark.skipif(
    not os.getenv(ALIYUN_API_ENV),
    reason=f"Requires real vendor API key in env var {ALIYUN_API_ENV}",
)


def test_asr_result():
    property_json = {
        "params": {
            "api_key": "${env:ALIYUN_ASR_BIGMODEL_API_KEY}",
            "language_hints": ["en"],
            "sample_rate": 16000,
        }
    }

    audio_file_path = os.path.join(
        os.path.dirname(__file__), f"test_data/16k_en_US.pcm"
    )
    # Check if the audio file exists
    if not os.path.exists(audio_file_path):
        pytest.skip(f"Audio file {audio_file_path} does not exist")

    tester = AliyunASRBigmodelExtensionTester(audio_file_path)
    tester.set_test_mode_single(
        "aliyun_asr_bigmodel_python", json.dumps(property_json)
    )
    err = tester.run()
    assert err is None, f"err: {err}"
