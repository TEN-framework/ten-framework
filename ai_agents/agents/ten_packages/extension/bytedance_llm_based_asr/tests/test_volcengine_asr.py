#!/usr/bin/env python3
"""
Test file for Volcengine ASR LLM Extension
"""

import asyncio
import json
import threading
from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    AudioFrame,
    TenError,
    TenErrorCode,
)

# We must import it, which means this test fixture will be automatically executed
from .mock import patch_volcengine_ws  # noqa: F401


class BytedanceASRLLMExtensionTester(AsyncExtensionTester):
    """Extension tester for Bytedance ASR LLM."""

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped: bool = False

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        """Send audio frames to the extension."""
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
        """Start the audio sender task."""
        self.sender_task = asyncio.create_task(
            self.audio_sender(ten_env_tester)
        )

    def stop_test_if_checking_failed(
        self,
        ten_env_tester: AsyncTenEnvTester,
        success: bool,
        error_message: str,
    ) -> None:
        """Stop test if a check fails."""
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
        """Handle ASR result data."""
        data_name = data.get_name()
        if data_name == "asr_result":
            # Check the data structure.
            data_json, _ = data.get_property_to_json()
            data_dict: dict = json.loads(data_json)

            ten_env_tester.log_info(f"tester on_data, data_dict: {data_dict}")

            # Check required fields
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

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "metadata" in data_dict,
                f"metadata is not in data_dict: {data_dict}",
            )

            session_id: str = data_dict.get("metadata", {}).get(
                "session_id", ""
            )
            self.stop_test_if_checking_failed(
                ten_env_tester,
                session_id == "123",
                f"session_id is not 123: {session_id}",
            )

            if data_dict["final"] == True:
                ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        """Stop the audio sender task."""
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


def test_asr_result(patch_volcengine_ws):  # type: ignore
    """Test ASR result processing."""

    property_json = {
        "params": {
            "app_key": "fake_app_key",
            "access_key": "fake_access_key",
            "sample_rate": 16000,
            "language": "zh-CN",
        }
    }

    tester = BytedanceASRLLMExtensionTester()
    tester.set_test_mode_single(
        "bytedance_llm_based_asr", json.dumps(property_json)
    )
    err = tester.run()
    if err is not None:
        # Print readable error for debugging
        try:
            em = err.error_message()  # type: ignore[attr-defined]
            ec = err.error_code()  # type: ignore[attr-defined]
            assert False, f"test_asr_result err: {em}, {ec}"
        except Exception:
            assert False, f"test_asr_result err: {err}"
