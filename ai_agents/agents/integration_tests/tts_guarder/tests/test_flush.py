
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

TTS_FLUSH_CONFIG_FILE="property_dump.json"


class FlushTester(AsyncExtensionTester):
    """Test class for TTS extension flush"""

    def __init__(
        self,
        session_id: str = "test_flush_session_123",
        text: str = "",
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: TTS Flush Test")
        print("=" * 80)
        print(
            "📋 Test Description: Validate TTS flush"
        )
        print("🎯 Test Objectives:")
        print("   - Verify flush is generated")
        print("=" * 80)

        self.session_id: str = session_id
        self.text: str = text
        self.count_audio_end = 0
        self.flush_send = False
        self.audio_end_received = False

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
        tts_text_input_obj.set_property_string("request_id", "test_flush_request_id_1")
        tts_text_input_obj.set_property_bool("text_input_end", False)
        metadata = {
            "session_id": "test_flush_session_123",
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
        ten_env.log_info(f"Received data: {name}")

        if name == "error":
            code, _ = data.get_property_int("code")
            ten_env.log_info(f"Received error data: {code}")
            self._stop_test_with_error(ten_env, f"Received wrong error code: {code}")
            return
        elif name == "tts_audio_end":
            json_str, _ = data.get_property_to_json("")
            ten_env.log_info(f"Received tts_audio_end data: {json_str}")
            reason, _ = data.get_property_int("reason")
            if reason != 2:
                self._stop_test_with_error(ten_env, f"Received wrong tts_audio_end reason: {reason}")
                return
            else:
                self.audio_end_received = True
        elif name == "tts_flush_end":
            if self.audio_end_received:
                ten_env.stop_test()
                return
            else:
                self._stop_test_with_error(ten_env, f"Received tts_flush_end before tts_audio_end")
                return

                
        
    @override
    async def on_audio_frame(self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame) -> None:
        """Handle received audio frame from TTS extension."""
        if not self.flush_send:
            self.flush_send = True
            ten_env.log_info("Received audio frame, sending flush")
            await self._send_flush(ten_env)
            

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""

        ten_env.log_info("Test stopped")

    async def _send_flush(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info("Sending flush")
        flush_data = Data.create("tts_flush")
        flush_data.set_property_string("flush_id", "test_flush_request_id_1")
        metadata = {
            "session_id": "test_flush_session_123",
        }
        #flush_data.set_property_from_json("metadata", json.dumps(metadata))
        await ten_env.send_data(flush_data)

def test_flush(extension_name: str, config_dir: str) -> None:
    """Verify TTS result flush."""


    # Get config file path
    config_file_path = os.path.join(config_dir, TTS_FLUSH_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Expected test results


    # Log test configuration
    print(f"Using test configuration: {config}")
    config["dump"] = False



    # Create and run tester
    tester = FlushTester(
        session_id="test_flush_session_123",
        text="Mr. and Mrs. Dursley, of number four, Privet Drive, were proud to say that they were perfectly normal, thank you very much. They were the last people you'd expect to be involved in anything strange or mysterious, because they just didn't hold with such nonsense.",
    )

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"
