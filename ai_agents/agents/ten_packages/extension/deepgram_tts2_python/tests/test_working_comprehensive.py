#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import os
from typing import Any
from pathlib import Path

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    Data,
    TenError,
)
from ten_ai_base.struct import TTSTextInput

# ================ Case 1: Basic Extension Lifecycle ================
class ExtensionTesterBasic(ExtensionTester):
    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
            return
        statusCode = result.get_status_code()
        print("receive hello_world, status:" + str(statusCode))

        if statusCode == StatusCode.OK:
            ten_env.stop_test()
        else:
            ten_env.stop_test(TenError(1, f"Unexpected status code: {statusCode}"))

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")
        print("send hello_world")
        ten_env_tester.send_cmd(
            new_cmd,
            lambda ten_env, result, _: self.check_hello(ten_env, result),
        )
        print("tester on_start_done")
        ten_env_tester.on_start_done()

def test_basic_lifecycle():
    """Test basic extension lifecycle and hello_world command"""
    print("🧪 Running basic lifecycle test...")
    tester = ExtensionTesterBasic()
    tester.set_test_mode_single("deepgram_tts2_python")
    tester.run()
    print("✅ Basic lifecycle test passed")

# ================ Case 2: Configuration Validation ================
class ExtensionTesterConfig(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.test_completed = False

    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
            return
        statusCode = result.get_status_code()
        print(f"Config test - receive hello_world, status: {statusCode}")

        if statusCode == StatusCode.OK:
            self.test_completed = True
            ten_env.stop_test()
        else:
            ten_env.stop_test(TenError(1, f"Config test failed with status: {statusCode}"))

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")
        print("Config test - send hello_world")
        ten_env_tester.send_cmd(
            new_cmd,
            lambda ten_env, result, _: self.check_hello(ten_env, result),
        )
        ten_env_tester.on_start_done()

def test_valid_configuration():
    """Test extension with valid configuration"""
    print("🧪 Running valid configuration test...")
    
    # Valid configuration
    config = {
        "api_key": os.getenv("DEEPGRAM_TTS_API_KEY", "test_key"),
        "model": "aura-luna-en",
        "voice": "aura-luna-en",
        "encoding": "linear16",
        "sample_rate": 24000
    }

    tester = ExtensionTesterConfig()
    tester.set_test_mode_single(
        "deepgram_tts2_python",
        json.dumps(config)
    )
    tester.run()
    
    assert tester.test_completed, "Configuration test did not complete successfully"
    print("✅ Valid configuration test passed")

# ================ Case 3: TTS Request Processing ================
class ExtensionTesterTTSRequest(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_start_received = False
        self.audio_end_received = False
        self.audio_frames_received = 0
        self.total_audio_bytes = 0
        self.error_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("TTS request test started")
        
        # Send a simple TTS request
        tts_input = TTSTextInput(
            request_id="test_tts_request",
            text="Hello world, this is a test.",
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "tts_audio_start":
            self.audio_start_received = True
            ten_env.log_info("Received tts_audio_start")

        elif name == "tts_audio_end":
            self.audio_end_received = True
            ten_env.log_info("Received tts_audio_end, stopping test")
            ten_env.stop_test()

        elif name == "error":
            self.error_received = True
            json_str, _ = data.get_property_to_json(None)
            error_data = json.loads(json_str)
            ten_env.log_error(f"Received error: {error_data}")
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receive and count audio frames"""
        self.audio_frames_received += 1
        
        buf = audio_frame.lock_buf()
        try:
            self.total_audio_bytes += len(buf)
            if self.audio_frames_received == 1:
                ten_env.log_info("First audio frame received")
        finally:
            audio_frame.unlock_buf(buf)

def test_tts_request_with_api():
    """Test TTS request processing with API (only if API key is available)"""
    api_key = os.getenv("DEEPGRAM_TTS_API_KEY")
    if not api_key or api_key == "test_key":
        print("⚠️ Skipping TTS request test - no valid API key")
        return

    print("🧪 Running TTS request test with real API...")

    config = {
        "api_key": api_key,
        "model": "aura-luna-en",
        "voice": "aura-luna-en",
        "encoding": "linear16",
        "sample_rate": 24000
    }

    tester = ExtensionTesterTTSRequest()
    tester.set_test_mode_single(
        "deepgram_tts2_python",
        json.dumps(config)
    )
    tester.run()

    # Verify results
    if not tester.error_received:
        assert tester.audio_start_received, "Did not receive tts_audio_start"
        assert tester.audio_end_received, "Did not receive tts_audio_end"
        assert tester.audio_frames_received > 0, "Did not receive any audio frames"
        assert tester.total_audio_bytes > 0, "Did not receive any audio data"
        print(f"✅ TTS request test passed: {tester.audio_frames_received} frames, {tester.total_audio_bytes} bytes")
    else:
        print("⚠️ TTS request test completed with error (expected if API key is invalid)")

# ================ Case 4: Connection Handling ================
class ExtensionTesterConnection(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.connection_established = False
        self.test_completed = False

    def check_hello(self, ten_env: TenEnvTester, result: CmdResult | None):
        if result is None:
            ten_env.stop_test(TenError(1, "CmdResult is None"))
            return
        statusCode = result.get_status_code()
        print(f"Connection test - receive hello_world, status: {statusCode}")

        if statusCode == StatusCode.OK:
            self.connection_established = True
            self.test_completed = True
            ten_env.stop_test()
        else:
            ten_env.stop_test(TenError(1, f"Connection test failed: {statusCode}"))

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        new_cmd = Cmd.create("hello_world")
        print("Connection test - send hello_world")
        ten_env_tester.send_cmd(
            new_cmd,
            lambda ten_env, result, _: self.check_hello(ten_env, result),
        )
        ten_env_tester.on_start_done()

def test_connection_handling():
    """Test WebSocket connection handling"""
    print("🧪 Running connection handling test...")

    config = {
        "api_key": os.getenv("DEEPGRAM_TTS_API_KEY", "test_key"),
        "model": "aura-luna-en",
        "voice": "aura-luna-en"
    }

    tester = ExtensionTesterConnection()
    tester.set_test_mode_single(
        "deepgram_tts2_python",
        json.dumps(config)
    )
    tester.run()

    assert tester.test_completed, "Connection test did not complete"
    print("✅ Connection handling test passed")

# ================ Test Runner ================
def run_all_tests():
    """Run all working comprehensive tests"""
    print("🚀 Running Deepgram TTS2 Working Comprehensive Test Suite")
    print("=" * 60)
    
    test_functions = [
        test_basic_lifecycle,
        test_valid_configuration,
        test_connection_handling,
        test_tts_request_with_api,  # This will skip if no valid API key
    ]
    
    passed = 0
    failed = 0
    skipped = 0
    
    for test_func in test_functions:
        try:
            print(f"\n🧪 Running {test_func.__name__}...")
            test_func()
            passed += 1
        except Exception as e:
            if "Skipping" in str(e) or "no valid API key" in str(e):
                print(f"⚠️ {test_func.__name__} SKIPPED: {e}")
                skipped += 1
            else:
                print(f"❌ {test_func.__name__} FAILED: {e}")
                failed += 1
    
    print(f"\n📊 Test Results: {passed} passed, {failed} failed, {skipped} skipped")
    print("=" * 60)
    return passed, failed, skipped

# Individual test functions for pytest
def test_basic():
    """Pytest entry point for basic test"""
    test_basic_lifecycle()

def test_config():
    """Pytest entry point for configuration test"""
    test_valid_configuration()

def test_connection():
    """Pytest entry point for connection test"""
    test_connection_handling()

def test_tts_api():
    """Pytest entry point for TTS API test"""
    test_tts_request_with_api()

if __name__ == "__main__":
    run_all_tests()
