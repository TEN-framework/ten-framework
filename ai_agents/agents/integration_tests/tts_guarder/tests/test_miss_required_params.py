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

TTS_MISS_REQUIRED_PARAMS_CONFIG_FILE = "property_miss_required.json"


class MissRequiredParamsTester(AsyncExtensionTester):
    """Test class for TTS extension miss required params"""

    def __init__(
        self,
        session_id: str = "test_miss_required_params_session_123",
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: Miss Required Params TTS Test")
        print("=" * 80)
        print("📋 Test Description: Validate TTS result miss required params")
        print("🎯 Test Objectives:")
        print("   - Verify required params are not missing")
        print("   - Test will receive error message with FATAL ERROR")
        print("=" * 80)

        self.session_id: str = session_id

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
        finalize_data_obj.set_property_from_json(None, json.dumps(finalize_data))

        # Send the finalize signal
        await ten_env.send_data(finalize_data_obj)

        ten_env.log_info(
            f"✅ tts_finalize signal sent with ID: {finalize_data['finalize_id']}"
        )

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Start the TTS miss required params test."""
        ten_env.log_info("Starting TTS miss required params test")

    def _stop_test_with_error(
        self, ten_env: AsyncTenEnvTester, error_message: str
    ) -> None:
        """Stop test with error message."""
        ten_env.stop_test(TenError.create(TenErrorCode.ErrorCodeGeneric, error_message))

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
        missing_fields = [field for field in required_fields if field not in json_data]

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

        if name == "error":
            json_str, _ = data.get_property_to_json("")
            ten_env.log_info(f"Received error data: {json_str}")
            code, _ = data.get_property_int("code")
            if code == -1000:
                ten_env.log_info(
                    "✅ TTS miss required params test passed with final result"
                )
                ten_env.stop_test()
            else:
                self._stop_test_with_error(
                    ten_env, f"Received wrong error code: {code}"
                )
            return

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up resources when test stops."""
        ten_env.log_info("Test stopped")


def test_miss_required_params(extension_name: str, config_dir: str) -> None:
    """Verify TTS result miss required params."""

    # Get config file path
    config_file_path = os.path.join(config_dir, TTS_MISS_REQUIRED_PARAMS_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Expected test results

    # Log test configuration
    print(f"Using test configuration: {config}")
    # Get config file path
    config_file_path = os.path.join(config_dir, TTS_MISS_REQUIRED_PARAMS_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Expected test results

    # Log test configuration
    print(f"Using test configuration: {config}")

    # Create and run tester
    tester = MissRequiredParamsTester(
        session_id="test_miss_required_params_session_123",
    )

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    # Verify test results
    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"
