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

# Expected event sequence states for each request
class RequestState:
    """Track state for each request"""
    WAITING_AUDIO_START = "waiting_audio_start"
    RECEIVING_AUDIO_FRAMES = "receiving_audio_frames"
    WAITING_AUDIO_END = "waiting_audio_end"
    COMPLETED = "completed"


class InterleavedRequestsTester(AsyncExtensionTester):
    """Test class for TTS extension with interleaved requests"""

    def __init__(
        self,
        session_id: str = "test_interleaved_requests_session_123",
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: Interleaved Requests TTS Test")
        print("=" * 80)
        print("📋 Test Description: Validate TTS with interleaved requests (request_id 1, 2, 1, 3, 1)")
        print("🎯 Test Objectives:")
        print("   - Verify interleaved requests with different request_ids")
        print("   - Verify request_id 1 sends multiple messages without text_input_end, then completes")
        print("   - Verify request_id 2 and 3 send single message with text_input_end")
        print("   - Verify strict event sequence order")
        print("   - Verify dump file generation")
        print("=" * 80)

        self.session_id: str = session_id
        self.dump_file_name = f"tts_interleaved_requests_{self.session_id}.pcm"
        
        # Define request IDs
        self.request_id_1 = "test_interleaved_request_id_1"
        self.request_id_2 = "test_interleaved_request_id_2"
        self.request_id_3 = "test_interleaved_request_id_3"
        
        # Define messages for each request
        # Request 1: multiple messages, only last one has text_input_end
        self.request_1_messages = [
            ("Hello world, this is the first message for request 1.", False),
            ("This is the second message for request 1.", False),
            ("This is the third message for request 1.", False),
            ("This is the fourth message for request 1 after request 2.", False),
            ("This is the fifth message for request 1 after request 2.", False),
            ("This is the final message for request 1 after request 3.", True),  # text_input_end = True
        ]
        
        # Request 2: single message with text_input_end
        self.request_2_messages = [
            ("This is the only message for request 2.", True),
        ]
        
        # Request 3: single message with text_input_end
        self.request_3_messages = [
            ("This is the only message for request 3.", True),
        ]
        
        # Define sending sequence: (request_id, message_index)
        # Sequence: request_1 (first 3) -> request_2 (1) -> request_1 (next 2) -> request_3 (1) -> request_1 (last 1)
        self.sending_sequence = [
            (self.request_id_1, 0),  # First message of request 1
            (self.request_id_1, 1),  # Second message of request 1
            (self.request_id_1, 2),  # Third message of request 1
            (self.request_id_2, 0),  # Only message of request 2
            (self.request_id_1, 3),  # Fourth message of request 1
            (self.request_id_1, 4),  # Fifth message of request 1
            (self.request_id_3, 0),  # Only message of request 3
            (self.request_id_1, 5),  # Final message of request 1
        ]
        
        # Expected request IDs that should complete
        self.expected_request_ids = [self.request_id_1, self.request_id_2, self.request_id_3]
        
        # Request metadata
        self.request_metadata = {
            self.request_id_1: {
                "session_id": self.session_id,
                "turn_id": 1,
            },
            self.request_id_2: {
                "session_id": self.session_id,
                "turn_id": 2,
            },
            self.request_id_3: {
                "session_id": self.session_id,
                "turn_id": 3,
            },
        }
        
        # Event sequence tracking - track state for each request
        self.request_states = {
            self.request_id_1: RequestState.WAITING_AUDIO_START,
            self.request_id_2: RequestState.WAITING_AUDIO_START,
            self.request_id_3: RequestState.WAITING_AUDIO_START,
        }
        self.audio_start_received = {
            self.request_id_1: False,
            self.request_id_2: False,
            self.request_id_3: False,
        }
        self.audio_frames_received = {
            self.request_id_1: False,
            self.request_id_2: False,
            self.request_id_3: False,
        }
        self.audio_end_received = {
            self.request_id_1: False,
            self.request_id_2: False,
            self.request_id_3: False,
        }
        
        # Track which requests are currently active (waiting for audio)
        self.active_requests = []  # List of request_ids that are waiting for audio
        
        # Audio tracking
        self.current_request_id = None
        self.current_metadata = None
        self.audio_start_time = None
        self.total_audio_bytes = 0
        self.request_audio_bytes = {
            self.request_id_1: 0,
            self.request_id_2: 0,
            self.request_id_3: 0,
        }
        self.current_request_audio_bytes = 0
        self.sample_rate = 0

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
        """Start the TTS interleaved requests test - send messages in interleaved order."""
        ten_env.log_info("Starting TTS interleaved requests test")
        
        # Send messages according to the sequence
        for seq_idx, (request_id, msg_idx) in enumerate(self.sending_sequence):
            if request_id == self.request_id_1:
                text, text_input_end = self.request_1_messages[msg_idx]
            elif request_id == self.request_id_2:
                text, text_input_end = self.request_2_messages[msg_idx]
            elif request_id == self.request_id_3:
                text, text_input_end = self.request_3_messages[msg_idx]
            else:
                self._stop_test_with_error(ten_env, f"Unknown request_id: {request_id}")
                return
            
            metadata = self.request_metadata[request_id]
            ten_env.log_info(
                f"Sending message {seq_idx + 1}/{len(self.sending_sequence)}: "
                f"request_id={request_id}, text_input_end={text_input_end}, text={text[:50]}..."
            )
            
            await self._send_tts_text_input(
                ten_env, text, request_id, metadata, text_input_end
            )
        
        ten_env.log_info(f"✅ All {len(self.sending_sequence)} messages sent in interleaved order")

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

    def _check_event_sequence(self, ten_env: AsyncTenEnvTester, received_event: str, request_id: str) -> None:
        """Check if received event matches expected sequence for the request."""
        if request_id not in self.request_states:
            self._stop_test_with_error(ten_env, f"Received event {received_event} for unknown request_id: {request_id}")
            return
        
        current_state = self.request_states[request_id]
        error_msg = None
        
        if received_event == "tts_audio_start":
            if current_state == RequestState.WAITING_AUDIO_START:
                # Expected audio start for this request
                self.request_states[request_id] = RequestState.RECEIVING_AUDIO_FRAMES
                if request_id not in self.active_requests:
                    self.active_requests.append(request_id)
            else:
                error_msg = f"Unexpected tts_audio_start for request_id {request_id} in state: {current_state}"
        elif received_event == "audio_frame":
            if current_state == RequestState.RECEIVING_AUDIO_FRAMES:
                # Expected audio frames for this request
                pass
            else:
                error_msg = f"Unexpected audio_frame for request_id {request_id} in state: {current_state}"
        elif received_event == "tts_audio_end":
            if current_state == RequestState.RECEIVING_AUDIO_FRAMES:
                # Expected audio end for this request
                self.request_states[request_id] = RequestState.COMPLETED
                if request_id in self.active_requests:
                    self.active_requests.remove(request_id)
            else:
                error_msg = f"Unexpected tts_audio_end for request_id {request_id} in state: {current_state}"
        
        if error_msg:
            self._stop_test_with_error(ten_env, error_msg)

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from TTS extension."""
        name: str = data.get_name()
        ten_env.log_info(f"Received data: {name}")

        if name == "error":
            json_str, _ = data.get_property_to_json("")
            ten_env.log_info(f"Received error data: {json_str}")
            self._stop_test_with_error(ten_env, f"Received error data: {json_str}")
            return
        elif name == "tts_audio_start":
            # Get request_id
            received_request_id, _ = data.get_property_string("request_id")
            
            # Check event sequence
            self._check_event_sequence(ten_env, "tts_audio_start", received_request_id)
            
            ten_env.log_info(f"Received tts_audio_start for request_id: {received_request_id}")
            self.audio_start_time = time.time()
            
            # Validate request_id
            if received_request_id not in self.expected_request_ids:
                self._stop_test_with_error(
                    ten_env,
                    f"Unexpected request_id in tts_audio_start: {received_request_id}",
                )
                return
            
            self.current_request_id = received_request_id
            self.current_metadata = self.request_metadata[received_request_id]
            self.audio_start_received[received_request_id] = True
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
            # Get request_id
            received_request_id, _ = data.get_property_string("request_id")
            
            # Check event sequence
            self._check_event_sequence(ten_env, "tts_audio_end", received_request_id)
            
            ten_env.log_info(f"Received tts_audio_end for request_id: {received_request_id}")
            
            # Validate request_id
            if received_request_id not in self.expected_request_ids:
                self._stop_test_with_error(
                    ten_env,
                    f"Unexpected request_id in tts_audio_end: {received_request_id}",
                )
                return
            
            self.current_request_id = received_request_id
            self.current_metadata = self.request_metadata[received_request_id]
            self.audio_end_received[received_request_id] = True
            self.request_audio_bytes[received_request_id] = self.current_request_audio_bytes
            
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
            
            ten_env.log_info(f"✅ tts_audio_end received with correct request_id, metadata, and reason for request_id: {received_request_id}")
            
            # If all requests completed, check dump files
            if all(self.audio_end_received.values()):
                ten_env.log_info(f"✅ All {len(self.expected_request_ids)} requests completed, checking dump files")
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
        
        # Get all files in the directory
        time.sleep(1)
        dump_files = []
        for file_path in glob.glob(os.path.join(self.tts_extension_dump_folder, "*")):
            if os.path.isfile(file_path):
                dump_files.append(file_path)

        ten_env.log_info(f"Found {len(dump_files)} dump files in {self.tts_extension_dump_folder}")
        for i, file_path in enumerate(dump_files):
            ten_env.log_info(f"  {i+1}. {os.path.basename(file_path)}")
        
        # Check if there are exactly expected number of dump files (one per request)
        expected_count = len(self.expected_request_ids)
        if len(dump_files) == expected_count:
            ten_env.log_info(f"✅ Found exactly {expected_count} dump files as expected (one per request)")
            ten_env.stop_test()
        elif len(dump_files) > expected_count:
            self._stop_test_with_error(ten_env, f"Found {len(dump_files)} dump files, expected exactly {expected_count} (one per request)")
        else:
            self._stop_test_with_error(ten_env, f"Found {len(dump_files)} dump files, expected exactly {expected_count} (one per request)")

    @override
    async def on_audio_frame(self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame) -> None:
        """Handle received audio frame from TTS extension."""
        # Get request_id from current_request_id (set in tts_audio_start)
        if self.current_request_id is None:
            ten_env.log_warn("Received audio_frame but current_request_id is None")
            return
        
        request_id = self.current_request_id
        
        # Check event sequence
        self._check_event_sequence(ten_env, "audio_frame", request_id)
        
        # Check sample_rate
        sample_rate = audio_frame.get_sample_rate()
        ten_env.log_info(f"Received audio frame with sample_rate: {sample_rate} for request_id: {request_id}")

        # Store current test sample_rate
        if self.sample_rate == 0:
            self.sample_rate = sample_rate
            ten_env.log_info(f"First audio frame received with sample_rate: {sample_rate}")

        # Mark that audio frames have been received for current request
        if request_id in self.audio_frames_received:
            self.audio_frames_received[request_id] = True

        # Accumulate audio bytes for duration calculation
        try:
            audio_data = audio_frame.get_buf()
            if audio_data:
                self.total_audio_bytes += len(audio_data)
                self.current_request_audio_bytes += len(audio_data)
                self.request_audio_bytes[request_id] += len(audio_data)
                ten_env.log_info(
                    f"Audio frame size: {len(audio_data)} bytes, Current request: {self.current_request_audio_bytes} bytes, "
                    f"Request {request_id}: {self.request_audio_bytes[request_id]} bytes, Total: {self.total_audio_bytes} bytes"
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


def test_interleaved_requests(extension_name: str, config_dir: str) -> None:
    """Verify TTS with interleaved requests (request_id 1, 2, 1, 3, 1)."""
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

    # Create and run tester
    tester = InterleavedRequestsTester(
        session_id="test_interleaved_requests_session_123",
    )

    # Set the tts_extension_dump_folder for the tester
    tester.tts_extension_dump_folder = config["dump_path"]

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"

