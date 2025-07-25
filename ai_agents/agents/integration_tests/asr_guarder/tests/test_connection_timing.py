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
DEFAULT_CONFIG_FILE = "property_en.json"
DEFAULT_EXPECTED_TEXT = "hello world"
DEFAULT_SESSION_ID = "test_session_123"
DEFAULT_EXPECTED_LANGUAGE = "en-US"


class AsrExtensionTester(AsyncExtensionTester):
    """Test class for ASR extension integration testing."""

    def __init__(
        self,
        audio_file_path: str,
        expected_text: str = DEFAULT_EXPECTED_TEXT,
        session_id: str = DEFAULT_SESSION_ID,
        expected_language: str = DEFAULT_EXPECTED_LANGUAGE,
    ):
        super().__init__()
        self.audio_file_path: str = audio_file_path
        self.expected_text: str = expected_text
        self.session_id: str = session_id
        self.expected_language: str = expected_language
        self.sender_task: asyncio.Task[None] | None = None

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

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio data and silence packets to ASR extension."""
        try:
            # Send audio file
            await self._send_audio_file(ten_env)

            # Send silence packets to trigger final results
            await self._send_silence_packets(ten_env)

        except Exception as e:
            ten_env.log_error(f"Error in audio sender: {e}")
            raise

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the ASR integration test."""
        ten_env.log_info("Starting ASR integration test")
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

    def _validate_text_content(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate text content matches expected text."""
        text: str = json_data.get("text", "")
        if self.expected_text.lower() not in text.lower():
            self._stop_test_with_error(
                ten_env,
                f"Text mismatch, expected: '{self.expected_text}' to be contained in actual: '{text}'",
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
            lambda: self._validate_text_content(ten_env, json_data),
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
        ten_env.log_info(f"Received ASR result - final: {is_final}")

        if not is_final:
            ten_env.log_info("Received intermediate ASR result, continuing...")
            return

        # For final results, log complete structure and validate
        ten_env.log_info("Received final ASR result, validating...")
        self._log_asr_result_structure(
            ten_env, json_str, json_data.get("metadata")
        )

        # Validate final result - metadata is part of json_data according to API spec
        metadata_dict: dict[str, Any] | None = json_data.get("metadata")
        if self._validate_final_result(ten_env, json_data, metadata_dict):
            ten_env.log_info("✅ ASR integration test passed with final result")
            ten_env.stop_test()

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


def test_connection_timing(extension_name: str, config_dir: str) -> None:
    """Verify extension establishes connection after startup."""

    # Audio file path
    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data/16k_en_us_helloworld.pcm"
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
        "text": DEFAULT_EXPECTED_TEXT,
        "language": DEFAULT_EXPECTED_LANGUAGE,
        "session_id": DEFAULT_SESSION_ID,
    }

    # Log test configuration
    print(f"Using test configuration: {config}")
    print(f"Audio file path: {audio_file_path}")
    print(
        f"Expected results: text='{expected_result['text']}', language='{expected_result['language']}', session_id='{expected_result['session_id']}'"
    )

    # Create and run tester
    tester = AsrExtensionTester(
        audio_file_path=audio_file_path,
        expected_text=expected_result["text"],
        session_id=expected_result["session_id"],
        expected_language=expected_result["language"],
    )

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"
