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
AUDIO_BYTES_PER_SAMPLE = 2
SILENCE_DURATION_SECONDS = 5
FRAME_INTERVAL_MS = 10

# Constants for test configuration
METRICS_CONFIG_FILE = "property_en.json"
METRICS_SESSION_ID = "test_metrics_session_123"
METRICS_EXPECTED_LANGUAGE = "en-US"


class MetricsTester(AsyncExtensionTester):
    """Test class for ASR metrics testing."""

    def __init__(
        self,
        audio_file_path: str,
        session_id: str = METRICS_SESSION_ID,
        expected_language: str = METRICS_EXPECTED_LANGUAGE,
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: ASR Metrics Test")
        print("=" * 80)
        print(
            "📋 Test Description: Validate ASR extension metrics functionality"
        )
        print("🎯 Test Objectives:")
        print("   - Verify ASR extension can process audio and return results")
        print("   - Test active sending of asr_finalize signal")
        print("   - Validate metrics data structure and format")
        print("   - Check TTFW (Time To First Word) metric")
        print("   - Verify TTLW (Time To Last Word) metric")
        print("   - Validate asr_finalize_end signal is received")
        print("   - Ensure both TTFW and TTLW metrics are captured")
        print("=" * 80)

        self.audio_file_path: str = audio_file_path
        self.session_id: str = session_id
        self.expected_language: str = expected_language
        self.sender_task: asyncio.Task[None] | None = None

        # Track metrics state
        self.ttfw: int | None = None
        self.ttlw: int | None = None
        self.finalize_id: str | None = None
        self.finalize_end_received: bool = False

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

    def _create_silence_frame(self, size: int, session_id: str) -> AudioFrame:
        """Create a silence frame filled with zeros."""
        silence_frame = AudioFrame.create("pcm_frame")

        # Set session_id in metadata according to API specification
        metadata = {"session_id": session_id}
        silence_frame.set_property_from_json("metadata", json.dumps(metadata))

        silence_frame.alloc_buf(size)
        buf = silence_frame.lock_buf()
        buf[:] = b"\x00" * size
        silence_frame.unlock_buf(buf)
        return silence_frame

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

    async def _send_silence_packets(self, ten_env: AsyncTenEnvTester) -> None:
        """Send silence packets to trigger final ASR results."""
        ten_env.log_info("Sending silence packets to trigger final results...")

        # Calculate silence packet parameters
        samples_per_chunk = AUDIO_CHUNK_SIZE // AUDIO_BYTES_PER_SAMPLE
        chunks_per_second = AUDIO_SAMPLE_RATE // samples_per_chunk
        total_chunks = SILENCE_DURATION_SECONDS * chunks_per_second

        for i in range(total_chunks):
            silence_frame = self._create_silence_frame(
                AUDIO_CHUNK_SIZE, self.session_id
            )
            await ten_env.send_audio_frame(silence_frame)
            await asyncio.sleep(FRAME_INTERVAL_MS / 1000)

        ten_env.log_info(
            "Silence packets sent, waiting for final ASR results..."
        )

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
        """Send audio data and silence packets to ASR extension."""
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
            ten_env.log_info("Waiting for final ASR result and metrics...")
            await asyncio.sleep(2)  # Give some time for processing

        except Exception as e:
            ten_env.log_error(f"Error in audio sender: {e}")
            raise

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the ASR metrics test."""
        ten_env.log_info("Starting ASR metrics test")
        await self.audio_sender(ten_env)

    def _stop_test_with_error(
        self, ten_env: AsyncTenEnvTester, error_message: str
    ) -> None:
        """Stop test with error message."""
        ten_env.stop_test(
            TenError.create(TenErrorCode.ErrorCodeGeneric, error_message)
        )

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

        self.finalize_end_received = True

        ten_env.log_info(
            f"✅ asr_finalize_end validation passed - ID: {finalize_end_id}, session_id: {finalize_end_session_id}"
        )
        return True

    def _validate_metrics(self, ten_env: AsyncTenEnvTester, data: Data) -> bool:
        """Validate metrics data."""
        ten_env.log_info("Validating metrics data...")

        # Parse metrics data
        json_str, _ = data.get_property_to_json(None)
        metrics_data: dict[str, Any] = json.loads(json_str)

        ten_env.log_info(
            f"Received metrics data: {json.dumps(metrics_data, indent=2)}"
        )

        # Validate required fields
        required_fields = ["id", "module", "vendor", "metrics", "metadata"]
        missing_fields = [
            field for field in required_fields if field not in metrics_data
        ]

        if missing_fields:
            self._stop_test_with_error(
                ten_env, f"Missing required fields in metrics: {missing_fields}"
            )
            return False

        # Validate module is "asr"
        if metrics_data.get("module") != "asr":
            self._stop_test_with_error(
                ten_env,
                f"Module should be 'asr', got: {metrics_data.get('module')}",
            )
            return False

        # Validate vendor is "microsoft"
        if metrics_data.get("vendor") != "microsoft":
            self._stop_test_with_error(
                ten_env,
                f"Vendor should be 'microsoft', got: {metrics_data.get('vendor')}",
            )
            return False

        # Validate session_id in metadata
        metadata = metrics_data.get("metadata", {})
        if not self._validate_session_id(ten_env, metadata):
            return False

        # Extract metrics
        metrics = metrics_data.get("metrics", {})
        if "ttfw" in metrics:
            self.ttfw = metrics["ttfw"]
            ten_env.log_info(f"✅ TTFW: {self.ttfw}")
        if "ttlw" in metrics:
            self.ttlw = metrics["ttlw"]
            ten_env.log_info(f"✅ TTLW: {self.ttlw}")

        # Check if we have both metrics
        if self.ttfw is not None and self.ttlw is not None:
            ten_env.log_info(
                f"✅ Both metrics received - TTFW: {self.ttfw}, TTLW: {self.ttlw}"
            )
            return True
        else:
            ten_env.log_info("Waiting for both TTFW and TTLW metrics...")
            return False

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from ASR extension."""
        name: str = data.get_name()

        if name == "asr_finalize_end":
            """Handle asr_finalize_end signal."""
            ten_env.log_info("Received asr_finalize_end signal")

            if self._validate_finalize_end(ten_env, data):
                ten_env.log_info("✅ asr_finalize_end validation completed")
            return
        elif name == "metrics":
            """Handle metrics data."""
            ten_env.log_info("Received metrics data")

            if self._validate_metrics(ten_env, data):
                # Check if we have both finalize_end and metrics
                if self.finalize_end_received:
                    ten_env.log_info(
                        "✅ ASR metrics test passed with all validations"
                    )
                    ten_env.stop_test()
                else:
                    ten_env.log_info("Waiting for asr_finalize_end signal...")
            return
        elif name == "asr_result":
            """Handle asr_result data."""
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

            if is_final:
                # Validate final result - metadata is part of json_data according to API spec
                metadata_dict: dict[str, Any] | None = json_data.get("metadata")
                if not self._validate_final_result(
                    ten_env, json_data, metadata_dict
                ):
                    return

                ten_env.log_info("✅ Final ASR result validation passed")
            else:
                ten_env.log_info(
                    "Received intermediate ASR result, continuing..."
                )
        else:
            ten_env.log_info(f"Received non-ASR data: {name}")

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""
        ten_env.log_info("Stopping audio sender task...")

        if self.sender_task:
            self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                ten_env.log_info("Audio sender task cancelled")
            except Exception as e:
                ten_env.log_error(f"Error cancelling audio sender task: {e}")
            finally:
                ten_env.log_info("Audio sender task cleanup completed")


def test_metrics(extension_name: str, config_dir: str) -> None:
    """Verify ASR metrics functionality with TTFW and TTLW metrics."""

    # Audio file path
    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data/16k_en_us_helloworld.pcm"
    )

    # Get config file path
    config_file_path = os.path.join(config_dir, METRICS_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Expected test results
    expected_result = {
        "language": METRICS_EXPECTED_LANGUAGE,
        "session_id": METRICS_SESSION_ID,
    }

    # Log test configuration
    print(f"Using test configuration: {config}")
    print(f"Audio file path: {audio_file_path}")
    print(
        f"Expected results: language='{expected_result['language']}', session_id='{expected_result['session_id']}'"
    )
    print("Metrics validation requirements:")
    print("  1. Send asr_finalize signal after audio send")
    print("  2. Receive final result with final=True")
    print("  3. Output asr_finalize_end signal")
    print("  4. Output metrics with TTFW and TTLW")
    print("  5. Validate metrics structure and content")

    # Create and run tester
    tester = MetricsTester(
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
