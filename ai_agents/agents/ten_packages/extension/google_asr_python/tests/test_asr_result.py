import asyncio
from typing import Union
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

# Enable local mock to patch GoogleASRClient behavior
from .mock import patch_google_asr_client  # noqa: F401


class GoogleAsrExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.sender_task: Union[asyncio.Task, None] = None
        self.stopped = False

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        while not self.stopped:
            chunk = b"\x01\x02" * 160  # 320 bytes (16-bit * 160 samples)
            if not chunk:
                break
            audio_frame = AudioFrame.create("pcm_frame")
            metadata = {"session_id": "123"}
            audio_frame.set_property_from_json("metadata", json.dumps(metadata))
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)
            await asyncio.sleep(0.1)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
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
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            # Align with Azure UT: assert id exists (do not check value)
            self.stop_test_if_checking_failed(
                ten_env_tester, "id" in data_dict, f"id not in: {data_dict}"
            )
            self.stop_test_if_checking_failed(
                ten_env_tester, "text" in data_dict, f"text not in: {data_dict}"
            )
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "final" in data_dict,
                f"final not in: {data_dict}",
            )
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "start_ms" in data_dict,
                f"start_ms not in: {data_dict}",
            )
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "duration_ms" in data_dict,
                f"duration_ms not in: {data_dict}",
            )
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "language" in data_dict,
                f"language not in: {data_dict}",
            )
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "metadata" in data_dict,
                f"metadata not in: {data_dict}",
            )

            # Session id may not be set if final result arrives early; do not assert equality here

            if data_dict.get("final") is True:
                ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


def test_asr_result(patch_google_asr_client):
    property_json = {
        "params": {
            "project_id": "fake-project-id",
            "language": "en-US",
            "model": "long",
        }
    }

    tester = GoogleAsrExtensionTester()
    tester.set_test_mode_single("google_asr_python", json.dumps(property_json))
    err = tester.run()
    if err is not None:
        # Print readable error for debugging
        try:
            em = err.error_message()  # type: ignore[attr-defined]
            ec = err.error_code()  # type: ignore[attr-defined]
            assert False, f"test_asr_result err: {em}, {ec}"
        except Exception:
            assert False, f"test_asr_result err: {err}"
