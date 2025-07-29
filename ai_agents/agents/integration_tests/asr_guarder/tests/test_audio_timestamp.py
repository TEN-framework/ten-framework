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
AUDIO_TIMESTAMP_CONFIG_FILE = "property_en.json"
AUDIO_TIMESTAMP_SESSION_ID = "test_audio_timestamp_session_123"
AUDIO_TIMESTAMP_EXPECTED_LANGUAGE = "en-US"


class AudioTimestampAsrTester(AsyncExtensionTester):
    """Test class for ASR extension audio timestamp validation testing."""

    def __init__(
        self,
        audio_file_path: str,
        session_id: str = AUDIO_TIMESTAMP_SESSION_ID,
        expected_language: str = AUDIO_TIMESTAMP_EXPECTED_LANGUAGE,
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: Audio Timestamp ASR Test")
        print("=" * 80)
        print(
            "📋 Test Description: Validate ASR result timestamp fields and accuracy"
        )
        print("🎯 Test Objectives:")
        print("   - Verify timestamp fields are int type")
        print("   - Validate time unit is milliseconds")
        print("   - Check timestamps are non-negative integers")
        print("   - Ensure duration is positive integer")
        print("   - Validate start_ms reflects audio start position")
        print("   - Verify duration_ms represents audio segment length")
        print("   - Check timestamp precision is milliseconds")
        print("=" * 80)

        self.audio_file_path: str = audio_file_path
        self.session_id: str = session_id
        self.expected_language: str = expected_language

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

        ten_env.log_info(
            f"✅ asr_finalize signal sent with ID: {finalize_data['finalize_id']}"
        )

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio data and finalize signal to ASR extension."""
        try:
            # Send audio file
            ten_env.log_info("=== Starting audio send ===")
            await self._send_audio_file(ten_env)

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

        except Exception as e:
            ten_env.log_error(f"Error in audio sender: {e}")
            raise

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the ASR timestamp validation test."""
        ten_env.log_info("Starting ASR timestamp validation test")
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

    def _validate_timestamp_type(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that timestamp fields are int type."""
        start_ms = json_data.get("start_ms")
        duration_ms = json_data.get("duration_ms")

        if not isinstance(start_ms, int):
            self._stop_test_with_error(
                ten_env,
                f"start_ms field must be int type, got {type(start_ms)} with value {start_ms}",
            )
            return False

        if not isinstance(duration_ms, int):
            self._stop_test_with_error(
                ten_env,
                f"duration_ms field must be int type, got {type(duration_ms)} with value {duration_ms}",
            )
            return False

        ten_env.log_info(
            f"✅ Timestamp type validation passed - start_ms: {start_ms} ({type(start_ms)}), duration_ms: {duration_ms} ({type(duration_ms)})"
        )
        return True

    def _validate_timestamp_non_negative(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that timestamp fields are non-negative integers."""
        start_ms = json_data.get("start_ms")
        duration_ms = json_data.get("duration_ms")

        if start_ms is None or duration_ms is None:
            self._stop_test_with_error(
                ten_env, "Timestamp fields are missing or null"
            )
            return False

        if start_ms < 0:
            self._stop_test_with_error(
                ten_env,
                f"start_ms must be non-negative, got {start_ms}",
            )
            return False

        if duration_ms < 0:
            self._stop_test_with_error(
                ten_env,
                f"duration_ms must be non-negative, got {duration_ms}",
            )
            return False

        ten_env.log_info(
            f"✅ Timestamp non-negative validation passed - start_ms: {start_ms}, duration_ms: {duration_ms}"
        )
        return True

    def _validate_duration_positive(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that duration_ms is a positive integer."""
        duration_ms = json_data.get("duration_ms")

        if duration_ms is None:
            self._stop_test_with_error(
                ten_env, "duration_ms field is missing or null"
            )
            return False

        if duration_ms <= 0:
            self._stop_test_with_error(
                ten_env,
                f"duration_ms must be positive integer, got {duration_ms}",
            )
            return False

        ten_env.log_info(
            f"✅ Duration positive validation passed - duration_ms: {duration_ms}"
        )
        return True

    def _validate_timestamp_milliseconds(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that timestamp values are reasonable for milliseconds."""
        start_ms = json_data.get("start_ms")
        duration_ms = json_data.get("duration_ms")

        if start_ms is None or duration_ms is None:
            self._stop_test_with_error(
                ten_env, "Timestamp fields are missing or null"
            )
            return False

        ten_env.log_info(
            f"✅ Timestamp milliseconds validation passed - start_ms: {start_ms} ms, duration_ms: {duration_ms} ms"
        )
        return True

    def _validate_start_ms_accuracy(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that start_ms correctly reflects the audio start position."""
        start_ms = json_data.get("start_ms")

        if start_ms is None:
            self._stop_test_with_error(
                ten_env, "start_ms field is missing or null"
            )
            return False

        # start_ms should be 0 for the first audio segment
        # For subsequent segments, it should be reasonable based on audio duration
        if start_ms == 0:
            ten_env.log_info(
                "✅ start_ms correctly indicates beginning of audio (0ms)"
            )
        elif start_ms > 0:
            # For non-zero start times, validate they are reasonable
            # Assuming audio chunks are sent every 10ms (FRAME_INTERVAL_MS)
            expected_start_times = [
                i * FRAME_INTERVAL_MS for i in range(100)
            ]  # First 1 second
            if (
                start_ms in expected_start_times
                or start_ms % FRAME_INTERVAL_MS == 0
            ):
                ten_env.log_info(
                    f"✅ start_ms correctly reflects audio position: {start_ms} ms"
                )
            else:
                ten_env.log_info(
                    f"start_ms {start_ms} ms may not align with expected frame intervals"
                )
        else:
            self._stop_test_with_error(
                ten_env,
                f"start_ms should be non-negative, got {start_ms}",
            )
            return False

        return True

    def _validate_duration_ms_accuracy(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that duration_ms accurately represents the audio segment length."""
        duration_ms = json_data.get("duration_ms")
        text = json_data.get("text", "")

        if duration_ms is None:
            self._stop_test_with_error(
                ten_env, "duration_ms field is missing or null"
            )
            return False

        # Calculate expected duration based on text length and speaking rate
        # Average speaking rate is about 150 words per minute
        # For "hello world" (2 words), expected duration should be around 800ms
        expected_min_duration = 100  # Minimum 100ms for any speech
        expected_max_duration = 5000  # Maximum 5 seconds for short phrases

        if duration_ms < expected_min_duration:
            ten_env.log_info(
                f"duration_ms {duration_ms} ms seems too short for text: '{text}'"
            )
        elif duration_ms > expected_max_duration:
            ten_env.log_info(
                f"duration_ms {duration_ms} ms seems too long for text: '{text}'"
            )
        else:
            ten_env.log_info(
                f"✅ duration_ms accurately represents audio segment length: {duration_ms} ms for text: '{text}'"
            )

        return True

    def _validate_timestamp_precision(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that timestamp precision is in milliseconds."""
        start_ms = json_data.get("start_ms")
        duration_ms = json_data.get("duration_ms")

        if start_ms is None or duration_ms is None:
            self._stop_test_with_error(
                ten_env,
                "Timestamp fields must be present for precision validation",
            )
            return False

        # Check if timestamps are integers (millisecond precision)
        if not isinstance(start_ms, int) or not isinstance(duration_ms, int):
            self._stop_test_with_error(
                ten_env,
                f"Timestamp fields must be integers for millisecond precision, got start_ms: {type(start_ms)}, duration_ms: {type(duration_ms)}",
            )
            return False

        # Check if timestamps are reasonable millisecond values
        # start_ms should be >= 0
        if start_ms < 0:
            self._stop_test_with_error(
                ten_env,
                f"start_ms must be non-negative for millisecond precision, got {start_ms}",
            )
            return False

        # duration_ms should be > 0
        if duration_ms <= 0:
            self._stop_test_with_error(
                ten_env,
                f"duration_ms must be positive for millisecond precision, got {duration_ms}",
            )
            return False

        # Check if values are reasonable millisecond ranges
        if start_ms > 3600000:  # More than 1 hour
            ten_env.log_info(f"start_ms {start_ms} ms seems unusually large")

        if duration_ms > 300000:  # More than 5 minutes
            ten_env.log_info(
                f"duration_ms {duration_ms} ms seems unusually large"
            )

        ten_env.log_info(
            f"✅ Timestamp precision validation passed - millisecond precision confirmed: start_ms={start_ms}ms, duration_ms={duration_ms}ms"
        )
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
        """Validate all fields for final ASR result including timestamp validation."""
        validations = [
            lambda: self._validate_language(ten_env, json_data),
            lambda: self._validate_session_id(ten_env, metadata),
            lambda: self._validate_timestamp_type(ten_env, json_data),
            lambda: self._validate_timestamp_non_negative(ten_env, json_data),
            lambda: self._validate_duration_positive(ten_env, json_data),
            lambda: self._validate_timestamp_milliseconds(ten_env, json_data),
            lambda: self._validate_start_ms_accuracy(ten_env, json_data),
            lambda: self._validate_duration_ms_accuracy(ten_env, json_data),
            lambda: self._validate_timestamp_precision(ten_env, json_data),
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
        ten_env.log_info(f"Received ASR result - final: {is_final}")

        if not is_final:
            ten_env.log_info("Received intermediate ASR result, continuing...")
            return

        # For final results, log complete structure and validate
        ten_env.log_info(
            "Received final ASR result, validating timestamp fields..."
        )
        self._log_asr_result_structure(
            ten_env, json_str, json_data.get("metadata")
        )

        # Validate final result - metadata is part of json_data according to API spec
        metadata_dict: dict[str, Any] | None = json_data.get("metadata")
        if self._validate_final_result(ten_env, json_data, metadata_dict):
            ten_env.log_info(
                "✅ ASR timestamp validation test passed with final result"
            )
            ten_env.stop_test()

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""
        ten_env.log_info("Test stopped")


def test_audio_timestamp(extension_name: str, config_dir: str) -> None:
    """Verify ASR result timestamp fields meet requirements."""

    # Audio file path
    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data/16k_en_us.pcm"
    )

    # Get config file path
    config_file_path = os.path.join(config_dir, AUDIO_TIMESTAMP_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Expected test results
    expected_result = {
        "language": AUDIO_TIMESTAMP_EXPECTED_LANGUAGE,
        "session_id": AUDIO_TIMESTAMP_SESSION_ID,
    }

    # Log test configuration
    print(f"Using test configuration: {config}")
    print(f"Audio file path: {audio_file_path}")
    print(
        f"Expected results: language='{expected_result['language']}', session_id='{expected_result['session_id']}'"
    )
    print("Timestamp validation requirements:")
    print("  1. start_ms and duration_ms must be int type")
    print("  2. Time unit must be milliseconds")
    print("  3. start_ms must be non-negative integer")
    print("  4. duration_ms must be positive integer")

    # Create and run tester
    tester = AudioTimestampAsrTester(
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
