
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
import glob
import time

TTS_DUMP_CONFIG_FILE="property_dump.json"


class DumperByRequestTester(AsyncExtensionTester):
    """Test class for TTS extension dump"""

    def __init__(
        self,
        session_id: str = "test_dump_each_request_id_session_123",
        text: str = "",
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: Dump TTS Test")
        print("=" * 80)
        print(
            "📋 Test Description: Validate TTS result dump"
        )
        print("🎯 Test Objectives:")
        print("   - Verify dump is generated")
        print("=" * 80)

        self.session_id: str = session_id
        self.text: str = text
        self.dump_file_name = f"tts_dump_{self.session_id}.pcm"
        self.count_audio_end = 0

    async def _send_finalize_signal(self, ten_env: AsyncTenEnvTester) -> None:
        """Send tts_finalize signal to trigger finalization."""
        ten_env.log_info("Sending tts_finalize signal...")

        # Create finalize data according to protocol
        finalize_data = {
            "finalize_id": f"finalize_{self.session_id}_{int(asyncio.get_event_loop().time())}",
            "metadata": {"session_id": self.session_id},
        }

        # Create Data object for tts_finalize
        finalize_data_obj = Data.create("tts_finalize")
        finalize_data_obj.set_property_from_json(
            None, json.dumps(finalize_data)
        )

        # Send the finalize signal
        await ten_env.send_data(finalize_data_obj)

        ten_env.log_info(
            f"✅ tts_finalize signal sent with ID: {finalize_data['finalize_id']}"
        )

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the TTS invalid required params test."""
        ten_env.log_info("Starting TTS invalid required params test")
        await self._send_tts_text_input(ten_env, self.text)

    async def _send_tts_text_input(self, ten_env: AsyncTenEnvTester, text: str, request_num: int = 1) -> None:
        """Send tts text input to TTS extension."""
        ten_env.log_info(f"Sending tts text input: {text}")
        tts_text_input_obj = Data.create("tts_text_input")
        tts_text_input_obj.set_property_string("text", text)
        tts_text_input_obj.set_property_string("request_id", "test_dump_each_request_id_request_id_"+str(request_num))
        tts_text_input_obj.set_property_bool("text_input_end", True)
        metadata = {
            "session_id": "test_dump_each_request_id_session_123",
            "turn_id": 1,
        }
        tts_text_input_obj.set_property_from_json("metadata", json.dumps(metadata))
        await ten_env.send_data(tts_text_input_obj)
        ten_env.log_info(f"✅ tts text input sent: {text}")

    def _stop_test_with_error(
        self, ten_env: AsyncTenEnvTester, error_message: str
    ) -> None:
        ten_env.log_info(f"Stopping test with error message: {error_message}")
        """Stop test with error message."""
        ten_env.stop_test(
            TenError.create(TenErrorCode.ErrorCodeGeneric, error_message)
        )

    def _log_tts_result_structure(
        self,
        ten_env: AsyncTenEnvTester,
        json_str: str,
        metadata: Any,
    ) -> None:
        """Log complete TTS result structure for debugging."""
        ten_env.log_info("=" * 80)
        ten_env.log_info("RECEIVED TTS RESULT - COMPLETE STRUCTURE:")
        ten_env.log_info("=" * 80)
        ten_env.log_info(f"Raw JSON string: {json_str}")
        ten_env.log_info(f"Metadata: {metadata}")
        ten_env.log_info(f"Metadata type: {type(metadata)}")
        ten_env.log_info("=" * 80)

    def _validate_required_fields(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that all required fields exist in TTS result."""
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

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from TTS extension."""
        name: str = data.get_name()

        if name == "error":
            json_str, _ = data.get_property_to_json("")
            ten_env.log_info(f"Received error data: {json_str}")

            self._stop_test_with_error(ten_env, f"Received error data")
            return
        elif name == "tts_audio_end":
            if self.count_audio_end == 0:
                self.count_audio_end += 1
                time.sleep(1)
                await self._send_tts_text_input(ten_env, "second request id" + self.text, 2)
                return
            else:
                ten_env.log_info("✅ Two TTS audio end received, check dump file number")
                self._check_dump_file_number(ten_env)
            return

    def _check_dump_file_number(self, ten_env: AsyncTenEnvTester) -> None:
        """Check if there are exactly two dump files in the directory."""
        # Check the number of files in TTS extension dump directory
        if not hasattr(self, 'tts_extension_dump_folder') or not self.tts_extension_dump_folder:
            ten_env.log_error("tts_extension_dump_folder not set")
            self._stop_test_with_error(ten_env, "tts_extension_dump_folder not configured")
            return

        if not os.path.exists(self.tts_extension_dump_folder):
            self._stop_test_with_error(ten_env, f"TTS extension dump folder not found: {self.tts_extension_dump_folder}")
            return
        
        # Get all files in the directory
        time.sleep(1)
        dump_files = []
        for file_path in glob.glob(os.path.join(self.tts_extension_dump_folder, "*")):
            if os.path.isfile(file_path):
                dump_files.append(file_path)

        ten_env.log_info(f"Found {len(dump_files)} dump files in {self.tts_extension_dump_folder}")
        for i, file_path in enumerate(dump_files):
            ten_env.log_info(f"  {i+1}. {os.path.basename(file_path)}")
        
        # Check if there are exactly two dump files
        if len(dump_files) == 2:
            ten_env.log_info("✅ Found exactly 2 dump files as expected")

            ten_env.stop_test()
        elif len(dump_files) > 2:
            self._stop_test_with_error(ten_env, f"Found {len(dump_files)} dump files, expected exactly 2")
        else:
            self._stop_test_with_error(ten_env, f"Found {len(dump_files)} dump files, expected exactly 2")


    @override
    async def on_audio_frame(self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame) -> None:
        """Handle received audio frame from TTS extension."""
        pass

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""

        ten_env.log_info("Test stopped")
        _delete_dump_file(self.tts_extension_dump_folder)

def _delete_dump_file(dump_path: str) -> None:
    for file_path in glob.glob(os.path.join(dump_path, "*")):
            if os.path.isfile(file_path):
                os.remove(file_path)
            elif os.path.isdir(file_path):
                import shutil
                shutil.rmtree(file_path)

def test_dump_each_request_id(extension_name: str, config_dir: str) -> None:
    """Verify TTS result dump."""


    # Get config file path
    config_file_path = os.path.join(config_dir, TTS_DUMP_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Expected test results


    # Log test configuration
    print(f"Using test configuration: {config}")
    if not os.path.exists(config["dump_path"]):
        os.makedirs(config["dump_path"])
    else:
        # Delete all files in the directory
        _delete_dump_file(config["dump_path"])



    # Create and run tester
    tester = DumperByRequestTester(
        session_id="test_dump_each_request_id_session_123",
        text="hello world, hello agora, hello shanghai, nice to meet you!",
    )

    # Set the tts_extension_dump_folder for the tester
    tester.tts_extension_dump_folder = config["dump_path"]

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"
