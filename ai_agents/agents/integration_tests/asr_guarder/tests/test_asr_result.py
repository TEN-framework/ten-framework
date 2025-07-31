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


# Constants for audio configuration
AUDIO_CHUNK_SIZE = 320
AUDIO_SAMPLE_RATE = 16000
FRAME_INTERVAL_MS = 10

# Constants for test configuration
DEFAULT_CONFIG_FILE = "property_en.json"
DEFAULT_SESSION_ID = "test_asr_result_session_123"
DEFAULT_EXPECTED_LANGUAGE = "en-US"


class AsrExtensionTester(AsyncExtensionTester):
    """Test class for ASR extension integration testing."""

    def __init__(
        self,
        audio_file_path: str,
        session_id: str = DEFAULT_SESSION_ID,
        expected_language: str = DEFAULT_EXPECTED_LANGUAGE,
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: ASR Result Integration Test")
        print("=" * 80)
        print(
            "📋 Test Description: Validate ASR extension result processing and consistency"
        )
        print("🎯 Test Objectives:")
        print("   - Verify ASR extension processes audio and returns results")
        print("   - Validate required fields in ASR results")
        print("   - Check language detection accuracy")
        print("   - Ensure session ID consistency")
        print("   - Validate ID consistency across multiple audio sends")
        print("   - Test multiple audio send scenarios")
        print("=" * 80)

        self.audio_file_path: str = audio_file_path
        self.session_id: str = session_id
        self.expected_language: str = expected_language
        # Track state for single audio send
        self.final_id: str | None = None

        # Track final results for validation
        self.final_result: dict[str, Any] | None = None

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

    async def _send_finalize_signal(
        self, ten_env: AsyncTenEnvTester, session_id: str | None = None
    ) -> None:
        """Send asr_finalize signal to trigger finalization."""
        ten_env.log_info("Sending asr_finalize signal...")

        # Use provided session_id or default to self.session_id
        target_session_id = session_id if session_id else self.session_id

        # Create finalize data according to protocol
        finalize_data = {
            "finalize_id": f"finalize_{target_session_id}_{int(asyncio.get_event_loop().time())}",
            "metadata": {"session_id": target_session_id},
        }

        # Create Data object for asr_finalize
        finalize_data_obj = Data.create("asr_finalize")
        finalize_data_obj.set_property_from_json(
            None, json.dumps(finalize_data)
        )

        # Send the finalize signal
        await ten_env.send_data(finalize_data_obj)

        ten_env.log_info(
            f"✅ asr_finalize signal sent with ID: {finalize_data['finalize_id']}"
        )

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio data to ASR extension."""
        try:
            # Send audio
            ten_env.log_info("=== Starting audio send ===")
            ten_env.log_info(f"Using session_id: {self.session_id}")

            await self._send_audio_file(ten_env)

            # Wait 1.5 seconds after sending audio
            ten_env.log_info("=== Waiting 1.5 seconds after audio send ===")
            await asyncio.sleep(1.5)

            # Send finalize signal
            ten_env.log_info("=== Sending finalize signal ===")
            await self._send_finalize_signal(ten_env)

            # Wait for final result
            ten_env.log_info("=== Waiting for final result ===")
            await asyncio.sleep(2.0)  # Give time for processing

        except Exception as e:
            ten_env.log_error(f"Error in audio sender: {e}")
            raise

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the ASR integration test with one audio send."""
        ten_env.log_info("Starting ASR integration test with one audio send")
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
        expected_session_id = self.session_id

        if actual_session_id != expected_session_id:
            self._stop_test_with_error(
                ten_env,
                f"session_id mismatch, expected: {expected_session_id}, actual: {actual_session_id}",
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

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from ASR extension."""
        name: str = data.get_name()

        if name != "asr_result":
            ten_env.log_info(f"Received non-ASR data: {name}")
            return

        # Parse ASR result
        json_str, _ = data.get_property_to_json(None)
        json_data: dict[str, Any] = json.loads(json_str)

        # Validate required fields first
        if not self._validate_required_fields(ten_env, json_data):
            return

        # Check if this is a final result
        is_final: bool = json_data.get("final", False)
        result_id: str = json_data.get("id", "")
        ten_env.log_info(
            f"Received ASR result - final: {is_final}, id: {result_id}"
        )

        # Only process final results
        if not is_final:
            ten_env.log_info(f"Received intermediate result, continuing...")
            return

        # Process final results
        if self.final_id is None:
            self.final_id = result_id
            self.final_result = json_data
            ten_env.log_info(
                f"✅ Final ASR result received with id: {result_id}"
            )

            # Log complete structure for final result
            ten_env.log_info("Received final ASR result, validating...")
            self._log_asr_result_structure(
                ten_env, json_str, json_data.get("metadata")
            )

            # Validate final result
            metadata_dict: dict[str, Any] | None = json_data.get("metadata")
            if not self._validate_final_result(
                ten_env, json_data, metadata_dict
            ):
                return

            ten_env.log_info("✅ ASR integration test passed")
            ten_env.stop_test()
        else:
            ten_env.log_info(
                f"Received additional final result with id: {result_id}"
            )

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""
        ten_env.log_info("Test stopped")


def test_asr_result(extension_name: str, config_dir: str) -> None:
    """Verify ASR extension processes one audio send and returns a final ID."""

    # Audio file path
    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data/16k_en_us.pcm"
    )

    # Get config file path
    config_file_path = os.path.join(config_dir, DEFAULT_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Expected test results
    expected_result = {
        "language": DEFAULT_EXPECTED_LANGUAGE,
        "session_id": DEFAULT_SESSION_ID,
    }

    # Log test configuration
    print(f"Using test configuration: {config}")
    print(f"Audio file path: {audio_file_path}")
    print(
        f"Expected results: language='{expected_result['language']}', session_id='{expected_result['session_id']}'"
    )

    # Store results from two test runs
    first_result_id: str | None = None
    second_result_id: str | None = None

    # First test run
    print("\n" + "=" * 60)
    print("First audio send test")
    print("=" * 60)

    tester1 = AsrExtensionTester(
        audio_file_path=audio_file_path,
        session_id=expected_result["session_id"],
        expected_language=expected_result["language"],
    )

    tester1.set_test_mode_single(extension_name, json.dumps(config))
    error1 = tester1.run()

    # Verify first test results
    assert (
        error1 is None
    ), f"First test failed: {error1.error_message() if error1 else 'Unknown error'}"

    # Get first result ID
    if tester1.final_id:
        first_result_id = tester1.final_id
        print(f"✅ First test passed with ID: {first_result_id}")
    else:
        raise AssertionError("First test did not produce a final ID")

    # Second test run
    print("\n" + "=" * 60)
    print("Second audio send test")
    print("=" * 60)

    tester2 = AsrExtensionTester(
        audio_file_path=audio_file_path,
        session_id=f"{expected_result['session_id']}_second",
        expected_language=expected_result["language"],
    )

    tester2.set_test_mode_single(extension_name, json.dumps(config))
    error2 = tester2.run()

    # Verify second test results
    assert (
        error2 is None
    ), f"Second test failed: {error2.error_message() if error2 else 'Unknown error'}"

    # Get second result ID
    if tester2.final_id:
        second_result_id = tester2.final_id
        print(f"✅ Second test passed with ID: {second_result_id}")
    else:
        raise AssertionError("Second test did not produce a final ID")

    # Compare the two IDs
    if first_result_id == second_result_id:
        raise AssertionError(
            f"ID validation failed: Both tests have the same id '{first_result_id}'"
        )
    else:
        print(
            f"✅ ID validation passed: First id '{first_result_id}' != Second id '{second_result_id}'"
        )
        print("✅ ASR integration test passed with two different final results")
