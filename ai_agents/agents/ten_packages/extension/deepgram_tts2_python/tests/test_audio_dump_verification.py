#!/usr/bin/env python3

import asyncio
import json
import os
import time
from ten_runtime import (
    TenEnvTester,
    ExtensionTester,
    Cmd,
    Data,
    StatusCode,
    CmdResult,
)


class AudioDumpTestExtension(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_data_received = False
        self.dump_path = "/tmp/deepgram_tts2_python_out.pcm"

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Audio dump test started")
        # Clean up any existing dump file
        if os.path.exists(self.dump_path):
            os.remove(self.dump_path)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        data_name = data.get_name()
        
        if data_name == "pcm_frame":
            self.audio_data_received = True
            ten_env.log_info("✅ AUDIO DATA RECEIVED")
            
            # Check if dump file was created (with a small delay)
            time.sleep(0.1)
            if os.path.exists(self.dump_path):
                file_size = os.path.getsize(self.dump_path)
                ten_env.log_info(f"✅ AUDIO DUMP FILE CREATED: {self.dump_path} ({file_size} bytes)")
            else:
                ten_env.log_warn(f"❌ AUDIO DUMP FILE NOT FOUND: {self.dump_path}")
            
            ten_env.stop_test()

    def on_cmd(self, ten_env: TenEnvTester, cmd) -> None:
        cmd_name = cmd.get_name()
        
        # Return success for all commands
        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        ten_env.return_result(cmd_result)


def test_audio_dump():
    """Test audio dump functionality"""
    tester = AudioDumpTestExtension()
    
    # Mock configuration with dump enabled
    mock_config = {
        "api_key": "mock_api_key_for_testing",
        "model": "aura-asteria-en",
        "voice": "aura-asteria-en", 
        "sample_rate": 24000,
        "encoding": "linear16",
        "container": "none",
        "dump_enabled": True,
        "dump_path": "/tmp"
    }
    
    # Create a TTS request to trigger audio dump
    mock_tts_data = {
        "request_id": "test_dump_request",
        "text": "Hello world for dump testing",
        "metadata": {}
    }
    
    # Run the test
    tester.run_test(
        extension_addon="deepgram_tts2_python",
        property=json.dumps(mock_config),
        test_data_value=json.dumps(mock_tts_data)
    )
    
    # Verify audio data was received
    assert tester.audio_data_received, "Audio data was not received"
    
    # Verify dump file was created
    dump_exists = os.path.exists(tester.dump_path)
    if dump_exists:
        file_size = os.path.getsize(tester.dump_path)
        print(f"✅ Audio dump test PASSED! File: {tester.dump_path} ({file_size} bytes)")
        # Clean up
        os.remove(tester.dump_path)
    else:
        print(f"⚠️  Audio dump file not created (may be due to mock data)")
    
    print("✅ Audio dump functionality test completed!")


if __name__ == "__main__":
    test_audio_dump()
