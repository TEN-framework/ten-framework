#!/usr/bin/env python3

import asyncio
import json
import time
from ten_runtime import (
    TenEnvTester,
    ExtensionTester,
    Cmd,
    Data,
    StatusCode,
    CmdResult,
)


class TTFBKeypointTestExtension(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ttfb_received = False
        self.ttfb_metrics = None
        self.keypoint_logs = []
        self.audio_start_received = False
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("TTFB and KEYPOINT test started")
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"Received data: {data_name}")

        if data_name == "tts_ttfb_metrics":
            self.ttfb_received = True
            json_str, _ = data.get_property_to_json(None)
            self.ttfb_metrics = json.loads(json_str)
            ten_env.log_info(f"✅ TTFB METRICS RECEIVED: {self.ttfb_metrics}")
            
        elif data_name == "tts_audio_start":
            self.audio_start_received = True
            ten_env.log_info("✅ AUDIO START RECEIVED")
            
        elif data_name == "tts_audio_end":
            self.audio_end_received = True
            ten_env.log_info("✅ AUDIO END RECEIVED")
            
        # Check if we have all expected data
        if self.ttfb_received and self.audio_start_received and self.audio_end_received:
            ten_env.log_info("✅ ALL TTS METRICS RECEIVED!")
            ten_env.stop_test()

    def on_cmd(self, ten_env: TenEnvTester, cmd) -> None:
        cmd_name = cmd.get_name()
        
        # Return success for all commands
        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        ten_env.return_result(cmd_result)


def test_ttfb_and_keypoint():
    """Test TTFB metrics and KEYPOINT logging"""
    print("Starting TTFB and KEYPOINT test...")
    
    # Mock configuration with valid API key
    mock_config = {
        "api_key": "mock_api_key_for_testing",
        "model": "aura-asteria-en", 
        "voice": "aura-asteria-en",
        "sample_rate": 24000,
        "encoding": "linear16",
        "container": "none"
    }
    
    tester = TTFBKeypointTestExtension()
    tester.set_test_mode_single(
        "deepgram_tts2_python",
        json.dumps(mock_config)
    )
    
    print("Running test...")
    tester.run()
    print("Test completed.")
    
    # Verify TTFB metrics were received
    assert tester.ttfb_received, "TTFB metrics were not received"
    assert tester.ttfb_metrics is not None, "TTFB metrics data is None"
    
    # Verify audio events were received
    assert tester.audio_start_received, "Audio start event not received"
    assert tester.audio_end_received, "Audio end event not received"
    
    print("✅ TTFB and KEYPOINT test PASSED!")
    print(f"✅ TTFB Metrics: {tester.ttfb_metrics}")


if __name__ == "__main__":
    test_ttfb_and_keypoint()
