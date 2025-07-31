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
        audio_file_path: str = "",
    ):
        super().__init__()

        # Print test case header
        print("=" * 80)
        print("🧪 ASR DUMP FUNCTIONALITY TEST")
        print("=" * 80)
        print("📋 Test Description: Validate ASR extension dump functionality")
        print("🎯 Test Objectives:")
        print(
            "   • Verify ASR extension can process audio and generate dump files"
        )
        print("   • Test dump file content matches original audio data")
        print("   • Validate dump file creation and proper cleanup")
        print("   • Ensure audio frame processing integrity")
        print("=" * 80)

        self.session_id: str = session_id
        self.expected_language: str = expected_language
        self.audio_file_path: str = audio_file_path
        self.frames_sent: int = 0

    def _create_audio_frame(self, data: bytes, session_id: str) -> AudioFrame:
        """Create an audio frame with the given data and session ID."""
        audio_frame = AudioFrame.create("pcm_frame")

        # Set session_id in metadata according to API specification
        metadata = {"session_id": session_id}
        audio_frame.set_property_from_json("metadata", json.dumps(metadata))

        # Allocate buffer and copy data
        audio_frame.alloc_buf(len(data))
        buf = audio_frame.lock_buf()
        data_array = bytearray(data)
        buf[: len(data_array)] = data_array
        audio_frame.unlock_buf(buf)

        return audio_frame

    async def _send_audio_file(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio file data to ASR extension."""
        ten_env.log_info(f"Sending audio file: {self.audio_file_path}")

        with open(self.audio_file_path, "rb") as audio_file:
            chunk_count = 0
            total_bytes_sent = 0

            while True:
                chunk = audio_file.read(AUDIO_CHUNK_SIZE)
                if not chunk:
                    break

                chunk_count += 1
                total_bytes_sent += len(chunk)

                audio_frame = self._create_audio_frame(chunk, self.session_id)
                await ten_env.send_audio_frame(audio_frame)
                await asyncio.sleep(FRAME_INTERVAL_MS / 1000)

            ten_env.log_info(
                f"Audio file sent completely: {chunk_count} chunks, {total_bytes_sent} total bytes"
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
        ten_env.log_info(
            f"asr_finalize signal sent with ID: {finalize_data['finalize_id']}"
        )

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio data for dump testing."""
        try:
            # Send audio frames
            ten_env.log_info("Starting audio send for dump test")
            await self._send_audio_file(ten_env)

            # Wait after sending audio
            ten_env.log_info("Waiting after audio send")
            await asyncio.sleep(1.5)

            # Send finalize signal after audio send
            ten_env.log_info("Sending finalize signal")
            await self._send_finalize_signal(ten_env)

            # Wait after sending finalize signal
            ten_env.log_info("Waiting after finalize signal")
            await asyncio.sleep(1.5)

            # Additional wait for ASR providers that may produce multiple final results
            # This ensures we capture all possible dump data before stopping the test
            ten_env.log_info(
                "Waiting for potential additional final results from ASR provider"
            )
            await asyncio.sleep(3.0)

            # Now stop the test after all operations are complete
            ten_env.log_info("All operations completed, stopping test")
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

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from ASR extension."""
        name: str = data.get_name()

        if name == "asr_result":
            # Parse ASR result
            json_str, _ = data.get_property_to_json(None)
            json_data: dict[str, Any] = json.loads(json_str)

            # Check if this is the final result
            is_final: bool = json_data.get("final", False)
            result_id: str = json_data.get("id", "")

            ten_env.log_info(
                f"Received ASR result - final: {is_final}, id: {result_id}"
            )

            # Track final results but don't stop test immediately
            # Some ASR providers (like Azure) may produce multiple final results
            if is_final:
                ten_env.log_info("Received final ASR result")
                # Note: Test will stop when audio_sender completes, not here

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""
        ten_env.log_info("Test stopped")


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

    # Update config to enable dump functionality
    if "params" not in config:
        config["params"] = {}

    config["dump"] = True
    config["dump_path"] = str(temp_dir)

    # Get audio file path
    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data/16k_en_us.pcm"
    )

    # Create and run tester
    tester = DumpTester(
        session_id=DUMP_SESSION_ID,
        expected_language=DUMP_EXPECTED_LANGUAGE,
        audio_file_path=audio_file_path,
    )

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"

    # Find dump file in the directory
    pcm_files = list(temp_dir.glob("*.pcm"))
    assert (
        len(pcm_files) > 0
    ), f"No .pcm files found in dump directory: {temp_dir}"

    # Use the first .pcm file found
    dump_file_path = pcm_files[0]
    print(f"Found dump file: {dump_file_path}")

    # Validate dump file content
    file_size = dump_file_path.stat().st_size
    assert file_size > 0, f"Dump file is empty: {file_size} bytes"

    # Compare dump file with original audio file
    if os.path.exists(audio_file_path):
        with open(audio_file_path, "rb") as original_file:
            original_content = original_file.read()

        with open(dump_file_path, "rb") as dump_file:
            dump_content = dump_file.read()

        # Verify content matches
        assert (
            dump_content == original_content
        ), "Dump file content does not match original audio file"
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
