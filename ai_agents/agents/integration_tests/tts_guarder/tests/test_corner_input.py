
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

TTS_CORNER_INPUT_CONFIG_FILE="property_basic_audio_setting1.json"


class ConerTester(AsyncExtensionTester):
    """Test class for TTS extension corner input"""

    def __init__(
        self,
        session_id: str = "test_corner_input_session_123",
        text: str = "",
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: TTS Corner Input Test")
        print("=" * 80)
        print(
            "📋 Test Description: Validate TTS corner input"
        )
        print("🎯 Test Objectives:")
        print("   - Verify corner input is generated")
        print("=" * 80)

        self.session_id: str = session_id
        self.text: str = text
        self.receive_metircs = False
        self.sent_metadata = None  # Store sent metadata for validation
        self.request_id = "test_corner_input_request_id_1"
        self.audio_start_time = None  # Store tts_audio_start timestamp
        self.total_audio_bytes = 0  # Track total audio bytes received
        self.sample_rate = 0  # Store sample rate

    def _calculate_pcm_audio_duration_ms(self) -> int:
        """Calculate PCM audio duration in milliseconds based on received audio bytes"""
        if self.total_audio_bytes == 0 or self.sample_rate == 0:
            return 0
        
        # PCM format: 16-bit (2 bytes per sample), mono (1 channel)
        bytes_per_sample = 2
        channels = 1
        
        # Calculate duration in seconds, then convert to milliseconds
        duration_sec = self.total_audio_bytes / (self.sample_rate * bytes_per_sample * channels)
        return int(duration_sec * 1000)

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

    async def _send_tts_text_input(self, ten_env: AsyncTenEnvTester, text: str) -> None:
        """Send tts text input to TTS extension."""
        ten_env.log_info(f"Sending tts text input: {text}")
        tts_text_input_obj = Data.create("tts_text_input")
        tts_text_input_obj.set_property_string("text", text)
        tts_text_input_obj.set_property_string("request_id", "test_corner_input_request_id_1")
        tts_text_input_obj.set_property_bool("text_input_end", True)
        metadata = {
            "session_id": "test_corner_input_session_123",
            "turn_id": 1,
        }
        # Store sent metadata for validation
        self.sent_metadata = metadata
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
        json_str, _ = data.get_property_to_json("")
        ten_env.log_info(f"test extension Received data {name} as: {json_str}")

        if name == "error":
            self._stop_test_with_error(ten_env, f"Received error data")
            return
        elif name == "metrics":
            self.receive_metircs = True
        elif name == "tts_audio_start":
            ten_env.log_info("Received tts_audio_start")
            self.audio_start_time = time.time()
            
            # 校验request_id
            received_request_id, _ = data.get_property_string("request_id")
            if received_request_id != self.request_id:
                self._stop_test_with_error(ten_env, f"Request ID mismatch in tts_audio_start. Expected: {self.request_id}, Received: {received_request_id}")
                return
            
            # 校验metadata (基类实现中tts_audio_start的metadata只包含session_id和turn_id)
            metadata_str, _ = data.get_property_to_json("metadata")
            if metadata_str:
                try:
                    received_metadata = json.loads(metadata_str)
                    expected_metadata = {
                        "session_id": self.sent_metadata.get("session_id", ""),
                        "turn_id": self.sent_metadata.get("turn_id", -1)
                    }
                    if received_metadata != expected_metadata:
                        self._stop_test_with_error(ten_env, f"Metadata mismatch in tts_audio_start. Expected: {expected_metadata}, Received: {received_metadata}")
                        return
                except json.JSONDecodeError:
                    self._stop_test_with_error(ten_env, f"Invalid JSON in tts_audio_start metadata: {metadata_str}")
                    return
            else:
                self._stop_test_with_error(ten_env, f"Missing metadata in tts_audio_start response")
                return
            
            ten_env.log_info(f"✅ tts_audio_start received with correct request_id and metadata")
            return
        elif name == "tts_audio_end":
            # 校验request_id
            received_request_id, _ = data.get_property_string("request_id")
            if received_request_id != self.request_id:
                self._stop_test_with_error(ten_env, f"Request ID mismatch. Expected: {self.request_id}, Received: {received_request_id}")
                return
            
            # 校验metadata (基类实现中tts_audio_end的metadata只包含session_id和turn_id)
            metadata_str, _ = data.get_property_to_json("metadata")
            if metadata_str:
                try:
                    received_metadata = json.loads(metadata_str)
                    expected_metadata = {
                        "session_id": self.sent_metadata.get("session_id", ""),
                        "turn_id": self.sent_metadata.get("turn_id", -1)
                    }
                    if received_metadata != expected_metadata:
                        self._stop_test_with_error(ten_env, f"Metadata mismatch in tts_audio_end. Expected: {expected_metadata}, Received: {received_metadata}")
                        return
                except json.JSONDecodeError:
                    self._stop_test_with_error(ten_env, f"Invalid JSON in tts_audio_end metadata: {metadata_str}")
                    return
            else:
                self._stop_test_with_error(ten_env, f"Missing metadata in tts_audio_end response")
                return
            
            # 校验音频长度
            if self.audio_start_time is not None:
                current_time = time.time()
                actual_duration_ms = (current_time - self.audio_start_time) * 1000
                
                # 获取request_total_audio_duration_ms（音频实际长度）
                received_audio_duration_ms, _ = data.get_property_int("request_total_audio_duration_ms")
                
                # 校验音频长度：request_total_audio_duration_ms 应该与 PCM 文件计算出的长度一致
                pcm_audio_duration_ms = self._calculate_pcm_audio_duration_ms()
                if pcm_audio_duration_ms > 0 and received_audio_duration_ms > 0:
                    audio_duration_diff = abs(received_audio_duration_ms - pcm_audio_duration_ms)
                    if audio_duration_diff > 50:  # 允许50ms误差
                        self._stop_test_with_error(ten_env, f"Audio duration mismatch. PCM calculated: {pcm_audio_duration_ms}ms, Reported: {received_audio_duration_ms}ms, Diff: {audio_duration_diff}ms")
                        return
                    ten_env.log_info(f"✅ Audio duration validation passed. PCM: {pcm_audio_duration_ms}ms, Reported: {received_audio_duration_ms}ms, Diff: {audio_duration_diff}ms")
                else:
                    ten_env.log_info(f"Skipping audio duration validation - PCM: {pcm_audio_duration_ms}ms, Reported: {received_audio_duration_ms}ms")
                
                # 记录实际经过的时间（用于调试）
                ten_env.log_info(f"Actual event duration: {actual_duration_ms:.2f}ms")
            else:
                ten_env.log_warn("tts_audio_start not received before tts_audio_end")
            
            ten_env.log_info(f"✅ tts_audio_end received with correct request_id and metadata")
            
            if not self.receive_metircs:
                self._stop_test_with_error(ten_env, f"no metrics data before tts_audio_end")
            else:
                ten_env.stop_test()

    @override
    async def on_audio_frame(
        self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame
    ) -> None:
        """Handle received audio frame from TTS extension."""
        # Check sample_rate
        sample_rate = audio_frame.get_sample_rate()
        ten_env.log_info(f"Received audio frame with sample_rate: {sample_rate}")

        # Store current test sample_rate
        if self.sample_rate == 0:
            self.sample_rate = sample_rate
            ten_env.log_info(f"First audio frame received with sample_rate: {sample_rate}")

        # Accumulate audio bytes for duration calculation
        try:
            audio_data = audio_frame.get_buf()
            if audio_data:
                self.total_audio_bytes += len(audio_data)
                ten_env.log_info(f"Audio frame size: {len(audio_data)} bytes, Total: {self.total_audio_bytes} bytes")
        except Exception as e:
            ten_env.log_warn(f"Failed to get audio data: {e}")


def test_corner_input(extension_name: str, config_dir: str) -> None:
    """Verify TTS result corner input."""


    # Get config file path
    config_file_path = os.path.join(config_dir, TTS_CORNER_INPUT_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")
    

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)


    # Create and run tester
    tester = ConerTester(
        session_id="test_corner_input_session_123",
        text="hello world, hello agora, hello shanghai, nice to meet you!",
    )

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"
