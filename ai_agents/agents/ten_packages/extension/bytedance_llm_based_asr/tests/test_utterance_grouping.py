#!/usr/bin/env python3
"""
Test file for Utterance Processing Logic
验证 utterances 处理逻辑：每个 utterance 单独发送，不再进行分组拼接
"""

import asyncio
import json
from typing import Any
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
from .mock import patch_volcengine_ws_grouping  # noqa: F401  # type: ignore


class UtteranceGroupingTester(AsyncExtensionTester):
    """Extension tester for testing utterance processing logic."""

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped: bool = False
        self.received_results: list[dict[str, Any]] = []
        self.expected_results: list[dict[str, Any]] = []

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
        """Handle ASR result data and verify processing logic."""
        data_name = data.get_name()
        if data_name == "asr_result":
            # Parse the data
            data_json, _ = data.get_property_to_json()
            data_dict: dict[str, Any] = json.loads(data_json)

            ten_env_tester.log_info(
                f"Received ASRResult: text='{data_dict.get('text')}', "
                f"final={data_dict.get('final')}, "
                f"metadata={data_dict.get('metadata')}"
            )

            # Store received result
            self.received_results.append(data_dict)

            # Check required fields
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
                "metadata" in data_dict,
                f"metadata is not in data_dict: {data_dict}",
            )

            # Verify processing logic
            # If final=False, metadata should not contain utterance additions fields
            # (but may contain framework-added fields like session_id)
            if not data_dict["final"]:
                metadata = data_dict.get("metadata", {})
                utterance_addition_fields = [
                    "speech_rate",
                    "volume",
                    "emotion",
                    "gender",
                    "lid_lang",
                ]
                for field in utterance_addition_fields:
                    self.stop_test_if_checking_failed(
                        ten_env_tester,
                        field not in metadata,
                        f"definite=False result should not contain '{field}' in metadata, "
                        f"got: {metadata}",
                    )

            # Check if we've received all expected results
            if len(self.received_results) >= len(self.expected_results):
                self.verify_results(ten_env_tester)
                ten_env_tester.stop_test()

    def verify_results(self, ten_env_tester: AsyncTenEnvTester) -> None:
        """Verify that received results match expected results."""
        ten_env_tester.log_info(
            f"Verifying results: received {len(self.received_results)}, "
            f"expected {len(self.expected_results)}"
        )

        self.stop_test_if_checking_failed(
            ten_env_tester,
            len(self.received_results) == len(self.expected_results),
            f"Expected {len(self.expected_results)} results, "
            f"got {len(self.received_results)}",
        )

        for i, (received, expected) in enumerate(
            zip(self.received_results, self.expected_results)
        ):
            self.stop_test_if_checking_failed(
                ten_env_tester,
                received["text"] == expected["text"],
                f"Result {i}: expected text '{expected['text']}', "
                f"got '{received['text']}'",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                received["final"] == expected["final"],
                f"Result {i}: expected final={expected['final']}, "
                f"got final={received['final']}",
            )

            # For final=False, metadata should not contain utterance additions fields
            # (but may contain framework-added fields like session_id)
            # For final=True, we don't strictly check metadata content
            # (it may contain extracted fields from additions)
            if not expected["final"]:
                metadata = received.get("metadata", {})
                utterance_addition_fields = [
                    "speech_rate",
                    "volume",
                    "emotion",
                    "gender",
                    "lid_lang",
                ]
                for field in utterance_addition_fields:
                    self.stop_test_if_checking_failed(
                        ten_env_tester,
                        field not in metadata,
                        f"Result {i}: final=False result should not contain '{field}' in metadata, "
                        f"got {metadata}",
                    )

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        """Stop the audio sender task."""
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


def test_utterance_grouping(patch_volcengine_ws_grouping):  # type: ignore
    """Test utterance processing logic: each utterance is sent individually [true, true, false, false, true, false]"""

    property_json = {
        "params": {
            "app_key": "fake_app_key",
            "access_key": "fake_access_key",
            "sample_rate": 16000,
            "language": "zh-CN",
        }
    }

    tester = UtteranceGroupingTester()

    # Set expected results based on the simplified processing logic:
    # [true, true, false, false, true, false]
    # Each utterance is sent individually:
    # 1. "你好" (final=True, metadata may contain speech_rate, volume)
    # 2. "世界" (final=True, metadata may contain speech_rate)
    # 3. "这是" (final=False, metadata={})
    # 4. "一个" (final=False, metadata={})
    # 5. "测试" (final=True, metadata may contain speech_rate, emotion)
    # 6. "示例" (final=False, metadata={})
    tester.expected_results = [
        {
            "text": "你好",
            "final": True,
            "metadata": {},
        },  # Will check text and final only
        {"text": "世界", "final": True, "metadata": {}},
        {"text": "这是", "final": False, "metadata": {}},
        {"text": "一个", "final": False, "metadata": {}},
        {"text": "测试", "final": True, "metadata": {}},
        {"text": "示例", "final": False, "metadata": {}},
    ]

    tester.set_test_mode_single(
        "bytedance_llm_based_asr", json.dumps(property_json)
    )

    err = tester.run()
    if err is not None:
        # Print readable error for debugging
        try:
            em = err.error_message()  # type: ignore[attr-defined]
            ec = err.error_code()  # type: ignore[attr-defined]
            assert False, f"test_utterance_grouping err: {em}, {ec}"
        except Exception:
            assert False, f"test_utterance_grouping err: {err}"

    # Verify we got the expected number of results
    assert len(tester.received_results) == len(
        tester.expected_results
    ), f"Expected {len(tester.expected_results)} results, got {len(tester.received_results)}"
