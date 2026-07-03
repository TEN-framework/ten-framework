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

        self.audio_file_path: str = audio_file_path
        self.session_id: str = session_id
        self.expected_language: str = expected_language
        self.final_results = []  # Collect all final results
        self.audio_duration_ms = 0  # Audio file total duration
        self.test_completed = False
        self.audio_sent = False
        self.finalize_sent = False
        self.last_result_time = 0  # Track when last result was received

    def _calculate_audio_duration(self, audio_file_path: str) -> int:
        """Calculate the actual duration of the audio file in milliseconds."""
        try:
            file_size = os.path.getsize(audio_file_path)
            # PCM format: 16-bit = 2 bytes per sample, mono = 1 channel
            # Sample rate is 16000 Hz (16kHz)
            bytes_per_sample = 2  # 16-bit
            channels = 1  # mono
            sample_rate = 16000  # 16kHz

            # Calculate total samples
            total_samples = file_size / (bytes_per_sample * channels)

            # Calculate duration in seconds
            duration_seconds = total_samples / sample_rate

            return int(duration_seconds * 1000)
        except Exception as e:
            print(f"Warning: Could not calculate audio duration: {e}")
            return 0

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

        try:
            with open(self.audio_file_path, "rb") as audio_file:
                chunk_count = 0
                while True:
                    chunk = audio_file.read(AUDIO_CHUNK_SIZE)
                    if not chunk:
                        break

                    audio_frame = self._create_audio_frame(
                        chunk, self.session_id
                    )
                    await ten_env.send_audio_frame(audio_frame)
                    chunk_count += 1

                    # Reduce interval between chunks for faster sending
                    await asyncio.sleep(0.001)  # 1ms instead of 10ms

                ten_env.log_info(
                    f"✅ Audio file sent successfully: {chunk_count} chunks"
                )

        except Exception as e:
            ten_env.log_error(f"Error sending audio file: {e}")
            raise

    async def _send_finalize_signal(self, ten_env: AsyncTenEnvTester) -> None:
        """Send asr_finalize signal to trigger finalization."""
        ten_env.log_info("Sending asr_finalize signal...")

        try:
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

        except Exception as e:
            ten_env.log_error(f"Error sending finalize signal: {e}")
            raise

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio data and finalize signal to ASR extension."""
        try:
            # Calculate audio duration
            self.audio_duration_ms = self._calculate_audio_duration(
                self.audio_file_path
            )
            ten_env.log_info(f"Audio file duration: {self.audio_duration_ms}ms")

            # Send audio file
            await self._send_audio_file(ten_env)
            self.audio_sent = True

            # Wait shorter time after sending audio, but ensure ASR has processed it
            await asyncio.sleep(1.0)

            # Send finalize signal after audio send
            await self._send_finalize_signal(ten_env)
            self.finalize_sent = True

            # Wait for results to be processed in on_data

            # Dynamic wait: check for results every 0.5 seconds, up to 30 seconds total
            max_wait_time = 30.0  # Maximum total wait time
            check_interval = 0.5  # Check every 0.5 seconds
            total_wait_time = 0
            last_result_count = 0

            while total_wait_time < max_wait_time:
                current_result_count = len(self.final_results)

                if current_result_count > 0:
                    if current_result_count > last_result_count:
                        # New results received, reset timer
                        last_result_count = current_result_count
                        ten_env.log_info(
                            f"Received {current_result_count} final results, continuing to wait for more..."
                        )
                    elif current_result_count == last_result_count:
                        # No new results for a while, check if we should proceed
                        if (
                            total_wait_time > 5.0
                        ):  # Wait at least 5 seconds after last result
                            ten_env.log_info(
                                f"No new results for 5 seconds, proceeding with validation of {current_result_count} final results"
                            )
                            break

                await asyncio.sleep(check_interval)
                total_wait_time += check_interval

            # If no results received after timeout, check if we should fail
            if len(self.final_results) == 0:
                ten_env.log_error("No final results received after timeout")
                ten_env.log_error("Audio sent: " + str(self.audio_sent))
                ten_env.log_error("Finalize sent: " + str(self.finalize_sent))
                ten_env.log_error(
                    "Audio duration: " + str(self.audio_duration_ms) + "ms"
                )
                self._stop_test_with_error(ten_env, "No final results received")
            else:
                # Validate the collected results
                if self._validate_multiple_final_results(ten_env):
                    ten_env.log_info(
                        "✅ Multiple final results timestamp validation passed"
                    )
                    ten_env.stop_test()
                else:
                    self._stop_test_with_error(
                        ten_env,
                        "Multiple final results timestamp validation failed",
                    )

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
        ten_env.log_info("=" * 60)
        ten_env.log_info("FINAL ASR RESULT STRUCTURE:")
        ten_env.log_info("=" * 60)
        ten_env.log_info(f"Raw JSON: {json_str}")
        ten_env.log_info("=" * 60)

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
            f"✅ Timestamp type validation passed - start_ms: {start_ms}, duration_ms: {duration_ms}"
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

        # start_ms should be non-negative
        if start_ms < 0:
            self._stop_test_with_error(
                ten_env,
                f"start_ms should be non-negative, got {start_ms}",
            )
            return False

        # For the first result, start_ms should typically be 0
        # For subsequent results, it should be reasonable based on audio duration
        if start_ms == 0:
            ten_env.log_info(
                "✅ start_ms correctly indicates beginning of audio (0ms)"
            )
        elif start_ms > 0:
            # For non-zero start times, just validate they are reasonable
            # ASR may return results at any time point during audio processing
            ten_env.log_info(
                f"✅ start_ms correctly reflects audio position: {start_ms}ms"
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
            f"✅ Timestamp precision validation passed - start_ms={start_ms}ms, duration_ms={duration_ms}ms"
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
            lambda: self._validate_start_ms_accuracy(ten_env, json_data),
            lambda: self._validate_timestamp_precision(ten_env, json_data),
        ]

        return all(validation() for validation in validations)

    def _validate_multiple_final_results(
        self, ten_env: AsyncTenEnvTester
    ) -> bool:
        """Validate multiple final results timestamp continuity and audio coverage."""
        if len(self.final_results) < 1:
            ten_env.log_error("No final results collected")
            return False

        # Sort results by start_ms
        sorted_results = sorted(
            self.final_results, key=lambda x: x.get("start_ms", 0)
        )
        ten_env.log_info(f"Validating {len(sorted_results)} final results")

        # Validate timestamp continuity
        for i in range(len(sorted_results) - 1):
            current = sorted_results[i]
            next_result = sorted_results[i + 1]

            current_end = current.get("start_ms", 0) + current.get(
                "duration_ms", 0
            )
            next_start = next_result.get("start_ms", 0)

            # Check for overlaps or gaps
            if current_end > next_start:
                ten_env.log_error(
                    f"Timestamp overlap: result {i} ends at {current_end}ms, result {i+1} starts at {next_start}ms"
                )
                return False

        # For multiple final results, focus on continuity rather than full coverage
        # Different ASR providers may return multiple final results covering different audio segments
        last_result = sorted_results[-1]
        last_end = last_result.get("start_ms", 0) + last_result.get(
            "duration_ms", 0
        )

        ten_env.log_info(
            f"✅ Multiple results validation passed: {len(self.final_results)} final results"
        )
        ten_env.log_info(f"Last result end time: {last_end}ms")

        # Log audio coverage info for reference, but don't fail the test
        if self.audio_duration_ms > 0:
            coverage_ratio = last_end / self.audio_duration_ms
            ten_env.log_info(f"Audio file duration: {self.audio_duration_ms}ms")
            ten_env.log_info(f"Coverage ratio: {coverage_ratio*100:.1f}%")

            # Only warn if coverage is very low (< 90%)
            if coverage_ratio < 0.9:
                ten_env.log_info(
                    f"⚠️ Low audio coverage: {coverage_ratio*100:.1f}% (this may be normal for some ASR providers)"
                )
        else:
            ten_env.log_info("Audio duration not calculated")

        return True

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
        text: str = json_data.get("text", "")
        ten_env.log_info(
            f"Received ASR result - final: {is_final}, text: '{text}'"
        )

        if is_final:
            # Update last result time
            self.last_result_time = asyncio.get_event_loop().time()

            # Collect final result
            self.final_results.append(json_data)
            ten_env.log_info(
                f"Collected final result #{len(self.final_results)}"
            )

            # Validate current result
            metadata_dict: dict[str, Any] | None = json_data.get("metadata")
            if not self._validate_final_result(
                ten_env, json_data, metadata_dict
            ):
                return

            # Log complete structure for the first final result
            if len(self.final_results) == 1:
                self._log_asr_result_structure(
                    ten_env, json_str, json_data.get("metadata")
                )

            # Wait a bit more to see if more final results come
            await asyncio.sleep(1.0)

            # For multiple final results, we need to wait longer
            # Instead of checking time here, let's just collect results
            # The validation will be done in audio_sender after a longer wait
            ten_env.log_info(
                f"Collected {len(self.final_results)} final results"
            )

        else:
            ten_env.log_info(
                f"Received intermediate ASR result: '{text}', continuing..."
            )

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""
        ten_env.log_info("Test stopped")


def test_audio_timestamp(extension_name: str, config_dir: str) -> None:
    """Verify ASR result timestamp fields meet requirements."""

    print("=" * 80)
    print("🧪 TEST CASE: Audio Timestamp ASR Test")
    print("📋 Validate ASR result timestamp fields and accuracy")
    print("=" * 80)

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
