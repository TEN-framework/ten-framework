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
import tempfile
import uuid
from pathlib import Path


# Constants for audio configuration
AUDIO_CHUNK_SIZE = 320
AUDIO_SAMPLE_RATE = 16000
AUDIO_BYTES_PER_SAMPLE = 2
FRAME_INTERVAL_MS = 10
TOTAL_FRAMES = 30

# Constants for test configuration
DUMP_CONFIG_FILE = "property_en.json"
DUMP_SESSION_ID = "test_dump_session_123"
DUMP_EXPECTED_LANGUAGE = "en-US"


class DumpTester(AsyncExtensionTester):
    """Test class for ASR dump functionality testing."""

    def __init__(
        self,
        session_id: str = DUMP_SESSION_ID,
        expected_language: str = DUMP_EXPECTED_LANGUAGE,
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: ASR Dump Functionality Test")
        print("=" * 80)
        print("📋 Test Description: Validate ASR extension dump functionality")
        print("🎯 Test Objectives:")
        print("   - Verify ASR extension can process audio and return results")
        print("   - Test dump functionality with real audio file")
        print("   - Validate dump file is created and not empty")
        print("   - Check dump file content matches original audio file")
        print("   - Verify dump file size is appropriate")
        print("   - Test dump file path configuration")
        print("   - Ensure proper cleanup of temporary files")
        print("=" * 80)

        self.session_id: str = session_id
        self.expected_language: str = expected_language
        self.sender_task: asyncio.Task[None] | None = None

        # Track dump state
        self.dump_file_path: str | None = None
        self.frames_sent: int = 0

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

    async def _send_audio_frames(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio frames from real audio file for dump verification."""
        ten_env.log_info(
            "Sending audio frames from real audio file for dump testing..."
        )

        # Audio file path
        audio_file_path = os.path.join(
            os.path.dirname(__file__), "test_data/16k_en_us_helloworld.pcm"
        )

        if not os.path.exists(audio_file_path):
            ten_env.log_error(f"Audio file not found: {audio_file_path}")
            raise FileNotFoundError(f"Audio file not found: {audio_file_path}")

        ten_env.log_info(f"Reading audio file: {audio_file_path}")

        with open(audio_file_path, "rb") as audio_file:
            while True:
                chunk = audio_file.read(AUDIO_CHUNK_SIZE)
                if not chunk:
                    break

                audio_frame = self._create_audio_frame(chunk, self.session_id)
                await ten_env.send_audio_frame(audio_frame)
                self.frames_sent += 1

                await asyncio.sleep(FRAME_INTERVAL_MS / 1000)

        ten_env.log_info(
            f"✅ Sent {self.frames_sent} audio frames from real audio file"
        )

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio data for dump testing."""
        try:
            # Send audio frames
            ten_env.log_info("=== Starting audio send for dump test ===")
            await self._send_audio_frames(ten_env)

            # Wait for dump file to be written
            ten_env.log_info("=== Waiting for dump file to be written ===")
            await asyncio.sleep(2)  # Give time for dump file to be written

            # Stop test after sending all frames
            ten_env.log_info("=== Audio send completed, stopping test ===")
            ten_env.stop_test()

        except Exception as e:
            ten_env.log_error(f"Error in audio sender: {e}")
            raise

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the ASR dump test."""
        ten_env.log_info("Starting ASR dump test")
        await self.audio_sender(ten_env)

    def _stop_test_with_error(
        self, ten_env: AsyncTenEnvTester, error_message: str
    ) -> None:
        """Stop test with error message."""
        ten_env.stop_test(
            TenError.create(TenErrorCode.ErrorCodeGeneric, error_message)
        )

    def _validate_dump_file(self, dump_file_path: str) -> bool:
        """Validate the dump file content."""
        try:
            if not os.path.exists(dump_file_path):
                print(f"❌ Dump file does not exist: {dump_file_path}")
                return False

            # Check file size is not empty
            file_size = os.path.getsize(dump_file_path)
            if file_size == 0:
                print(f"❌ Dump file is empty: {file_size} bytes")
                return False

            # Read the original audio file to compare with dump file
            audio_file_path = os.path.join(
                os.path.dirname(__file__), "test_data/16k_en_us_helloworld.pcm"
            )

            if os.path.exists(audio_file_path):
                with open(audio_file_path, "rb") as original_file:
                    original_content = original_file.read()

                # Read dump file content
                with open(dump_file_path, "rb") as dump_file:
                    dump_content = dump_file.read()

                # Compare content - dump file should contain the same audio data
                if dump_content != original_content:
                    print(
                        f"❌ Dump file content does not match original audio file"
                    )
                    return False

                print(
                    f"✅ Dump file content matches original audio file - size: {file_size} bytes"
                )
            else:
                print(
                    f"⚠️  Original audio file not found for comparison: {audio_file_path}"
                )
                print(f"✅ Dump file exists with size: {file_size} bytes")

            return True

        except Exception as e:
            print(f"❌ Error validating dump file: {e}")
            return False

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from ASR extension."""
        name: str = data.get_name()

        if name == "asr_result":
            """Handle asr_result data."""
            # Parse ASR result
            json_str, _ = data.get_property_to_json(None)
            json_data: dict[str, Any] = json.loads(json_str)

            # Log ASR result for debugging
            is_final: bool = json_data.get("final", False)
            result_id: str = json_data.get("id", "")
            ten_env.log_info(
                f"Received ASR result - final: {is_final}, id: {result_id}"
            )

            # For dump test, we don't need to validate ASR results extensively
            # Just log them for debugging purposes
            if is_final:
                ten_env.log_info("✅ Received final ASR result")
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


def test_dump(extension_name: str, config_dir: str) -> None:
    """Verify ASR dump functionality with audio file output."""

    # Get config file path
    config_file_path = os.path.join(config_dir, DUMP_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Create temporary directory for dump file
    temp_dir = Path(tempfile.gettempdir()) / str(uuid.uuid4())
    temp_dir.mkdir(parents=True, exist_ok=True)

    # Define dump file path
    dump_file_path = temp_dir / "azure_asr_in.pcm"

    # Remove existing dump file if it exists
    if dump_file_path.exists():
        dump_file_path.unlink()

    # Update config to enable dump functionality
    if "params" not in config:
        config["params"] = {}

    config["params"]["dump"] = True
    config["params"]["dump_path"] = str(temp_dir)

    # Expected test results
    expected_result = {
        "language": DUMP_EXPECTED_LANGUAGE,
        "session_id": DUMP_SESSION_ID,
        "frames": TOTAL_FRAMES,
        "dump_file": str(dump_file_path),
    }

    # Log test configuration
    print(f"Using test configuration: {config}")
    print(f"Temporary directory: {temp_dir}")
    print(f"Dump file path: {dump_file_path}")
    print(
        f"Expected results: language='{expected_result['language']}', session_id='{expected_result['session_id']}', frames={expected_result['frames']}"
    )
    print("Dump validation requirements:")
    print("  1. Send real audio file frames")
    print("  2. Enable dump functionality in config")
    print("  3. Verify dump file exists")
    print("  4. Validate dump file size is not empty")
    print("  5. Validate dump file content matches original audio file")

    # Create and run tester
    tester = DumpTester(
        session_id=str(expected_result["session_id"]),
        expected_language=str(expected_result["language"]),
    )

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"

    # Validate dump file
    print(f"Validating dump file: {dump_file_path}")
    assert (
        dump_file_path.exists()
    ), f"Dump file does not exist: {dump_file_path}"

    # Validate dump file content
    file_size = dump_file_path.stat().st_size

    # For real audio file, we can't predict exact size, so just check it's not empty
    assert file_size > 0, f"Dump file is empty: {file_size} bytes"

    # Read the original audio file to compare with dump file
    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data/16k_en_us_helloworld.pcm"
    )

    if os.path.exists(audio_file_path):
        with open(audio_file_path, "rb") as original_file:
            original_content = original_file.read()

        # Read dump file content
        with open(dump_file_path, "rb") as dump_file:
            dump_content = dump_file.read()

        # Compare content - dump file should contain the same audio data
        assert (
            dump_content == original_content
        ), f"Dump file content does not match original audio file"

        print(
            f"✅ Dump file content matches original audio file - size: {file_size} bytes"
        )
    else:
        print(
            f"⚠️  Original audio file not found for comparison: {audio_file_path}"
        )
        print(f"✅ Dump file exists with size: {file_size} bytes")

    print(
        f"✅ Dump test passed - file size: {file_size} bytes, frames: {TOTAL_FRAMES}"
    )

    # Clean up temporary directory
    try:
        import shutil

        shutil.rmtree(temp_dir)
        print(f"✅ Cleaned up temporary directory: {temp_dir}")
    except Exception as e:
        print(
            f"Warning: Failed to clean up temporary directory {temp_dir}: {e}"
        )
