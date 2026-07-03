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
import random
import uuid
import string

TTS_DUMP_CONFIG_FILE = "property_dump.json"
AUDIO_DURATION_TOLERANCE_MS = 50

# Expected event sequence states for each group
class GroupState:
    """Track state for each group"""
    WAITING_AUDIO_START = "waiting_audio_start"
    RECEIVING_AUDIO_FRAMES = "receiving_audio_frames"
    WAITING_AUDIO_END = "waiting_audio_end"
    COMPLETED = "completed"


class AppendInputStressTester(AsyncExtensionTester):
    """Test class for TTS extension append input stress test with simple data"""

    def __init__(
        self,
        session_id: str = "test_append_input_stress_session_123",
        num_requests: int = 100,
        random_seed: int | None = None,
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: Append Input TTS Stress Test")
        print("=" * 80)
        print("📋 Test Description: Stress test TTS append input with multiple requests")
        print("🎯 Test Objectives:")
        print("   - Stress test with random request IDs")
        print("   - Verify append input with multiple requests (one text per request)")
        print("   - Verify strict event sequence order under stress")
        print("   - Verify dump file generation")
        print("=" * 80)
        print(f"📊 Stress Test Configuration:")
        print(f"   - Number of requests: {num_requests}")
        print(f"   - Text format: hello word1, hello word2, ..., hello word{num_requests}")
        if random_seed is not None:
            print(f"   - Random seed: {random_seed}")
        print("=" * 80)

        self.session_id: str = session_id
        self.dump_file_name = f"tts_append_input_stress_{self.session_id}.pcm"
        
        # Set random seed for reproducibility (if provided)
        if random_seed is not None:
            random.seed(random_seed)
            print(f"🌱 Random seed set to: {random_seed}")
        
        # Simple text generation: each request has one text "hello word{i+1}"
        self.expected_group_count = num_requests
        self.texts = [f"hello word{i+1}" for i in range(num_requests)]
        
        # No empty groups in simplified version
        self.empty_groups = [False] * num_requests
        
        # Generate random request IDs for all requests
        self.request_ids = [
            self._generate_random_request_id(i) for i in range(num_requests)
        ]
        self.metadatas = [
            {
                "session_id": self.session_id,
                "turn_id": i + 1,
            }
            for i in range(num_requests)
        ]
        
        # Event sequence tracking - track state for each request
        self.current_group_index = 0
        self.group_states = [GroupState.WAITING_AUDIO_START] * num_requests
        self.audio_start_received = [False] * num_requests
        self.audio_frames_received = [False] * num_requests
        self.audio_end_received = [False] * num_requests
        
        # Audio tracking
        self.current_request_id = None
        self.current_metadata = None
        self.audio_start_time = None
        self.total_audio_bytes = 0
        self.current_request_audio_bytes = 0
        self.sample_rate = 0
        self.request_audio_bytes = [0] * self.expected_group_count

    def _generate_random_request_id(self, request_index: int) -> str:
        """Generate a random request ID for stress testing."""
        # Use UUID for uniqueness, but keep it readable
        uuid_part = str(uuid.uuid4())[:8]
        random_suffix = ''.join(random.choices(string.ascii_lowercase + string.digits, k=6))
        return f"stress_req_{request_index}_{uuid_part}_{random_suffix}"

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
        ten_env.log_info("Starting TTS append input stress test")
        
        # Send all requests sequentially, each request has one text
        for request_idx in range(self.expected_group_count):
            request_id = self.request_ids[request_idx]
            metadata = self.metadatas[request_idx]
            text = self.texts[request_idx]
            
            ten_env.log_info(f"Sending request {request_idx + 1}/{self.expected_group_count}: {text} (request_id: {request_id})")
            
            # Each request has only one text, so text_input_end is always True
            time.sleep(5)
            await self._send_tts_text_input(
                ten_env, text, request_id, metadata, text_input_end=True
            )
        
        ten_env.log_info(f"✅ All {self.expected_group_count} requests sent sequentially")

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
        """Check if received event matches expected sequence for current group."""
        if self.current_group_index >= self.expected_group_count:
            self._stop_test_with_error(ten_env, f"Received event {received_event} but all {self.expected_group_count} groups are completed")
            return
        
        # Skip empty groups
        while self.current_group_index < self.expected_group_count and self.empty_groups[self.current_group_index]:
            ten_env.log_info(f"Skipping empty group {self.current_group_index + 1}")
            self.current_group_index += 1
            if self.current_group_index < self.expected_group_count:
                self.group_states[self.current_group_index] = GroupState.WAITING_AUDIO_START
        
        if self.current_group_index >= self.expected_group_count:
            self._stop_test_with_error(ten_env, f"Received event {received_event} but all groups are completed")
            return
        
        current_state = self.group_states[self.current_group_index]
        error_msg = None
        
        if received_event == "tts_audio_start":
            if current_state == GroupState.WAITING_AUDIO_START:
                # Expected audio start for current group
                self.group_states[self.current_group_index] = GroupState.RECEIVING_AUDIO_FRAMES
            else:
                error_msg = f"Unexpected tts_audio_start for group {self.current_group_index + 1} in state: {current_state}"
        elif received_event == "audio_frame":
            if current_state == GroupState.RECEIVING_AUDIO_FRAMES:
                # Expected audio frames for current group
                pass
            else:
                error_msg = f"Unexpected audio_frame for group {self.current_group_index + 1} in state: {current_state}"
        elif received_event == "tts_audio_end":
            if current_state == GroupState.RECEIVING_AUDIO_FRAMES:
                # Expected audio end for current group
                self.group_states[self.current_group_index] = GroupState.COMPLETED
                # Move to next non-empty group
                if self.current_group_index < self.expected_group_count - 1:
                    self.current_group_index += 1
                    # Skip empty groups
                    while self.current_group_index < self.expected_group_count and self.empty_groups[self.current_group_index]:
                        ten_env.log_info(f"Skipping empty group {self.current_group_index + 1}")
                        self.current_group_index += 1
                    if self.current_group_index < self.expected_group_count:
                        self.group_states[self.current_group_index] = GroupState.WAITING_AUDIO_START
            else:
                error_msg = f"Unexpected tts_audio_end for group {self.current_group_index + 1} in state: {current_state}"
        
        if error_msg:
            self._stop_test_with_error(ten_env, error_msg)

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from TTS extension."""
        name: str = data.get_name()
        current_state = self.group_states[self.current_group_index] if self.current_group_index < len(self.group_states) else "unknown"
        ten_env.log_info(f"Received data: {name} (current group: {self.current_group_index + 1}, state: {current_state})")

        if name == "error":
            json_str, _ = data.get_property_to_json("")
            ten_env.log_info(f"Received error data: {json_str}")
            self._stop_test_with_error(ten_env, f"Received error data: {json_str}")
            return
        elif name == "tts_audio_start":
            # Check event sequence
            self._check_event_sequence(ten_env, "tts_audio_start")
            
            ten_env.log_info(f"Received tts_audio_start for group {self.current_group_index + 1}")
            self.audio_start_time = time.time()
            
            # Get request_id and validate
            received_request_id, _ = data.get_property_string("request_id")
            expected_request_id = self.request_ids[self.current_group_index]
            
            if received_request_id != expected_request_id:
                self._stop_test_with_error(
                    ten_env,
                    f"Request ID mismatch in group {self.current_group_index + 1} tts_audio_start. Expected: {expected_request_id}, Received: {received_request_id}",
                )
                return
            
            self.current_request_id = expected_request_id
            self.current_metadata = self.metadatas[self.current_group_index]
            self.audio_start_received[self.current_group_index] = True
            self.current_request_audio_bytes = 0
            
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
            # Check event sequence (this will update current_group_index if needed)
            # Save current group index before check, as it might be updated
            group_idx_before_check = self.current_group_index
            self._check_event_sequence(ten_env, "tts_audio_end")
            
            ten_env.log_info(f"Received tts_audio_end for group {group_idx_before_check + 1}")
            
            # Get request_id and validate
            received_request_id, _ = data.get_property_string("request_id")
            expected_request_id = self.request_ids[group_idx_before_check]
            
            if received_request_id != expected_request_id:
                self._stop_test_with_error(
                    ten_env,
                    f"Request ID mismatch in group {group_idx_before_check + 1} tts_audio_end. Expected: {expected_request_id}, Received: {received_request_id}",
                )
                return
            
            self.current_request_id = expected_request_id
            self.current_metadata = self.metadatas[group_idx_before_check]
            self.audio_end_received[group_idx_before_check] = True
            self.request_audio_bytes[group_idx_before_check] = self.current_request_audio_bytes
            
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
            
            # Validate reason is REQUEST_END (1)
            # TTSAudioEndReason.REQUEST_END = 1
            received_reason, _ = data.get_property_int("reason")
            if received_reason != 1:
                self._stop_test_with_error(
                    ten_env,
                    f"Reason mismatch in tts_audio_end. Expected: 1 (REQUEST_END), Received: {received_reason}",
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
            
            ten_env.log_info(f"✅ tts_audio_end received with correct request_id, metadata, and reason for group {group_idx_before_check + 1}")
            
            # If all groups completed, check dump files
            if all(self.audio_end_received):
                ten_env.log_info(f"✅ All {self.expected_group_count} groups completed, checking dump files")
                self._check_dump_file_number(ten_env)
            
            return

    def _check_dump_file_number(self, ten_env: AsyncTenEnvTester) -> None:
        """Check if there are exactly expected number of dump files in the directory."""
        if not hasattr(self, 'tts_extension_dump_folder') or not self.tts_extension_dump_folder:
            ten_env.log_error("tts_extension_dump_folder not set")
            self._stop_test_with_error(ten_env, "tts_extension_dump_folder not configured")
            return

        if not os.path.exists(self.tts_extension_dump_folder):
            self._stop_test_with_error(ten_env, f"TTS extension dump folder not found: {self.tts_extension_dump_folder}")
            return
        
        # Calculate expected count
        expected_count = sum(1 for is_empty in self.empty_groups if not is_empty)
        
        # Retry mechanism: wait for dump files to be written with exponential backoff
        max_retries = 10
        retry_delay = 0.5  # Start with 0.5 seconds
        dump_files = []
        
        for retry in range(max_retries):
            # Wait before checking
            if retry > 0:
                time.sleep(retry_delay)
                retry_delay = min(retry_delay * 1.5, 2.0)  # Exponential backoff, max 2 seconds
            
            # Get all files in the directory
            dump_files = []
            for file_path in glob.glob(os.path.join(self.tts_extension_dump_folder, "*")):
                if os.path.isfile(file_path):
                    dump_files.append(file_path)
            
            ten_env.log_info(f"Attempt {retry + 1}/{max_retries}: Found {len(dump_files)} dump files (expected {expected_count})")
            
            # If we found the expected number, break
            if len(dump_files) == expected_count:
                break
        
        # Log all found files
        ten_env.log_info(f"Found {len(dump_files)} dump files in {self.tts_extension_dump_folder}")
        for i, file_path in enumerate(dump_files):
            ten_env.log_info(f"  {i+1}. {os.path.basename(file_path)}")
        
        # Check if there are exactly expected number of dump files (one per non-empty group)
        if len(dump_files) == expected_count:
            ten_env.log_info(f"✅ Found exactly {expected_count} dump files as expected (one per non-empty group)")
            ten_env.stop_test()
        elif len(dump_files) > expected_count:
            self._stop_test_with_error(ten_env, f"Found {len(dump_files)} dump files, expected exactly {expected_count} (one per non-empty group)")
        else:
            self._stop_test_with_error(ten_env, f"Found {len(dump_files)} dump files, expected exactly {expected_count} (one per non-empty group). Last dump file may not have been written in time.")

    @override
    async def on_audio_frame(self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame) -> None:
        """Handle received audio frame from TTS extension."""
        # Check event sequence
        self._check_event_sequence(ten_env, "audio_frame")
        
        # Check sample_rate
        sample_rate = audio_frame.get_sample_rate()
        ten_env.log_info(f"Received audio frame with sample_rate: {sample_rate} for group {self.current_group_index + 1}")

        # Store current test sample_rate
        if self.sample_rate == 0:
            self.sample_rate = sample_rate
            ten_env.log_info(f"First audio frame received with sample_rate: {sample_rate}")

        # Mark that audio frames have been received for current group
        if self.current_group_index < len(self.audio_frames_received):
            self.audio_frames_received[self.current_group_index] = True

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
        # Keep dump files for inspection
        # _delete_dump_file(self.tts_extension_dump_folder)


def _delete_dump_file(dump_path: str) -> None:
    """Delete all dump files in the specified path."""
    for file_path in glob.glob(os.path.join(dump_path, "*")):
        if os.path.isfile(file_path):
            os.remove(file_path)
        elif os.path.isdir(file_path):
            import shutil
            shutil.rmtree(file_path)


def test_append_input_stress(
    extension_name: str,
    config_dir: str,
    num_requests: int = 20,
    random_seed: int | None = None,
    timeout_seconds: int | None = None,
) -> None:
    """Stress test TTS append input with multiple simple requests.
    
    Each request has one text: "hello word1", "hello word2", ..., "hello wordN"
    
    Args:
        extension_name: Name of the extension to test
        config_dir: Directory containing the config file
        num_requests: Number of requests to send (default: 20)
        random_seed: Random seed for request ID generation (default: None)
        timeout_seconds: Test timeout in seconds. If None, calculated based on num_requests (default: None)
    """
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
        # Delete all files in the directory before test to avoid interference from previous test
        _delete_dump_file(config["dump_path"])

    # Generate a unique session ID for this stress test run
    session_id = f"test_append_input_stress_{uuid.uuid4().hex[:8]}"

    # Create and run tester with stress test parameters
    tester = AppendInputStressTester(
        session_id=session_id,
        num_requests=num_requests,
        random_seed=random_seed,
    )

    # Set the tts_extension_dump_folder for the tester
    tester.tts_extension_dump_folder = config["dump_path"]

    tester.set_test_mode_single(extension_name, json.dumps(config))
    
    # Set timeout: calculate based on num_requests if not provided
    # Estimate: ~3-5 seconds per request (including audio generation time)
    if timeout_seconds is None:
        # Base timeout: 30 seconds + 5 seconds per request
        timeout_seconds = 30 + (num_requests * 5)
        # Cap at 10 minutes for very large tests
        timeout_seconds = min(timeout_seconds, 10 * 60)
    
    # Convert seconds to microseconds (set_timeout expects microseconds)
    timeout_microseconds = timeout_seconds * 1000 * 1000
    tester.set_timeout(timeout_microseconds)
    print(f"⏱️  Test timeout set to: {timeout_seconds} seconds ({timeout_seconds // 60} minutes)")
    
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"

