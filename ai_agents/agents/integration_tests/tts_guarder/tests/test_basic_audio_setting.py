
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

class DumpTester(AsyncExtensionTester):
    """Test class for TTS extension dump"""

    def __init__(
        self,
        session_id: str = "test_dump_session_123",
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
        self.dump_file_name = f"tts_dump_{self.session_id}.pcm"
        self.count_audio_end = 0
        self.request_id: int = request_id
        self.sample_rate: int = 0  # 存储当前测试的 sample_rate
        self.test_name: str = test_name
        self.audio_frame_received: bool = False  # 标记是否已接收到音频帧

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
        tts_text_input_obj.set_property_string("request_id", str(self.request_id))
        tts_text_input_obj.set_property_bool("text_input_end", True)
        metadata = {
            "session_id": "test_dump_session_123",
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
            # 只有在接收到音频帧后才退出测试
            if self.audio_frame_received:
                ten_env.log_info(f"[{self.test_name}] Audio frame received, stopping test")
                ten_env.stop_test()
            else:
                ten_env.log_info(f"[{self.test_name}] Waiting for audio frame before stopping")
            return
    
        
    @override
    async def on_audio_frame(self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame) -> None:
        """Handle received audio frame from TTS extension."""
        # 检查 sample_rate
        sample_rate = audio_frame.get_sample_rate()
        ten_env.log_info(f"[{self.test_name}] Received audio frame with sample_rate: {sample_rate}")
        
        # 标记已接收到音频帧
        self.audio_frame_received = True
        
        # 存储当前测试的 sample_rate
        if self.sample_rate == 0:
            self.sample_rate = sample_rate
            ten_env.log_info(f"✅ [{self.test_name}] First audio frame received with sample_rate: {sample_rate}")
        else:
            # 检查 sample_rate 是否一致
            if self.sample_rate != sample_rate:
                ten_env.log_warn(f"[{self.test_name}] Sample rate changed from {self.sample_rate} to {sample_rate}")
            else:
                ten_env.log_info(f"✅ [{self.test_name}] Sample rate consistent: {sample_rate}")
        
        
    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""

        ten_env.log_info("Test stopped")


def run_single_test(extension_name: str, config_file: str, test_name: str, request_id: int) -> int:
    """运行单个测试并返回 sample_rate"""
    print(f"\n{'='*80}")
    print(f"🚀 开始运行测试: {test_name}")
    print(f"{'='*80}")
    
    # Load config file
    with open(config_file, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Create and run tester
    tester = DumpTester(
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
    
    # 返回测试获得的 sample_rate
    return tester.sample_rate


def test_sample_rate_comparison(extension_name: str, config_dir: str) -> None:
    """比较两个不同配置文件的 sample_rate"""
    print(f"\n{'='*80}")
    print("🧪 TEST: Sample Rate Comparison")
    print(f"{'='*80}")
    print("📋 测试目标: 验证不同配置文件产生不同的 sample_rate")
    print("🎯 预期结果: 两个测试的 sample_rate 应该不同")
    print(f"{'='*80}")
    
    # 测试1: 使用配置文件1
    config_file1 = os.path.join(config_dir, TTS_BASIC_AUDIO_SETTING_CONFIG_FILE1)
    if not os.path.exists(config_file1):
        raise FileNotFoundError(f"Config file not found: {config_file1}")
    
    sample_rate_1 = run_single_test(extension_name, config_file1, "16K_Test", 1)
    
    # 测试2: 使用配置文件2
    config_file2 = os.path.join(config_dir, TTS_BASIC_AUDIO_SETTING_CONFIG_FILE2)
    if not os.path.exists(config_file2):
        raise FileNotFoundError(f"Config file not found: {config_file2}")
    
    sample_rate_2 = run_single_test(extension_name, config_file2, "32K_Test", 2)
    
    # 比较结果
    print(f"\n{'='*80}")
    print("📊 测试结果比较")
    print(f"{'='*80}")
    print(f"测试1 ({TTS_BASIC_AUDIO_SETTING_CONFIG_FILE1}): sample_rate = {sample_rate_1}")
    print(f"测试2 ({TTS_BASIC_AUDIO_SETTING_CONFIG_FILE2}): sample_rate = {sample_rate_2}")
    
    if sample_rate_1 != sample_rate_2:
        print(f"✅ 测试通过: 两个配置文件产生了不同的 sample_rate")
        print(f"   差异: {abs(sample_rate_1 - sample_rate_2)} Hz")
    else:
        print(f"❌ 测试失败: 两个配置文件产生了相同的 sample_rate ({sample_rate_1})")
        raise AssertionError(f"Expected different sample rates, but both are {sample_rate_1}")
    
    print(f"{'='*80}")

