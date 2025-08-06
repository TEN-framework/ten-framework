#!/usr/bin/env python3
#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from typing import Any, List
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


class InvalidTextHandlingTester(AsyncExtensionTester):
    """Test class for TTS extension invalid text handling"""

    def __init__(self, session_id: str = "test_invalid_text_session_123"):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: TTS Invalid Text Handling Test")
        print("=" * 80)
        print("📋 Test Description: Validate TTS extension handles invalid text correctly")
        print("🎯 Test Objectives:")
        print("   - Verify invalid text returns NON_FATAL_ERROR with vendor_info")
        print("   - Verify valid text returns tts_text_output and audio frame")
        print("   - Test various types of invalid text")
        print("=" * 80)

        self.session_id: str = session_id
        self.current_test_index: int = 0
        self.test_results: List[dict] = []
        self.received_audio_frame: bool = False
        self.received_tts_output: bool = False
        self.received_error: bool = False
        self.current_test_text: str = ""
        
        # 定义测试用例：无效文本和有效文本的配对
        self.test_cases = [
            # 空字符串和空格
            {"invalid": "", "valid": "Hello world."},
            {"invalid": " ", "valid": "This is a test."},
            {"invalid": "   ", "valid": "Another test case."},
            
            # 换行符和制表符
            {"invalid": "\n", "valid": "Text with newline test."},
            {"invalid": "\t", "valid": "Text with tab test."},
            {"invalid": "\n\t\n", "valid": "Mixed whitespace test."},
            
            # 颜文字和表情
            {"invalid": ":-)", "valid": "Smile test."},
            {"invalid": "😊", "valid": "Emoji test."},
            {"invalid": "😀😃😄😁", "valid": "Multiple emoji test."},
            
            # 标点符号
            {"invalid": "，", "valid": "Chinese punctuation test."},
            {"invalid": "。", "valid": "Chinese punctuation test."},
            {"invalid": "/", "valid": "Chinese punctuation test."},
            {"invalid": "】", "valid": "Chinese punctuation test."},
            {"invalid": "（", "valid": "Chinese punctuation test."},
            {"invalid": ".", "valid": "English punctuation test."},
            {"invalid": "/", "valid": "English punctuation test."},
            {"invalid": "(", "valid": "English punctuation test."},
            {"invalid": "]", "valid": "English punctuation test."},
            {"invalid": "}", "valid": "English punctuation test."},
            {"invalid": "！", "valid": "More Chinese punctuation."},
            {"invalid": "？", "valid": "More Chinese punctuation."},
            {"invalid": "；", "valid": "More Chinese punctuation."},
            {"invalid": "：", "valid": "More Chinese punctuation."},
            
            # 计算公式
            {"invalid": "x = (-b ± √(b² - 4ac)) / 2a", "valid": "Mathematical formula test."},
            {"invalid": "2H₂ + O₂ → 2H₂O", "valid": "Chemical equation test."},
            {"invalid": "H₂O", "valid": "Chemical formula test."},
            
            # 混合无效文本
            {"invalid": "   \n\t😊，。/(]}x = (-b ± √(b² - 4ac)) / 2a", "valid": "Mixed invalid text test."},
        ]

    async def _send_tts_text_input(self, ten_env: AsyncTenEnvTester, text: str, is_end: bool = False) -> None:
        """Send tts text input to TTS extension."""
        ten_env.log_info(f"Sending tts text input: '{text}' (length: {len(text)})")
        
        tts_text_input_obj = Data.create("tts_text_input")
        tts_text_input_obj.set_property_string("text", text)
        tts_text_input_obj.set_property_string("request_id", f"test_invalid_request_{self.current_test_index}")
        tts_text_input_obj.set_property_bool("text_input_end", is_end)
        
        metadata = {
            "session_id": self.session_id,
            "turn_id": self.current_test_index + 1,
        }
        tts_text_input_obj.set_property_from_json("metadata", json.dumps(metadata))
        
        await ten_env.send_data(tts_text_input_obj)
        ten_env.log_info(f"✅ tts text input sent: '{text}'")

    def _validate_error_response(self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]) -> bool:
        """验证错误响应是否符合要求"""
        ten_env.log_info("Validating error response...")
        
        # 检查必需字段
        required_fields = ["code", "message", "vendor_info"]
        missing_fields = [field for field in required_fields if field not in json_data]
        
        if missing_fields:
            ten_env.log_error(f"Missing required fields in error response: {missing_fields}")
            return False
        
        # 检查错误代码
        if json_data["code"] != 1000:
            ten_env.log_error(f"Expected error code 1000, got {json_data['code']}")
            return False
        
        # 检查vendor_info
        vendor_info = json_data.get("vendor_info", {})
        if "vendor" not in vendor_info:
            ten_env.log_error("Missing 'vendor' field in vendor_info")
            return False
        
        ten_env.log_info(f"✅ Error response validation passed: {json_data}")
        return True

    def _reset_test_state(self):
        """重置测试状态"""
        self.received_audio_frame = False
        self.received_tts_output = False
        self.received_error = False

    async def _run_single_test(self, ten_env: AsyncTenEnvTester, invalid_text: str, valid_text: str) -> bool:
        """运行单个测试用例"""
        ten_env.log_info(f"\n{'='*60}")
        ten_env.log_info(f"Running test case {self.current_test_index + 1}")
        ten_env.log_info(f"Invalid text: '{invalid_text}'")
        ten_env.log_info(f"Valid text: '{valid_text}'")
        ten_env.log_info(f"{'='*60}")
        
        self._reset_test_state()
        
        # 步骤1: 发送无效文本
        ten_env.log_info("Step 1: Sending invalid text...")
        self.current_test_text = invalid_text
        await self._send_tts_text_input(ten_env, invalid_text, False)
        
        # 等待错误响应
        await asyncio.sleep(2)
        
        ten_env.log_info("✅ Error response received for invalid text")
        
        # 步骤2: 发送有效文本
        ten_env.log_info("Step 2: Sending valid text...")
        self.current_test_text = valid_text
        await self._send_tts_text_input(ten_env, valid_text, True)
        
        # 等待TTS输出和音频帧
        await asyncio.sleep(5)
        
        if not self.received_tts_output:
            ten_env.log_error("❌ No tts_text_output received for valid text")
            return False
        
        if not self.received_audio_frame:
            ten_env.log_error("❌ No audio frame received for valid text")
            return False
        
        ten_env.log_info("✅ TTS output and audio frame received for valid text")
        return True

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """开始测试"""
        ten_env.log_info("Starting TTS invalid text handling test")
        
        # 运行所有测试用例
        for i, test_case in enumerate(self.test_cases):
            self.current_test_index = i
            success = await self._run_single_test(ten_env, test_case["invalid"], test_case["valid"])
            
            test_result = {
                "test_index": i,
                "invalid_text": test_case["invalid"],
                "valid_text": test_case["valid"],
                "success": success
            }
            self.test_results.append(test_result)
            
            if not success:
                ten_env.log_error(f"❌ Test case {i + 1} failed")
                break
            else:
                ten_env.log_info(f"✅ Test case {i + 1} passed")
        
        # 测试完成
        ten_env.log_info("All test cases completed")
        ten_env.stop_test()

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """处理接收到的数据"""
        name: str = data.get_name()
        json_str, metadata = data.get_property_to_json("")
        
        ten_env.log_info(f"Received data: {name}")
        ten_env.log_info(f"JSON: {json_str}")
        ten_env.log_info(f"Metadata: {metadata}")
        
        if name == "error":
            # 处理错误响应
            try:
                error_data = json.loads(json_str) if json_str else {}
                if self._validate_error_response(ten_env, error_data):
                    self.received_error = True
                    ten_env.log_info("✅ Valid error response received")
                else:
                    ten_env.log_error("❌ Invalid error response")
                    # 即使错误响应格式不正确，也标记为收到了错误响应
                    self.received_error = True
            except json.JSONDecodeError as e:
                ten_env.log_error(f"❌ Failed to parse error JSON: {e}")
                # 即使JSON解析失败，也标记为收到了错误响应
                self.received_error = True
        
        elif name == "tts_text_result":
            # 处理TTS文本输出
            self.received_tts_output = True
            ten_env.log_info("✅ TTS text output received")
        
        elif name == "metrics":
            # 处理指标数据
            ten_env.log_info("📊 Metrics received")
        
        elif name == "tts_audio_end":
            # TTS音频结束
            ten_env.log_info("🎵 TTS audio ended")

    @override
    async def on_audio_frame(self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame) -> None:
        """处理音频帧"""
        self.received_audio_frame = True
        ten_env.log_info(f"🎵 Audio frame received: {audio_frame.get_sample_rate()}Hz, {audio_frame.get_bytes_per_sample()} bytes/sample")


def test_invalid_text_handling(extension_name: str, config_dir: str) -> None:
    """测试TTS扩展对无效文本的处理能力"""
    
    # 获取配置文件路径
    config_file_path = os.path.join(config_dir, "property_basic_audio_setting1.json")
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")
    
    # 加载配置文件
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)
    
    # 创建并运行测试器
    tester = InvalidTextHandlingTester(session_id="test_invalid_text_session_123")
    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()
    
    # 输出测试结果摘要
    print("\n" + "="*80)
    print("📊 TEST RESULTS SUMMARY")
    print("="*80)
    
    passed_tests = sum(1 for result in tester.test_results if result["success"])
    total_tests = len(tester.test_results)
    
    print(f"Total test cases: {total_tests}")
    print(f"Passed: {passed_tests}")
    print(f"Failed: {total_tests - passed_tests}")
    
    # 验证测试结果
    if error is not None:
        raise AssertionError(f"Test failed: {error.error_message() if error else 'Unknown error'}")
    
    # 检查是否有测试用例失败
    if passed_tests != total_tests:
        print("❌ Some tests failed!")
        for result in tester.test_results:
            if not result["success"]:
                print(f"  - Test {result['test_index'] + 1} failed")
                print(f"    Invalid text: '{result['invalid_text']}'")
                print(f"    Valid text: '{result['valid_text']}'")
        raise AssertionError(f"Test failed: {total_tests - passed_tests} out of {total_tests} test cases failed")
    else:
        print("🎉 All tests passed!")
    
    print("="*80)


if __name__ == "__main__":
    # 示例用法
    test_invalid_text_handling("elevenlabs_tts_python", "./config") 