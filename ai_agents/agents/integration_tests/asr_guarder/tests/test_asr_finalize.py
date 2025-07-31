#!/usr/bin/env python3
#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

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
import json
import asyncio
import os
from .id_group_manager import AsrIdGroupManager, AsrResultGroup


# Constants for audio configuration
AUDIO_CHUNK_SIZE = 320
AUDIO_SAMPLE_RATE = 16000
FRAME_INTERVAL_MS = 10

# Constants for test configuration
ASR_FINALIZE_CONFIG_FILE = "property_en.json"
ASR_FINALIZE_SESSION_ID = "test_asr_finalize_session_123"
ASR_FINALIZE_EXPECTED_LANGUAGE = "en-US"


class AsrFinalizeTester(AsyncExtensionTester):
    """Test class for ASR extension finalize testing."""

    def __init__(
        self,
        audio_file_path: str,
        session_id: str = ASR_FINALIZE_SESSION_ID,
        expected_language: str = ASR_FINALIZE_EXPECTED_LANGUAGE,
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: ASR Finalize Test")
        print("=" * 80)
        print(
            "📋 Test Description: Validate ASR extension finalize functionality"
        )
        print("🎯 Test Objectives:")
        print("   - Verify ASR extension can process audio and return results")
        print("   - Test active sending of asr_finalize signal")
        print("   - Validate final result has final=True")
        print("   - Check asr_finalize_end signal is received")
        print("   - Verify finalize_id consistency in finalize_end")
        print("   - Validate session_id consistency in finalize_end")
        print("   - Test finalize response timing")
        print("=" * 80)

        self.audio_file_path: str = audio_file_path
        self.session_id: str = session_id
        self.expected_language: str = expected_language
        # Track state for single audio send
        self.final_id: str | None = None
        self.waiting_for_final: bool = False

        # Track ASR results using ID group manager
        self.id_group_manager = AsrIdGroupManager()

        # Track finalize state
        self.finalize_id: str | None = None
        self.finalize_end_received: bool = False
        self.finalize_end_id: str | None = None
        self.finalize_end_session_id: str | None = None

    def _create_audio_frame(self, data: bytes, session_id: str) -> AudioFrame:
        """Create an audio frame with the given data and session ID."""
        audio_frame = AudioFrame.create("pcm_frame")

        # Set session_id in metadata according to API specification
        metadata = {"session_id": session_id}
        audio_frame.set_property_from_json("metadata", json.dumps(metadata))

        audio_frame.alloc_buf(len(data))
        buf = audio_frame.lock_buf()
        buf[:] = data
        audio_frame.unlock_buf(buf)
        return audio_frame

    async def _send_audio_file(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio file data to ASR extension."""
        ten_env.log_info(f"Sending audio file: {self.audio_file_path}")

        with open(self.audio_file_path, "rb") as audio_file:
            while True:
                chunk = audio_file.read(AUDIO_CHUNK_SIZE)
                if not chunk:
                    break

                audio_frame = self._create_audio_frame(chunk, self.session_id)
                await ten_env.send_audio_frame(audio_frame)
                await asyncio.sleep(FRAME_INTERVAL_MS / 1000)

    async def _send_finalize_signal(self, ten_env: AsyncTenEnvTester) -> None:
        """Send asr_finalize signal to trigger finalization."""
        ten_env.log_info("Sending asr_finalize signal...")

        # Create finalize data according to protocol
        finalize_data = {
            "finalize_id": f"finalize_{self.session_id}_{int(asyncio.get_event_loop().time())}",
            "metadata": {"session_id": self.session_id},
        }

        # Create Data object for asr_finalize
        finalize_data_obj = Data.create("asr_finalize")
        finalize_data_obj.set_property_from_json(
            None, json.dumps(finalize_data)
        )

        # Send the finalize signal
        await ten_env.send_data(finalize_data_obj)

        # Store the finalize_id for validation
        self.finalize_id = str(finalize_data["finalize_id"])

        ten_env.log_info(
            f"✅ asr_finalize signal sent with ID: {self.finalize_id}"
        )

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio data and silence packets to ASR extension once."""
        try:
            # Send audio file
            ten_env.log_info("=== Starting audio send ===")
            await self._send_audio_file(ten_env)
            # await self._send_silence_packets(ten_env)

            # Wait 1.5 seconds after sending audio
            ten_env.log_info("=== Waiting 1.5 seconds after audio send ===")
            await asyncio.sleep(1.5)

            # Send finalize signal after audio send
            ten_env.log_info("=== Sending finalize signal ===")
            await self._send_finalize_signal(ten_env)

            # Wait 1.5 seconds after sending finalize signal
            ten_env.log_info(
                "=== Waiting 1.5 seconds after finalize signal ==="
            )
            await asyncio.sleep(1.5)

            # Send additional silence packets after finalize
            ten_env.log_info(
                "=== Sending additional silence packets after finalize ==="
            )
            # await self._send_silence_packets(ten_env)

            # Wait for final result
            self.waiting_for_final = True
            ten_env.log_info("Waiting for final ASR result...")
            await asyncio.sleep(2)  # Give some time for processing

        except Exception as e:
            ten_env.log_error(f"Error in audio sender: {e}")
            raise

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the ASR finalize test with two audio sends."""
        ten_env.log_info("Starting ASR finalize test with two audio sends")
        await self.audio_sender(ten_env)

    def _stop_test_with_error(
        self, ten_env: AsyncTenEnvTester, error_message: str
    ) -> None:
        """Stop test with error message."""
        ten_env.stop_test(
            TenError.create(TenErrorCode.ErrorCodeGeneric, error_message)
        )

    def _log_asr_result_structure(
        self,
        ten_env: AsyncTenEnvTester,
        json_str: str,
        metadata: Any,
    ) -> None:
        """Log complete ASR result structure for debugging."""
        ten_env.log_info("=" * 80)
        ten_env.log_info("RECEIVED ASR RESULT - COMPLETE STRUCTURE:")
        ten_env.log_info("=" * 80)
        ten_env.log_info(f"Raw JSON string: {json_str}")
        ten_env.log_info(f"Metadata: {metadata}")
        ten_env.log_info(f"Metadata type: {type(metadata)}")
        ten_env.log_info("=" * 80)

    def _validate_required_fields(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that all required fields exist in ASR result."""
        required_fields = [
            "id",
            "text",
            "final",
            "start_ms",
            "duration_ms",
            "language",
        ]
        missing_fields = [
            field for field in required_fields if field not in json_data
        ]

        if missing_fields:
            self._stop_test_with_error(
                ten_env, f"Missing required fields: {missing_fields}"
            )
            return False
        return True

    def _validate_language(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate language matches expected language."""
        language: str = json_data.get("language", "")
        if language != self.expected_language:
            self._stop_test_with_error(
                ten_env,
                f"Language mismatch, expected: {self.expected_language}, actual: {language}",
            )
            return False
        return True

    def _validate_session_id(
        self, ten_env: AsyncTenEnvTester, metadata: dict[str, Any] | None
    ) -> bool:
        """Validate session_id in metadata."""
        if (
            not metadata
            or not isinstance(metadata, dict)
            or "session_id" not in metadata
        ):
            self._stop_test_with_error(
                ten_env, "Missing session_id field in metadata"
            )
            return False

        actual_session_id: str = metadata["session_id"]
        if actual_session_id != self.session_id:
            self._stop_test_with_error(
                ten_env,
                f"session_id mismatch, expected: {self.session_id}, actual: {actual_session_id}",
            )
            return False
        return True

    def _validate_final_result(
        self,
        ten_env: AsyncTenEnvTester,
        json_data: dict[str, Any],
        metadata: dict[str, Any] | None,
    ) -> bool:
        """Validate all fields for final ASR result."""
        validations = [
            lambda: self._validate_language(ten_env, json_data),
            lambda: self._validate_session_id(ten_env, metadata),
        ]

        return all(validation() for validation in validations)

    def _validate_non_final_and_final_results(
        self, ten_env: AsyncTenEnvTester
    ) -> bool:
        """Validate non-final and final results using ID group manager."""
        # Get complete groups
        complete_groups = self.id_group_manager.get_complete_groups()

        if not complete_groups:
            self._stop_test_with_error(ten_env, "No complete groups received")
            return False

        ten_env.log_info(f"✅ Received {len(complete_groups)} complete groups")

        # Validate each complete group
        for i, group in enumerate(complete_groups):
            ten_env.log_info(
                f"Validating group {i}: {len(group.non_final_results)} non-final, 1 final"
            )

            # Validate data format consistency for this group
            if not self._validate_group_data_format_consistency(ten_env, group):
                return False

        return True

    def _validate_group_data_format_consistency(
        self, ten_env: AsyncTenEnvTester, group: AsrResultGroup
    ) -> bool:
        """Validate that a group has consistent data format."""
        required_fields = [
            "id",
            "text",
            "final",
            "start_ms",
            "duration_ms",
            "language",
        ]

        # Check final result format
        if group.final_result is None:
            self._stop_test_with_error(
                ten_env, f"Group {group.group_id} final result is None"
            )
            return False

        for field in required_fields:
            if field not in group.final_result:
                self._stop_test_with_error(
                    ten_env,
                    f"Group {group.group_id} final result missing required field: {field}",
                )
                return False

        # Check non-final results format
        for i, non_final in enumerate(group.non_final_results):
            for field in required_fields:
                if field not in non_final:
                    self._stop_test_with_error(
                        ten_env,
                        f"Group {group.group_id} non-final result {i} missing required field: {field}",
                    )
                    return False

        ten_env.log_info(
            f"✅ Group {group.group_id} data format consistency validated"
        )
        return True

    def _validate_data_format_consistency(
        self, ten_env: AsyncTenEnvTester
    ) -> bool:
        """Validate that all groups have consistent data format."""
        complete_groups = self.id_group_manager.get_complete_groups()

        for group in complete_groups:
            if not self._validate_group_data_format_consistency(ten_env, group):
                return False

        ten_env.log_info(
            "✅ Data format consistency validated - all groups have required fields"
        )
        return True

    def _validate_id_consistency(self, ten_env: AsyncTenEnvTester) -> bool:
        """Validate that all groups have consistent IDs within each group."""
        complete_groups = self.id_group_manager.get_complete_groups()

        for group in complete_groups:
            if group.final_result is None:
                self._stop_test_with_error(
                    ten_env, f"Group {group.group_id} final result is None"
                )
                return False

            group_id = group.final_result.get("id", "")

            for i, non_final in enumerate(group.non_final_results):
                non_final_id = non_final.get("id", "")
                if non_final_id != group_id:
                    self._stop_test_with_error(
                        ten_env,
                        f"Group {group.group_id} ID inconsistency: Non-final result {i} has id '{non_final_id}' but final result has id '{group_id}'",
                    )
                    return False

        ten_env.log_info(
            f"✅ ID consistency validated - all groups have consistent IDs"
        )
        return True

    def _validate_finalize_end(
        self, ten_env: AsyncTenEnvTester, data: Data
    ) -> bool:
        """Validate asr_finalize_end signal."""
        ten_env.log_info("Validating asr_finalize_end signal...")

        # Parse finalize_end data
        json_str, _ = data.get_property_to_json(None)
        finalize_end_data: dict[str, Any] = json.loads(json_str)

        # Extract finalize_id from end signal
        finalize_end_id = finalize_end_data.get("finalize_id")
        metadata = finalize_end_data.get("metadata", {})
        finalize_end_session_id = (
            metadata.get("session_id") if metadata else None
        )

        # Validate finalize_id matches the one we sent
        if self.finalize_id is None:
            self._stop_test_with_error(
                ten_env, "No finalize_id stored for comparison"
            )
            return False

        if finalize_end_id != self.finalize_id:
            self._stop_test_with_error(
                ten_env,
                f"Finalize ID mismatch - expected: {self.finalize_id}, actual: {finalize_end_id}",
            )
            return False

        # Validate session_id matches
        if finalize_end_session_id != self.session_id:
            self._stop_test_with_error(
                ten_env,
                f"Finalize session_id mismatch - expected: {self.session_id}, actual: {finalize_end_session_id}",
            )
            return False

        # Store for later validation
        self.finalize_end_id = finalize_end_id
        self.finalize_end_session_id = finalize_end_session_id
        self.finalize_end_received = True

        ten_env.log_info(
            f"✅ asr_finalize_end validation passed - ID: {finalize_end_id}, session_id: {finalize_end_session_id}"
        )
        return True

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from ASR extension."""
        name: str = data.get_name()

        if name == "asr_finalize_end":
            """Handle asr_finalize_end signal."""
            ten_env.log_info("Received asr_finalize_end signal")

            if self._validate_finalize_end(ten_env, data):
                ten_env.log_info("✅ asr_finalize_end validation completed")
                # Check if we already have final result, then we can stop test
                if self.id_group_manager.get_complete_groups_count() > 0:
                    ten_env.log_info(
                        "✅ ASR finalize test passed with finalize validation"
                    )
                    ten_env.stop_test()
            return
        elif name == "asr_result":
            """Handle asr_result data."""
            # Parse ASR result
            json_str, _ = data.get_property_to_json(None)
            json_data: dict[str, Any] = json.loads(json_str)
        else:
            ten_env.log_info(f"Received non-ASR data: {name}")
            return

        # Validate required fields first
        if not self._validate_required_fields(ten_env, json_data):
            return

        # Check if this is a final result
        is_final: bool = json_data.get("final", False)
        result_id: str = json_data.get("id", "")
        ten_env.log_info(
            f"Received ASR result - final: {is_final}, id: {result_id}"
        )

        # Add result to ID group manager
        group_id = self.id_group_manager.add_result(json_data)
        # ten_env.log_info(f"Added result to group: {group_id}")

        # Get the group this result was added to
        group = self.id_group_manager.get_group_by_id(group_id)
        if group:
            ten_env.log_info(
                f"Group {group_id}: {len(group.non_final_results)} non-final, "
                f"final: {group.final_result is not None}"
            )

        # If this is a final result, validate it
        if is_final:
            # Log complete structure for final result
            ten_env.log_info("Received final ASR result, validating...")
            self._log_asr_result_structure(
                ten_env, json_str, json_data.get("metadata")
            )

            # Validate final result - metadata is part of json_data according to API spec
            metadata_dict: dict[str, Any] | None = json_data.get("metadata")
            if not self._validate_final_result(
                ten_env, json_data, metadata_dict
            ):
                return

            # Validate group consistency
            if not self.id_group_manager.validate_group_consistency():
                self._stop_test_with_error(
                    ten_env, "ID group consistency validation failed"
                )
                return

            # Log group summary
            summary = self.id_group_manager.get_group_summary()
            ten_env.log_info(f"Group summary: {summary}")

            # Check if we have received finalize_end signal
            if self.finalize_end_received:
                ten_env.log_info(
                    "✅ ASR finalize test passed with finalize validation"
                )
                ten_env.stop_test()
            else:
                ten_env.log_info("Waiting for asr_finalize_end signal...")

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""
        ten_env.log_info("Test stopped")


def test_asr_finalize(extension_name: str, config_dir: str) -> None:
    """Verify ASR extension finalize functionality with single audio send and finalize validation."""

    # Audio file path
    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data/16k_en_us.pcm"
    )

    # Get config file path
    config_file_path = os.path.join(config_dir, ASR_FINALIZE_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Expected test results
    expected_result = {
        "language": ASR_FINALIZE_EXPECTED_LANGUAGE,
        "session_id": ASR_FINALIZE_SESSION_ID,
    }

    # Log test configuration
    print(f"Using test configuration: {config}")
    print(f"Audio file path: {audio_file_path}")
    print(
        f"Expected results: language='{expected_result['language']}', session_id='{expected_result['session_id']}'"
    )
    print("Finalize validation requirements:")
    print("  1. Send asr_finalize signal after audio send")
    print("  2. Receive final result with final=True")
    print("  3. Output asr_finalize_end signal")
    print("  4. Finalize_end must validate finalize_id and session_id")

    # Create and run tester
    tester = AsrFinalizeTester(
        audio_file_path=audio_file_path,
        session_id=expected_result["session_id"],
        expected_language=expected_result["language"],
    )

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"
