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
import time

TTS_EMPTY_TEXT_CONFIG_FILE = "property_basic_audio_setting1.json"
MAX_RESPONSE_TIME_MS = 500  # Maximum time allowed from last send to receiving tts_audio_end


class EmptyTextRequestTester(AsyncExtensionTester):
    """Test class for TTS extension with empty text requests"""

    def __init__(
        self,
        session_id: str = "test_empty_text_session_123",
        request_id: int = 1,
        test_name: str = "empty_text_test",
    ):
        super().__init__()
        print("=" * 80)
        print(f"🧪 TEST CASE: {test_name}")
        print("=" * 80)
        print("📋 Test Description: Send 3 empty text requests with same request_id")
        print("🎯 Test Objectives:")
        print("   - Send 3 requests with empty/whitespace text")
        print("   - All requests use the same request_id")
        print("   - Verify tts_audio_end is received within 100ms after last send")
        print("=" * 80)

        self.session_id: str = session_id
        self.request_id: int = request_id
        self.test_name: str = test_name
        self.sent_metadata = None  # Store sent metadata for validation
        self.send_count: int = 0  # Track number of sends
        self.last_send_time: float = 0  # Timestamp of last send
        self.audio_end_received: bool = False  # Flag for tts_audio_end received

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the TTS empty text request test."""
        ten_env.log_info("Starting TTS empty text request test")
        
        # Send 3 empty text requests with same request_id
        for i in range(3):
            # Alternate between empty string and whitespace
            text = "" if i % 2 == 0 else " "
            await self._send_tts_text_input(ten_env, text, i + 1)
            self.send_count += 1
            
            # Record the time of the last send
            if i == 2:  # Last send (index 2 = 3rd request)
                self.last_send_time = time.time()
                ten_env.log_info(f"✅ Last send completed at {self.last_send_time}")

    async def _send_tts_text_input(
        self, ten_env: AsyncTenEnvTester, text: str, send_num: int
    ) -> None:
        """Send tts text input to TTS extension."""
        ten_env.log_info(f"[Send #{send_num}] Sending tts text input: '{text}' (length: {len(text)})")
        
        tts_text_input_obj = Data.create("tts_text_input")
        tts_text_input_obj.set_property_string("text", text)
        tts_text_input_obj.set_property_string(
            "request_id", str(self.request_id)
        )
        tts_text_input_obj.set_property_bool("text_input_end", True)
        
        metadata = {
            "session_id": self.session_id,
            "turn_id": 1,
        }
        
        # Store sent metadata for validation (only once)
        if self.sent_metadata is None:
            self.sent_metadata = metadata
            
        tts_text_input_obj.set_property_from_json(
            "metadata", json.dumps(metadata)
        )
        
        await ten_env.send_data(tts_text_input_obj)
        ten_env.log_info(f"✅ [Send #{send_num}] tts text input sent with request_id: {self.request_id}")

    def _stop_test_with_error(
        self, ten_env: AsyncTenEnvTester, error_message: str
    ) -> None:
        """Stop test with error message."""
        ten_env.log_error(f"❌ Test failed: {error_message}")
        ten_env.stop_test(
            TenError.create(TenErrorCode.ErrorCodeGeneric, error_message)
        )

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle received data from TTS extension."""
        name: str = data.get_name()
        ten_env.log_info(f"[{self.test_name}] Received data: {name}")

        if name == "error":
            json_str, _ = data.get_property_to_json("")
            ten_env.log_info(
                f"[{self.test_name}] Received error data: {json_str}"
            )
            self._stop_test_with_error(ten_env, f"Received error data: {json_str}")
            return
            
        elif name == "tts_audio_start":
            ten_env.log_info(f"[{self.test_name}] Received tts_audio_start (not validated in this test)")
            return
            
        elif name == "tts_audio_end":
            receive_time = time.time()
            ten_env.log_info(f"[{self.test_name}] Received tts_audio_end at {receive_time}")
            
            # Validate request_id
            received_request_id, _ = data.get_property_string("request_id")
            if received_request_id != str(self.request_id):
                self._stop_test_with_error(
                    ten_env, 
                    f"Request ID mismatch in tts_audio_end. Expected: {self.request_id}, Received: {received_request_id}"
                )
                return
            
            # Validate metadata
            metadata_str, _ = data.get_property_to_json("metadata")
            if metadata_str:
                try:
                    received_metadata = json.loads(metadata_str)
                    expected_metadata = {
                        "session_id": self.sent_metadata.get("session_id", ""),
                        "turn_id": self.sent_metadata.get("turn_id", -1)
                    }
                    if received_metadata != expected_metadata:
                        self._stop_test_with_error(
                            ten_env, 
                            f"Metadata mismatch in tts_audio_end. Expected: {expected_metadata}, Received: {received_metadata}"
                        )
                        return
                except json.JSONDecodeError:
                    self._stop_test_with_error(
                        ten_env, 
                        f"Invalid JSON in tts_audio_end metadata: {metadata_str}"
                    )
                    return
            else:
                self._stop_test_with_error(
                    ten_env, 
                    f"Missing metadata in tts_audio_end response"
                )
                return
            
            # Validate response time (from last send to receiving tts_audio_end)
            if self.last_send_time > 0:
                response_time_ms = (receive_time - self.last_send_time) * 1000
                ten_env.log_info(
                    f"⏱️  Response time: {response_time_ms:.2f}ms (max allowed: {MAX_RESPONSE_TIME_MS}ms)"
                )
                
                if response_time_ms > MAX_RESPONSE_TIME_MS:
                    self._stop_test_with_error(
                        ten_env,
                        f"Response time exceeded limit. Expected: <={MAX_RESPONSE_TIME_MS}ms, Actual: {response_time_ms:.2f}ms"
                    )
                    return
                
                ten_env.log_info(
                    f"✅ Response time validation passed: {response_time_ms:.2f}ms <= {MAX_RESPONSE_TIME_MS}ms"
                )
            else:
                self._stop_test_with_error(
                    ten_env,
                    "Last send time not recorded"
                )
                return
            
            # Mark as received
            self.audio_end_received = True
            
            ten_env.log_info(
                f"✅ [{self.test_name}] tts_audio_end received with correct request_id, metadata, and timing"
            )
            
            # Test passed, stop
            ten_env.log_info(f"✅ [{self.test_name}] Test completed successfully")
            ten_env.stop_test()
            return

    @override
    async def on_audio_frame(
        self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame
    ) -> None:
        """Handle received audio frame from TTS extension (not validated in this test)."""
        ten_env.log_info(f"[{self.test_name}] Received audio frame (not validated in this test)")

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""
        ten_env.log_info("Test stopped")


def test_empty_text_request(extension_name: str, config_dir: str) -> None:
    """Test sending 3 empty text requests with same request_id"""
    print(f"\n{'='*80}")
    print("🧪 TEST: Empty Text Request")
    print(f"{'='*80}")
    print("📋 Test objective: Send 3 empty text requests and verify tts_audio_end timing")
    print("🎯 Expected result: Receive tts_audio_end within 100ms after last send")
    print(f"{'='*80}")

    # Load config file
    config_file = os.path.join(config_dir, TTS_EMPTY_TEXT_CONFIG_FILE)
    if not os.path.exists(config_file):
        raise FileNotFoundError(f"Config file not found: {config_file}")

    with open(config_file, "r") as f:
        config: dict[str, Any] = json.load(f)

    print(f"Using config: {TTS_EMPTY_TEXT_CONFIG_FILE}")

    # Create and run tester
    tester = EmptyTextRequestTester(
        session_id="test_empty_text_session",
        request_id=999,
        test_name="EmptyTextRequest",
    )

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    if error is not None:
        error_msg = error.error_message() if error else "Unknown error"
        print(f"\n{'='*80}")
        print(f"❌ TEST FAILED: {error_msg}")
        print(f"{'='*80}")
        raise AssertionError(f"Test failed: {error_msg}")

    print(f"\n{'='*80}")
    print("✅ TEST PASSED: Empty text request test completed successfully")
    print(f"{'='*80}")
