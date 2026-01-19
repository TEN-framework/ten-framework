#!/usr/bin/env python3
"""
Test file for Utterance Processing Logic
Verify utterance processing logic: consecutive utterances with the same definite value are merged and concatenated
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
                f"start_ms={data_dict.get('start_ms')}, "
                f"duration_ms={data_dict.get('duration_ms')}, "
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

            # Verify processing logic
            # If final=False, metadata.asr_info should not contain utterance additions fields
            # (but may contain framework-added fields like vendor, invoke_type, source)
            # session_id is at metadata root level
            if not data_dict["final"]:
                metadata = data_dict.get("metadata", {})
                asr_info = metadata.get("asr_info", {})
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
                        field not in asr_info,
                        f"definite=False result should not contain '{field}' in metadata.asr_info, "
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
            # Verify text
            self.stop_test_if_checking_failed(
                ten_env_tester,
                received["text"] == expected["text"],
                f"Result {i}: expected text '{expected['text']}', "
                f"got '{received['text']}'",
            )

            # Verify final flag
            self.stop_test_if_checking_failed(
                ten_env_tester,
                received["final"] == expected["final"],
                f"Result {i}: expected final={expected['final']}, "
                f"got final={received['final']}",
            )

            # Verify duration_ms
            if "duration_ms" in expected:
                self.stop_test_if_checking_failed(
                    ten_env_tester,
                    received.get("duration_ms") == expected["duration_ms"],
                    f"Result {i}: expected duration_ms={expected['duration_ms']}, "
                    f"got {received.get('duration_ms')}",
                )

            # Verify start_ms (if specified, allow some tolerance for audio_timeline calculation)
            if "start_ms" in expected:
                received_start_ms = received.get("start_ms")
                expected_start_ms = expected["start_ms"]
                # Allow tolerance of ±100ms for audio_timeline calculation
                tolerance = 100
                self.stop_test_if_checking_failed(
                    ten_env_tester,
                    received_start_ms is not None
                    and abs(received_start_ms - expected_start_ms) <= tolerance,
                    f"Result {i}: expected start_ms≈{expected_start_ms} (±{tolerance}ms), "
                    f"got {received_start_ms}",
                )

            # For final=False, metadata.asr_info should not contain utterance additions fields
            # (but may contain framework-added fields like vendor, invoke_type, source)
            # session_id is at metadata root level
            # For final=True, we don't strictly check metadata content
            # (it may contain extracted fields from additions in asr_info)
            if not expected["final"]:
                metadata = received.get("metadata", {})
                asr_info = metadata.get("asr_info", {})
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
                        field not in asr_info,
                        f"Result {i}: final=False result should not contain '{field}' in metadata.asr_info, "
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


def test_utterance_grouping_enable(patch_volcengine_ws_grouping):  # type: ignore
    """Test utterance grouping and merging logic: consecutive utterances with same definite value are merged.

    Mock data has 6 utterances: [true, true, false, false, true, false]
    - "hello" (0-1000, true)
    - "world" (1000-2000, true)
    - "this" (2000-3000, false)
    - "is" (3000-4000, false)
    - "test" (4000-5000, true)
    - "example" (5000-6000, false)

    Expected merged results (4 groups):
    1. "helloworld" (0-2000, duration=2000, final=True) - merged [true, true]
    2. "thisis" (2000-4000, duration=2000, final=False) - merged [false, false]
    3. "test" (4000-5000, duration=1000, final=True) - single [true]
    4. "example" (5000-6000, duration=1000, final=False) - single [false]
    """

    property_json = {
        "params": {
            "app_key": "fake_app_key",
            "access_key": "fake_access_key",
            "sample_rate": 16000,
            "language": "zh-CN",
            "enable_utterance_grouping": True,
        }
    }

    tester = UtteranceGroupingTester()

    # Set expected results based on grouping and merging logic:
    # Consecutive utterances with same definite value are merged:
    # Group 1: [true, true] -> "helloworld" (start_time=0, end_time=2000, duration=2000)
    # Group 2: [false, false] -> "thisis" (start_time=2000, end_time=4000, duration=2000)
    # Group 3: [true] -> "test" (start_time=4000, end_time=5000, duration=1000)
    # Group 4: [false] -> "example" (start_time=5000, end_time=6000, duration=1000)
    tester.expected_results = [
        {
            "text": "helloworld",  # Merged from "hello" + "world"
            "final": True,
            "duration_ms": 2000,  # end_time(2000) - start_time(0)
            # "start_ms": 0,  # Will allow tolerance for audio_timeline calculation
            "metadata": {},  # Metadata from last utterance in group ("world")
        },
        {
            "text": "thisis",  # Merged from "this" + "is"
            "final": False,
            "duration_ms": 2000,  # end_time(4000) - start_time(2000)
            # "start_ms": 2000,  # Will allow tolerance for audio_timeline calculation
            "metadata": {},  # Metadata from last utterance in group ("is")
        },
        {
            "text": "test",  # Single utterance, not merged
            "final": True,
            "duration_ms": 1000,  # end_time(5000) - start_time(4000)
            # "start_ms": 4000,  # Will allow tolerance for audio_timeline calculation
            "metadata": {},  # Metadata from "test"
        },
        {
            "text": "example",  # Single utterance, not merged
            "final": False,
            "duration_ms": 1000,  # end_time(6000) - start_time(5000)
            # "start_ms": 5000,  # Will allow tolerance for audio_timeline calculation
            "metadata": {},  # Metadata from "example"
        },
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


def test_utterance_grouping_disable(patch_volcengine_ws_grouping):  # type: ignore
    """Test utterance processing logic: each utterance is sent individually [true, true, false, false, true, false]"""

    property_json = {
        "params": {
            "app_key": "fake_app_key",
            "access_key": "fake_access_key",
            "sample_rate": 16000,
            "language": "zh-CN",
            "enable_utterance_grouping": False,
        }
    }

    tester = UtteranceGroupingTester()

    # Set expected results based on the simplified processing logic:
    # [true, true, false, false, true, false]
    # Each utterance is sent individually:
    # Metadata structure: {"session_id": "...", "asr_info": {...}}
    # 1. "hello" (final=True, metadata.asr_info may contain speech_rate, volume)
    # 2. "world" (final=True, metadata.asr_info may contain speech_rate)
    # 3. "this" (final=False, metadata.asr_info should not contain utterance additions)
    # 4. "is" (final=False, metadata.asr_info should not contain utterance additions)
    # 5. "test" (final=True, metadata.asr_info may contain speech_rate, emotion)
    # 6. "example" (final=False, metadata.asr_info should not contain utterance additions)
    tester.expected_results = [
        {
            "text": "hello",
            "final": True,
            "metadata": {},
        },  # Will check text and final only
        {"text": "world", "final": True, "metadata": {}},
        {"text": "this", "final": False, "metadata": {}},
        {"text": "is", "final": False, "metadata": {}},
        {"text": "test", "final": True, "metadata": {}},
        {"text": "example", "final": False, "metadata": {}},
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
