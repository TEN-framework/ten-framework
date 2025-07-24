import asyncio
import os
from typing_extensions import override
from ten_runtime import AsyncExtensionTester, AsyncTenEnvTester, Data, AudioFrame
import json


class AzureAsrExtensionTester(AsyncExtensionTester):

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
        self.sender_task = asyncio.create_task(self.audio_sender(ten_env_tester))

    @override
    async def on_data(self, ten_env_tester: AsyncTenEnvTester, data: Data) -> None:
        final, _ = data.get_property_bool("final")
        text, _ = data.get_property_string("text")
        stream_id, _ = data.get_property_int("stream_id")
        user_id, _ = data.get_property_string("user_id")
        ten_env_tester.log_info(
            f"on_data, final: {final}, text: {text}, stream_id: {stream_id}, user_id: {user_id}"
        )
        ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


def test_basic():
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
    tester.run()
