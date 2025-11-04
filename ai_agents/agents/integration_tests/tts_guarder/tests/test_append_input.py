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
import os
import glob
import time

TTS_DUMP_CONFIG_FILE = "property_dump.json"
AUDIO_DURATION_TOLERANCE_MS = 50

# Expected event sequence states
class EventSequenceState:
    """Track expected event sequence state"""
    WAITING_FIRST_AUDIO_START = "waiting_first_audio_start"
    RECEIVING_FIRST_AUDIO_FRAMES = "receiving_first_audio_frames"
    WAITING_FIRST_AUDIO_END = "waiting_first_audio_end"
    WAITING_SECOND_AUDIO_START = "waiting_second_audio_start"
    RECEIVING_SECOND_AUDIO_FRAMES = "receiving_second_audio_frames"
    WAITING_SECOND_AUDIO_END = "waiting_second_audio_end"
    COMPLETED = "completed"


class AppendInputTester(AsyncExtensionTester):
    """Test class for TTS extension append input"""

    def __init__(
        self,
        session_id: str = "test_append_input_session_123",
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: Append Input TTS Test")
        print("=" * 80)
        print("📋 Test Description: Validate TTS append input with multiple text inputs")
        print("🎯 Test Objectives:")
        print("   - Verify append input with multiple text inputs")
        print("   - Verify strict event sequence order")
        print("   - Verify dump file generation")
        print("=" * 80)

        self.session_id: str = session_id
        self.dump_file_name = f"tts_append_input_{self.session_id}.pcm"
        
        # Request IDs and metadata
        self.first_request_id = "test_append_input_request_id_1"
        self.second_request_id = "test_append_input_request_id_2"
        self.first_metadata = {
            "session_id": self.session_id,
            "turn_id": 1,
        }
        self.second_metadata = {
            "session_id": self.session_id,
            "turn_id": 2,
        }
        
        # Event sequence tracking
        self.event_state = EventSequenceState.WAITING_FIRST_AUDIO_START
        self.first_audio_start_received = False
        self.first_audio_frames_received = False
        self.first_audio_end_received = False
        self.second_audio_start_received = False
        self.second_audio_frames_received = False
        self.second_audio_end_received = False
        
        # Audio tracking
        self.current_request_id = None
        self.current_metadata = None
        self.audio_start_time = None
        self.total_audio_bytes = 0
        self.current_request_audio_bytes = 0
        self.sample_rate = 0
        self.first_request_audio_bytes = 0
        self.second_request_audio_bytes = 0

    def _calculate_pcm_audio_duration_ms(self, audio_bytes: int) -> int:
        """Calculate PCM audio duration in milliseconds based on audio bytes"""
        if audio_bytes == 0 or self.sample_rate == 0:
            return 0
        
        # PCM format: 16-bit (2 bytes per sample), mono (1 channel)
        bytes_per_sample = 2
        channels = 1
        duration_sec = audio_bytes / (self.sample_rate * bytes_per_sample * channels)
        return int(duration_sec * 1000)

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the TTS append input test - send all text inputs sequentially without waiting."""
        ten_env.log_info("Starting TTS append input test")
        
        # Text content with appropriate length
        texts = [
            "Hello world, this is the first text input.",
            "This is the second text input for testing.",
            "And this is the third text input message.",
            "Now we start the second group of inputs.",
            "This is the fifth text input message.",
            "And finally the sixth text input message.",
        ]
        
        # Send first 3 text inputs with same request_id and metadata
        ten_env.log_info("Sending first 3 text inputs with request_id_1...")
        for i, text in enumerate(texts[:3]):
            await self._send_tts_text_input(
                ten_env, text, self.first_request_id, self.first_metadata, i < 2
            )
        
        # Send last 3 text inputs with same request_id and metadata (different from first)
        ten_env.log_info("Sending last 3 text inputs with request_id_2...")
        for i, text in enumerate(texts[3:]):
            await self._send_tts_text_input(
                ten_env, text, self.second_request_id, self.second_metadata, i < 2
            )
        
        ten_env.log_info("✅ All 6 text inputs sent sequentially")

    async def _send_tts_text_input(
        self,
        ten_env: AsyncTenEnvTester,
        text: str,
        request_id: str,
        metadata: dict[str, Any],
        text_input_end: bool = True,
    ) -> None:
        """Send tts text input to TTS extension."""
        ten_env.log_info(f"Sending tts text input: {text} (request_id: {request_id}, text_input_end: {text_input_end})")
        tts_text_input_obj = Data.create("tts_text_input")
        tts_text_input_obj.set_property_string("text", text)
        tts_text_input_obj.set_property_string("request_id", request_id)
        tts_text_input_obj.set_property_bool("text_input_end", text_input_end)
        tts_text_input_obj.set_property_from_json("metadata", json.dumps(metadata))
        await ten_env.send_data(tts_text_input_obj)
        ten_env.log_info(f"✅ tts text input sent: {text}")

    def _stop_test_with_error(
        self, ten_env: AsyncTenEnvTester, error_message: str
    ) -> None:
        """Stop test with error message."""
        ten_env.stop_test(
            TenError.create(TenErrorCode.ErrorCodeGeneric, error_message)
        )

    def _validate_metadata(
        self,
        ten_env: AsyncTenEnvTester,
        received_metadata: dict[str, Any],
        expected_metadata: dict[str, Any],
        event_name: str,
    ) -> bool:
        """Validate metadata matches expected."""
        if received_metadata != expected_metadata:
            self._stop_test_with_error(
                ten_env,
                f"Metadata mismatch in {event_name}. Expected: {expected_metadata}, Received: {received_metadata}",
            )
            return False
        return True

    def _check_event_sequence(self, ten_env: AsyncTenEnvTester, received_event: str) -> None:
        """Check if received event matches expected sequence."""
        error_msg = None
        
        if received_event == "tts_audio_start":
            if self.event_state == EventSequenceState.WAITING_FIRST_AUDIO_START:
                # Expected first audio start
                self.event_state = EventSequenceState.RECEIVING_FIRST_AUDIO_FRAMES
            elif self.event_state == EventSequenceState.WAITING_SECOND_AUDIO_START:
                # Expected second audio start
                if not self.first_audio_end_received:
                    error_msg = "Received second tts_audio_start before first tts_audio_end"
                else:
                    self.event_state = EventSequenceState.RECEIVING_SECOND_AUDIO_FRAMES
            else:
                error_msg = f"Unexpected tts_audio_start in state: {self.event_state}"
        elif received_event == "audio_frame":
            if self.event_state == EventSequenceState.RECEIVING_FIRST_AUDIO_FRAMES:
                # Expected first audio frames
                pass
            elif self.event_state == EventSequenceState.RECEIVING_SECOND_AUDIO_FRAMES:
                # Expected second audio frames
                pass
            else:
                error_msg = f"Unexpected audio_frame in state: {self.event_state}"
        elif received_event == "tts_audio_end":
            if self.event_state == EventSequenceState.RECEIVING_FIRST_AUDIO_FRAMES:
                # Expected first audio end
                self.event_state = EventSequenceState.WAITING_SECOND_AUDIO_START
            elif self.event_state == EventSequenceState.RECEIVING_SECOND_AUDIO_FRAMES:
                # Expected second audio end
                self.event_state = EventSequenceState.COMPLETED
            else:
                error_msg = f"Unexpected tts_audio_end in state: {self.event_state}"
        
        if error_msg:
            self._stop_test_with_error(ten_env, error_msg)

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from TTS extension."""
        name: str = data.get_name()
        ten_env.log_info(f"Received data: {name} (current state: {self.event_state})")

        if name == "error":
            json_str, _ = data.get_property_to_json("")
            ten_env.log_info(f"Received error data: {json_str}")
            self._stop_test_with_error(ten_env, f"Received error data: {json_str}")
            return
        elif name == "tts_audio_start":
            # Check event sequence
            self._check_event_sequence(ten_env, "tts_audio_start")
            
            ten_env.log_info("Received tts_audio_start")
            self.audio_start_time = time.time()
            
            # Get request_id and validate
            received_request_id, _ = data.get_property_string("request_id")
            
            # Determine which request this is based on state
            if self.event_state == EventSequenceState.RECEIVING_FIRST_AUDIO_FRAMES:
                if received_request_id != self.first_request_id:
                    self._stop_test_with_error(
                        ten_env,
                        f"Request ID mismatch in first tts_audio_start. Expected: {self.first_request_id}, Received: {received_request_id}",
                    )
                    return
                self.current_request_id = self.first_request_id
                self.current_metadata = self.first_metadata
                self.first_audio_start_received = True
                self.current_request_audio_bytes = 0
            elif self.event_state == EventSequenceState.RECEIVING_SECOND_AUDIO_FRAMES:
                if received_request_id != self.second_request_id:
                    self._stop_test_with_error(
                        ten_env,
                        f"Request ID mismatch in second tts_audio_start. Expected: {self.second_request_id}, Received: {received_request_id}",
                    )
                    return
                self.current_request_id = self.second_request_id
                self.current_metadata = self.second_metadata
                self.second_audio_start_received = True
                self.current_request_audio_bytes = 0
            else:
                self._stop_test_with_error(ten_env, f"Unexpected tts_audio_start state: {self.event_state}")
                return
            
            # Validate metadata
            metadata_str, _ = data.get_property_to_json("metadata")
            if metadata_str:
                try:
                    received_metadata = json.loads(metadata_str)
                    expected_metadata = {
                        "session_id": self.current_metadata.get("session_id", ""),
                        "turn_id": self.current_metadata.get("turn_id", -1),
                    }
                    if not self._validate_metadata(ten_env, received_metadata, expected_metadata, "tts_audio_start"):
                        return
                except json.JSONDecodeError:
                    self._stop_test_with_error(ten_env, f"Invalid JSON in tts_audio_start metadata: {metadata_str}")
                    return
            else:
                self._stop_test_with_error(ten_env, f"Missing metadata in tts_audio_start response")
                return
            
            ten_env.log_info(f"✅ tts_audio_start received with correct request_id: {received_request_id} and metadata")
            return
        elif name == "tts_audio_end":
            # Check event sequence
            self._check_event_sequence(ten_env, "tts_audio_end")
            
            ten_env.log_info("Received tts_audio_end")
            
            # Get request_id and validate
            received_request_id, _ = data.get_property_string("request_id")
            
            # Determine which request this is based on state
            if self.event_state == EventSequenceState.WAITING_SECOND_AUDIO_START:
                # First audio end
                if received_request_id != self.first_request_id:
                    self._stop_test_with_error(
                        ten_env,
                        f"Request ID mismatch in first tts_audio_end. Expected: {self.first_request_id}, Received: {received_request_id}",
                    )
                    return
                self.current_request_id = self.first_request_id
                self.current_metadata = self.first_metadata
                self.first_audio_end_received = True
                self.first_request_audio_bytes = self.current_request_audio_bytes
            elif self.event_state == EventSequenceState.COMPLETED:
                # Second audio end
                if received_request_id != self.second_request_id:
                    self._stop_test_with_error(
                        ten_env,
                        f"Request ID mismatch in second tts_audio_end. Expected: {self.second_request_id}, Received: {received_request_id}",
                    )
                    return
                self.current_request_id = self.second_request_id
                self.current_metadata = self.second_metadata
                self.second_audio_end_received = True
                self.second_request_audio_bytes = self.current_request_audio_bytes
            else:
                self._stop_test_with_error(ten_env, f"Unexpected tts_audio_end state: {self.event_state}")
                return
            
            # Validate metadata
            metadata_str, _ = data.get_property_to_json("metadata")
            if metadata_str:
                try:
                    received_metadata = json.loads(metadata_str)
                    expected_metadata = {
                        "session_id": self.current_metadata.get("session_id", ""),
                        "turn_id": self.current_metadata.get("turn_id", -1),
                    }
                    if not self._validate_metadata(ten_env, received_metadata, expected_metadata, "tts_audio_end"):
                        return
                except json.JSONDecodeError:
                    self._stop_test_with_error(ten_env, f"Invalid JSON in tts_audio_end metadata: {metadata_str}")
                    return
            else:
                self._stop_test_with_error(ten_env, f"Missing metadata in tts_audio_end response")
                return
            
            # Validate reason is end_request
            received_reason, _ = data.get_property_string("reason")
            if received_reason != "end_request":
                self._stop_test_with_error(
                    ten_env,
                    f"Reason mismatch in tts_audio_end. Expected: end_request, Received: {received_reason}",
                )
                return
            
            # Validate audio duration
            if self.audio_start_time is not None:
                current_time = time.time()
                actual_duration_ms = (current_time - self.audio_start_time) * 1000
                
                # Get request_total_audio_duration_ms
                received_audio_duration_ms, _ = data.get_property_int("request_total_audio_duration_ms")
                
                # Calculate PCM duration based on current request audio bytes
                # Use current_request_audio_bytes which is already updated by audio frames
                pcm_audio_duration_ms = self._calculate_pcm_audio_duration_ms(self.current_request_audio_bytes)
                
                if pcm_audio_duration_ms > 0 and received_audio_duration_ms > 0:
                    audio_duration_diff = abs(received_audio_duration_ms - pcm_audio_duration_ms)
                    if audio_duration_diff > AUDIO_DURATION_TOLERANCE_MS:
                        self._stop_test_with_error(
                            ten_env,
                            f"Audio duration mismatch. PCM calculated: {pcm_audio_duration_ms}ms, Reported: {received_audio_duration_ms}ms, Diff: {audio_duration_diff}ms",
                        )
                        return
                    ten_env.log_info(
                        f"✅ Audio duration validation passed. PCM: {pcm_audio_duration_ms}ms, Reported: {received_audio_duration_ms}ms, Diff: {audio_duration_diff}ms"
                    )
                else:
                    ten_env.log_info(
                        f"Skipping audio duration validation - PCM: {pcm_audio_duration_ms}ms, Reported: {received_audio_duration_ms}ms"
                    )
                
                ten_env.log_info(f"Actual event duration: {actual_duration_ms:.2f}ms")
            else:
                ten_env.log_warn("tts_audio_start not received before tts_audio_end")
            
            ten_env.log_info(f"✅ tts_audio_end received with correct request_id, metadata, and reason")
            
            # If second audio end received, check dump files
            if self.event_state == EventSequenceState.COMPLETED:
                ten_env.log_info("✅ All events received in correct sequence, checking dump files")
                self._check_dump_file_number(ten_env)
            
            return

    def _check_dump_file_number(self, ten_env: AsyncTenEnvTester) -> None:
        """Check if there are exactly two dump files in the directory."""
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
        # Check event sequence
        self._check_event_sequence(ten_env, "audio_frame")
        
        # Check sample_rate
        sample_rate = audio_frame.get_sample_rate()
        ten_env.log_info(f"Received audio frame with sample_rate: {sample_rate}")

        # Store current test sample_rate
        if self.sample_rate == 0:
            self.sample_rate = sample_rate
            ten_env.log_info(f"First audio frame received with sample_rate: {sample_rate}")

        # Mark that audio frames have been received
        if self.event_state == EventSequenceState.RECEIVING_FIRST_AUDIO_FRAMES:
            self.first_audio_frames_received = True
        elif self.event_state == EventSequenceState.RECEIVING_SECOND_AUDIO_FRAMES:
            self.second_audio_frames_received = True

        # Accumulate audio bytes for duration calculation
        try:
            audio_data = audio_frame.get_buf()
            if audio_data:
                self.total_audio_bytes += len(audio_data)
                self.current_request_audio_bytes += len(audio_data)
                ten_env.log_info(
                    f"Audio frame size: {len(audio_data)} bytes, Current request: {self.current_request_audio_bytes} bytes, Total: {self.total_audio_bytes} bytes"
                )
        except Exception as e:
            ten_env.log_warn(f"Failed to get audio data: {e}")

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""
        ten_env.log_info("Test stopped")
        _delete_dump_file(self.tts_extension_dump_folder)


def _delete_dump_file(dump_path: str) -> None:
    """Delete all dump files in the specified path."""
    for file_path in glob.glob(os.path.join(dump_path, "*")):
        if os.path.isfile(file_path):
            os.remove(file_path)
        elif os.path.isdir(file_path):
            import shutil
            shutil.rmtree(file_path)


def test_append_input(extension_name: str, config_dir: str) -> None:
    """Verify TTS append input with multiple text inputs."""
    # Get config file path
    config_file_path = os.path.join(config_dir, TTS_DUMP_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Log test configuration
    print(f"Using test configuration: {config}")
    if not os.path.exists(config["dump_path"]):
        os.makedirs(config["dump_path"])
    else:
        # Delete all files in the directory
        _delete_dump_file(config["dump_path"])

    # Create and run tester
    tester = AppendInputTester(
        session_id="test_append_input_session_123",
    )

    # Set the tts_extension_dump_folder for the tester
    tester.tts_extension_dump_folder = config["dump_path"]

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"

