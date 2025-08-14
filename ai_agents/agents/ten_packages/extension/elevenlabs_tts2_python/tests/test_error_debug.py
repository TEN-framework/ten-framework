import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

import json
from unittest.mock import patch, AsyncMock
import asyncio
from asyncio import QueueEmpty

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.message import ModuleVendorException, ModuleErrorVendorInfo


class ExtensionTesterErrorDebug(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_data = None
        self.test_name = "ERROR_DEBUG_TEST"

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env_tester.log_info(
            f"{self.test_name} started, sending TTS request."
        )

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello word, hello agora",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"{self.test_name} received data: {name}")
        if name == "error":
            ten_env.log_info(f"{self.test_name} received error data")
            self.error_received = True
            payload, _ = data.get_property_to_json("")
            self.error_data = json.loads(payload)
            ten_env.stop_test()


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2")
def test_error_debug(MockElevenLabsTTS2):
    """Debug test to identify the error handling issue."""
    print("Starting error debug test...")

    # Mock the ElevenLabsTTS2 class to raise an exception
    mock_client_instance = AsyncMock()

    # Mock the start_connection method to raise an exception
    error_info = ModuleErrorVendorInfo(
        vendor="elevenlabs", code="TEST_ERROR", message="Test error message"
    )
    mock_client_instance.start_connection.side_effect = ModuleVendorException(
        error_info
    )
    MockElevenLabsTTS2.return_value = mock_client_instance

    print("Creating tester...")
    # Create and run the tester
    tester = ExtensionTesterErrorDebug()
    tester.set_test_mode_single("elevenlabs_tts2_python")

    print("Running tester...")
    tester.run()

    print("Test completed")
    # Verify that error was received
    assert tester.error_received, "Error was not received"
    assert tester.error_data is not None, "Error data was not received"
    print("Error debug test passed!")
