#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
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

from .mock import patch_ezai_ws  # noqa: F401


class EZAIAsrExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        """Continuously send audio frames."""
        while not self.stopped:
            chunk = b"\x01\x02" * 160  # 320 bytes (16-bit * 160 samples)
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
            ten_env_tester.log_info(f"tester on_data: {data_dict}")

            # Verify all required fields
            required_fields = [
                "text",
                "final",
                "start_ms",
                "duration_ms",
                "language",
                "metadata",
            ]
            for field in required_fields:
                self.stop_test_if_checking_failed(
                    ten_env_tester,
                    field in data_dict,
                    f"{field} missing: {data_dict}",
                )

            session_id = data_dict.get("metadata", {}).get("session_id", "")
            self.stop_test_if_checking_failed(
                ten_env_tester,
                session_id == "123",
                f"session_id incorrect: {session_id}",
            )

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


def test_asr_result(patch_ezai_ws):
    """Test ASR result format."""
    property_json = {
        "params": {
            "key": "fake_key",
            "url": "wss://fake.ezai.example/v1/listen",
            "model": "nova-3",
            "language": "en-US",
            "sample_rate": 16000,
        },
    }

    tester = EZAIAsrExtensionTester()
    tester.set_test_mode_single("ezai_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_asr_result err: {err}"
