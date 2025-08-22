
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

TTS_BASIC_AUDIO_SETTING_CONFIG_FILE1="property_basic_audio_setting1.json"
TTS_BASIC_AUDIO_SETTING_CONFIG_FILE2="property_basic_audio_setting2.json"
CASE1_SAMPLE_RATE=0
CASE2_SAMPLE_RATE=0

class BasicAudioSettingTester(AsyncExtensionTester):
    """Test class for TTS extension basic audio setting"""

    def __init__(
        self,
        session_id: str = "test_basic_audio_setting_session_123",
        text: str = "",
        request_id: int = 1,
        test_name: str = "default",
    ):
        super().__init__()
        print("=" * 80)
        print(f"🧪 TEST CASE: {test_name}")
        print("=" * 80)
        print(
            "📋 Test Description: Validate TTS sample rate settings"
        )
        print("🎯 Test Objectives:")
        print("   - Verify different sample rates for different configs")
        print("=" * 80)

        self.session_id: str = session_id
        self.text: str = text
        self.dump_file_name = f"tts_basic_audio_setting_{self.session_id}.pcm"
        self.count_audio_end = 0
        self.request_id: int = request_id
        self.sample_rate: int = 0  # Store current test sample_rate
        self.test_name: str = test_name
        self.audio_frame_received: bool = False  # Flag whether audio frame has been received

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
        """Start the TTS Basic Audio Setting test."""
        ten_env.log_info("Starting TTS Basic Audio Setting test")
        await self._send_tts_text_input(ten_env, self.text)

    async def _send_tts_text_input(self, ten_env: AsyncTenEnvTester, text: str, request_num: int = 1) -> None:
        """Send tts text input to TTS extension."""
        ten_env.log_info(f"Sending tts text input: {text}")
        tts_text_input_obj = Data.create("tts_text_input")
        tts_text_input_obj.set_property_string("text", text)
        tts_text_input_obj.set_property_string("request_id", str(self.request_id))
        tts_text_input_obj.set_property_bool("text_input_end", True)
        metadata = {
            "session_id": self.session_id,
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
        ten_env.log_info(f"[{self.test_name}] Received data: {name}")

        if name == "error":
            json_str, _ = data.get_property_to_json("")
            ten_env.log_info(f"[{self.test_name}] Received error data: {json_str}")

            self._stop_test_with_error(ten_env, f"Received error data")
            return
        elif name == "tts_audio_end":
            ten_env.log_info(f"[{self.test_name}] Received tts_audio_end")
            # Only exit test after receiving audio frame
            if self.audio_frame_received:
                ten_env.log_info(f"[{self.test_name}] Audio frame received, stopping test")
                ten_env.stop_test()
            else:
                ten_env.log_info(f"[{self.test_name}] Waiting for audio frame before stopping")
            return
    
        
    @override
    async def on_audio_frame(self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame) -> None:
        """Handle received audio frame from TTS extension."""
                    # Check sample_rate
        sample_rate = audio_frame.get_sample_rate()
        ten_env.log_info(f"[{self.test_name}] Received audio frame with sample_rate: {sample_rate}")
        
                    # Mark that audio frame has been received
        self.audio_frame_received = True
        
        # Store current test sample_rate
        if self.sample_rate == 0:
            self.sample_rate = sample_rate
            ten_env.log_info(f"✅ [{self.test_name}] First audio frame received with sample_rate: {sample_rate}")
        else:
            # Check if sample_rate is consistent
            if self.sample_rate != sample_rate:
                ten_env.log_warn(f"[{self.test_name}] Sample rate changed from {self.sample_rate} to {sample_rate}")
            else:
                ten_env.log_info(f"✅ [{self.test_name}] Sample rate consistent: {sample_rate}")
        
        
    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""

        ten_env.log_info("Test stopped")


def run_single_test(extension_name: str, config_file: str, test_name: str, request_id: int) -> int:
    """Run single test and return sample_rate"""
    print(f"\n{'='*80}")
    print(f"🚀 Starting test: {test_name}")
    print(f"{'='*80}")
    
    # Load config file
    with open(config_file, "r") as f:
        config: dict[str, Any] = json.load(f)

    print(f"config: {json.dumps(config, indent=4)}")

    # Create and run tester
    tester = BasicAudioSettingTester(
        session_id=f"test_session_{test_name}",
        text="hello world, hello agora, hello shanghai, nice to meet you!",
        request_id=request_id,
        test_name=test_name
    )
    
    # Set the tts_extension_dump_folder for the tester
    tester.tts_extension_dump_folder = config["dump_path"]

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"
    
    # Return the sample_rate obtained from the test
    return tester.sample_rate


def test_sample_rate_comparison(extension_name: str, config_dir: str, enable_sample_rate: bool = True) -> None:
    """Compare sample_rate between two different config files"""
    print(f"\n{'='*80}")
    print("🧪 TEST: Sample Rate Comparison")
    print(f"{'='*80}")
    if enable_sample_rate:
        print("📋 Test objective: Verify that different config files produce different sample_rate")
        print("🎯 Expected result: Two tests should have different sample_rate")
    else:
        print("📋 Test objective: Verify that TTS extension works with different config files")
        print("🎯 Expected result: Both tests should complete successfully (sample rate comparison disabled)")
    print(f"{'='*80}")
    
    # Test 1: Use config file 1
    config_file1 = os.path.join(config_dir, TTS_BASIC_AUDIO_SETTING_CONFIG_FILE1)
    if not os.path.exists(config_file1):
        raise FileNotFoundError(f"Config file not found: {config_file1}")
    
    sample_rate_1 = run_single_test(extension_name, config_file1, "16K_Test", 1)
    
    # Test 2: Use config file 2
    config_file2 = os.path.join(config_dir, TTS_BASIC_AUDIO_SETTING_CONFIG_FILE2)
    if not os.path.exists(config_file2):
        raise FileNotFoundError(f"Config file not found: {config_file2}")
    
    sample_rate_2 = run_single_test(extension_name, config_file2, "32K_Test", 2)
    
    # Compare results
    print(f"\n{'='*80}")
    print("📊 Test result comparison")
    print(f"{'='*80}")
    print(f"Test 1 ({TTS_BASIC_AUDIO_SETTING_CONFIG_FILE1}): sample_rate = {sample_rate_1}")
    print(f"Test 2 ({TTS_BASIC_AUDIO_SETTING_CONFIG_FILE2}): sample_rate = {sample_rate_2}")
    
    if enable_sample_rate:
        # Compare sample rates when enabled
        if sample_rate_1 != sample_rate_2:
            print(f"✅ Test passed: Two config files produced different sample_rate")
            print(f"   Difference: {abs(sample_rate_1 - sample_rate_2)} Hz")
        else:
            print(f"❌ Test failed: Two config files produced the same sample_rate ({sample_rate_1})")
            raise AssertionError(f"Expected different sample rates, but both are {sample_rate_1}")
    else:
        # Skip sample rate comparison when disabled
        print(f"✅ Test passed: Both tests completed successfully (sample rate comparison disabled)")
        print(f"   Sample rates: {sample_rate_1} Hz and {sample_rate_2} Hz (not compared)")
    
    print(f"{'='*80}")

