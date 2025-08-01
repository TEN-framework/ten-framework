import asyncio
import os
from typing_extensions import override
from ten_runtime import AsyncExtensionTester, AsyncTenEnvTester, Data, AudioFrame, TenError, TenErrorCode
import json


class XfyunBigmodelASRExtensionTester(AsyncExtensionTester):

    def __init__(self, audio_file_path: str):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.audio_file_path: str = audio_file_path

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        print(f"audio_file_path: {self.audio_file_path}")
        with open(self.audio_file_path, "rb") as audio_file:
            total_ms = 0
            chunk_size = 320
            while True:
                chunk = audio_file.read(chunk_size)
                if not chunk:
                    break
                audio_frame = AudioFrame.create("pcm_frame")
                metadata = {"session_id": "123"}
                audio_frame.set_property_from_json("metadata", json.dumps(metadata))
                audio_frame.alloc_buf(len(chunk))
                buf = audio_frame.lock_buf()
                buf[:] = chunk
                audio_frame.unlock_buf(buf)
                _ = await ten_env.send_audio_frame(audio_frame)
                total_ms += 10
                # if total_ms >= 280000:
                #     break
                await asyncio.sleep(0.01)

    async def send_finalize_event(self, ten_env: AsyncTenEnvTester):
        finalize_data = Data.create("asr_finalize")

        data = {
            "finalize_id": "1",
            "metadata": {
                "session_id": "123",
            },
        }

        finalize_data.set_property_from_json(None, json.dumps(data))
        await ten_env.send_data(finalize_data)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        self.sender_task = asyncio.create_task(self.audio_sender(ten_env_tester))

        # send a finalize event after 1.5 seconds
        await asyncio.sleep(10)
        await self.send_finalize_event(ten_env_tester)

    @override
    async def on_data(self, ten_env_tester: AsyncTenEnvTester, data: Data) -> None:
        data_name = data.get_name()

        print(f"data_name_______: {data_name}")

        if data_name == "asr_finalize_end":
            # Check if the finalize_id equals to the one in finalize data.
            finalize_id, _ = data.get_property_string("finalize_id")
            self.stop_test_if_checking_failed(
                ten_env_tester,
                finalize_id == "1",
                f"finalize_id is not '1': {finalize_id}",
            )

            # Check if the metadata equals to the one in finalize data.
            metadata_json, _ = data.get_property_to_json("metadata")
            metadata_dict = json.loads(metadata_json)
            self.stop_test_if_checking_failed(
                ten_env_tester,
                metadata_dict["session_id"] == "123",
                f"session_id is not 123 in asr_finalize_end: {metadata_dict}",
            )

            ten_env_tester.stop_test()


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
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


def test_asr_result():
    property_json = {
      "params": {
        "api_key": "${env:XFYUN_ASR_BIGMODEL_API_KEY}",
        "app_id": "${env:XFYUN_ASR_BIGMODEL_APP_ID}",
        "api_secret": "${env:XFYUN_ASR_BIGMODEL_API_SECRET}",
        "lang": "zh_cn",
        "sample_rate": 16000,
      }
    }
    audio_file_path = os.path.join(
        os.path.dirname(__file__), f"test_data/16k_en_US.pcm"
    )
    tester = XfyunBigmodelASRExtensionTester(audio_file_path)
    tester.set_test_mode_single("xfyun_asr_bigmodel_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"err: {err.error_message()}"
