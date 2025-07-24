import asyncio
import os
from typing_extensions import override
from ten_runtime import AsyncExtensionTester, AsyncTenEnvTester, Data, AudioFrame, TenError, TenErrorCode
import json


class AzureAsrExtensionTester(AsyncExtensionTester):

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
                audio_frame.set_property_int("stream_id", 123)
                audio_frame.set_property_string("remote_user_id", "123")
                audio_frame.alloc_buf(len(chunk))
                buf = audio_frame.lock_buf()
                buf[:] = chunk
                audio_frame.unlock_buf(buf)
                _ = await ten_env.send_audio_frame(audio_frame)
                total_ms += 10
                if total_ms >= 2800:
                    break
                await asyncio.sleep(0.01)

            finalize_data = Data.create("finalize")
            await ten_env.send_data(finalize_data)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        ten_env_tester.log_info("on_start")

        # Wait for 3 seconds to ensure the audio sender is started.
        await asyncio.sleep(3)

        self.sender_task = asyncio.create_task(
            self.audio_sender(ten_env_tester))

    @override
    async def on_data(self, ten_env_tester: AsyncTenEnvTester, data: Data) -> None:
        data_name = data.get_name()
        if data_name == "asr_finalize_end":
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(f"tester on_data, data_dict: {data_dict}")

            id, err = data.get_property_string("id")
            if err is not None:
                ten_env_tester.stop_test(err)
                return

            latency_ms, _ = data.get_property_int("latency_ms")
            if err is not None:
                ten_env_tester.stop_test(err)
                return

            ten_env_tester.log_info(
                f"tester on_data, id: {id}, latency_ms: {latency_ms}")

            ten_env_tester.stop_test()

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
        "key": os.getenv("AZURE_ASR_API_KEY", ""),
        "region": os.getenv("AZURE_ASR_REGION", ""),
        "language": "zh-CN",
        "sample_rate": 16000,
    }
    audio_file_path = os.path.join(
        os.path.dirname(__file__), f"test_data/16k_zh_CN.pcm"
    )
    tester = AzureAsrExtensionTester(audio_file_path)
    tester.set_test_mode_single("azure_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"err: {err}"
