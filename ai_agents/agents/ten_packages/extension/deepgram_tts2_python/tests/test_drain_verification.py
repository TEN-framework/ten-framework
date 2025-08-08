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


class DrainTestExtension(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.drain_received = False
        self.drain_cmd_name = None

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Drain test started")
        ten_env_tester.on_start_done()

    def on_cmd(self, ten_env: TenEnvTester, cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"Received command: {cmd_name}")
        
        if cmd_name == "drain":
            self.drain_received = True
            self.drain_cmd_name = cmd_name
            ten_env.log_info("✅ DRAIN COMMAND RECEIVED!")
            
        # Return success for all commands
        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        ten_env.return_result(cmd_result)
        
        # Stop test after receiving drain
        if cmd_name == "drain":
            ten_env.stop_test()


def test_drain_command():
    """Test that drain command is sent after TTS completion"""
    tester = DrainTestExtension()
    
    # Mock configuration with valid API key to avoid initialization errors
    mock_config = {
        "api_key": "mock_api_key_for_testing",
        "model": "aura-asteria-en",
        "voice": "aura-asteria-en",
        "sample_rate": 24000,
        "encoding": "linear16",
        "container": "none"
    }
    
    # Create a mock TTS request that would trigger drain
    mock_tts_data = {
        "request_id": "test_drain_request",
        "text": "Hello world",
        "metadata": {}
    }
    
    # Run the test
    tester.run_test(
        extension_addon="deepgram_tts2_python",
        property=json.dumps(mock_config),
        test_data_value=json.dumps(mock_tts_data)
    )
    
    # Verify drain command was received
    assert tester.drain_received, "Drain command was not received"
    assert tester.drain_cmd_name == "drain", f"Expected 'drain', got '{tester.drain_cmd_name}'"
    
    print("✅ Drain command test PASSED!")


if __name__ == "__main__":
    test_drain_command()
