#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import time
from typing import Any

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Cmd,
    CmdResult,
    StatusCode,
    Data,
    TenError,
)

class ExtensionTesterDebugError(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_code = None
        self.error_message = None
        self.data_received = []
        self.timeout_seconds = 10

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Debug error test started - waiting for error data")
        
        # Start a timeout timer
        import threading
        def timeout_handler():
            time.sleep(self.timeout_seconds)
            if not self.error_received:
                ten_env_tester.log_error(f"Timeout after {self.timeout_seconds}s - no error received")
                ten_env_tester.stop_test(TenError(1, "Timeout waiting for error"))
        
        timeout_thread = threading.Thread(target=timeout_handler)
        timeout_thread.daemon = True
        timeout_thread.start()
        
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"🔍 DEBUG: Received data with name: '{name}'")
        self.data_received.append(name)

        if name == "error":
            self.error_received = True
            try:
                json_str, _ = data.get_property_to_json(None)
                error_data = json.loads(json_str)

                self.error_code = error_data.get("code")
                self.error_message = error_data.get("message", "")

                ten_env.log_info(f"✅ SUCCESS: Received error data - code={self.error_code}, message='{self.error_message}'")
                ten_env.stop_test()
            except Exception as e:
                ten_env.log_error(f"❌ ERROR: Failed to parse error data: {str(e)}")
                ten_env.stop_test(TenError(1, f"Failed to parse error data: {str(e)}"))
        else:
            ten_env.log_info(f"📝 INFO: Received non-error data: {name}")

def test_debug_empty_api_key():
    """Debug test for empty API key error handling"""
    print("🔍 Starting debug test for empty API key error...")

    # Empty API key configuration
    empty_config = {
        "api_key": "",  # Empty API key should trigger error
        "model": "aura-luna-en",
        "voice": "aura-luna-en"
    }

    tester = ExtensionTesterDebugError()
    tester.set_test_mode_single(
        "deepgram_tts2_python",
        json.dumps(empty_config)
    )

    print("🚀 Running debug test...")
    try:
        tester.run()
        print("✅ Debug test completed successfully")
        
        # Print debug information
        print(f"📊 Debug Results:")
        print(f"   - Error received: {tester.error_received}")
        print(f"   - Error code: {tester.error_code}")
        print(f"   - Error message: '{tester.error_message}'")
        print(f"   - All data received: {tester.data_received}")
        
        # Verify results
        if tester.error_received:
            print("✅ SUCCESS: Error was properly received!")
            assert tester.error_code == -1000, f"Expected error code -1000, got {tester.error_code}"
            assert "api key" in tester.error_message.lower(), f"Expected API key error, got: {tester.error_message}"
        else:
            print("❌ FAILURE: No error was received")
            raise AssertionError("Expected to receive error data")
            
    except Exception as e:
        print(f"❌ Debug test failed: {str(e)}")
        print(f"📊 Debug Results at failure:")
        print(f"   - Error received: {tester.error_received}")
        print(f"   - All data received: {tester.data_received}")
        raise

if __name__ == "__main__":
    test_debug_empty_api_key()
