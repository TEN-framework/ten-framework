
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
        elif name == "tts_audio_end":
            if not self.receive_metircs:
                self._stop_test_with_error(ten_env, f"no metrics data before tts_audio_end")
            else:
                ten_env.stop_test()


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
