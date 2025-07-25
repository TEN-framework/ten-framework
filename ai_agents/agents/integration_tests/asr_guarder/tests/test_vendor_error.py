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


# Constants for audio configuration
AUDIO_CHUNK_SIZE = 320
AUDIO_SAMPLE_RATE = 16000
AUDIO_BYTES_PER_SAMPLE = 2
FRAME_INTERVAL_MS = 10

# Constants for test configuration
DEFAULT_SESSION_ID = "test_session_123"
DEFAULT_CONFIG_FILE = "invalid.json"

# Error validation constants
REQUIRED_ERROR_FIELDS = ["id", "module", "code", "message"]
VENDOR_INFO_REQUIRED_FIELDS = ["vendor", "code", "message"]


class VendorErrorTester(AsyncExtensionTester):
    """Test class for ASR vendor error detection."""

    def __init__(
        self, audio_file_path: str, session_id: str = DEFAULT_SESSION_ID
    ):
        super().__init__()
        self.audio_file_path: str = audio_file_path
        self.session_id: str = session_id
        self.sender_task: asyncio.Task[None] | None = None
        self.error_received: bool = False
        self.error_data = {}
        self.vendor_info_received: bool = False

    def _create_audio_frame(self, data: bytes, session_id: str) -> AudioFrame:
        """Create an audio frame with the given data and session ID."""
        audio_frame = AudioFrame.create("pcm_frame")

        # Set session_id in metadata according to API specification
        metadata = {"session_id": session_id}
        audio_frame.set_property_from_json("metadata", json.dumps(metadata))

        audio_frame.alloc_buf(len(data))
        buf = audio_frame.lock_buf()
        buf[:] = data
        audio_frame.unlock_buf(buf)
        return audio_frame

    async def _send_audio_file(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio file data to ASR extension to trigger vendor errors."""
        ten_env.log_info(
            f"Sending audio file to trigger vendor errors: {self.audio_file_path}"
        )

        with open(self.audio_file_path, "rb") as audio_file:
            while True:
                chunk = audio_file.read(AUDIO_CHUNK_SIZE)
                if not chunk:
                    break

                audio_frame = self._create_audio_frame(chunk, self.session_id)
                await ten_env.send_audio_frame(audio_frame)
                await asyncio.sleep(FRAME_INTERVAL_MS / 1000)

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        """Send audio data to trigger vendor error conditions."""
        try:
            await self._send_audio_file(ten_env)
            # Send additional silence to ensure error conditions are triggered
            await asyncio.sleep(2.0)
        except Exception as e:
            ten_env.log_error(f"Error in audio sender: {e}")

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Initialize the test and start audio sending task."""
        ten_env.log_info("Starting vendor error detection test...")
        self.sender_task = asyncio.create_task(self.audio_sender(ten_env))

    def _stop_test_with_error(
        self, ten_env: AsyncTenEnvTester, error_message: str, details: str = ""
    ) -> None:
        """Stop the test with an error."""
        full_message = f"{error_message}"
        if details:
            full_message += f": {details}"

        err = TenError.create(
            error_code=TenErrorCode.ErrorCodeGeneric,
            error_message=full_message,
        )
        ten_env.stop_test(err)

    def _validate_error_format(
        self, ten_env: AsyncTenEnvTester, json_data
    ) -> tuple[bool, str]:
        """Validate error data format according to ASR protocol specification."""
        ten_env.log_info(f"Validating error format: {json_data}")

        # 1. Validate required fields (id, module, code, message are required)
        missing_fields = [
            field for field in REQUIRED_ERROR_FIELDS if field not in json_data
        ]
        if missing_fields:
            error_details = f"Missing required fields: {missing_fields}, got: {list(json_data.keys())}"
            ten_env.log_error(error_details)
            return False, error_details

        # 2. Validate field types
        if not isinstance(json_data.get("id"), str):
            error_details = (
                f"Field 'id' must be string, got {type(json_data.get('id'))}"
            )
            ten_env.log_error(error_details)
            return False, error_details

        if not isinstance(json_data.get("module"), str):
            error_details = f"Field 'module' must be string, got {type(json_data.get('module'))}"
            ten_env.log_error(error_details)
            return False, error_details

        if not isinstance(json_data.get("code"), int):
            error_details = (
                f"Field 'code' must be int, got {type(json_data.get('code'))}"
            )
            ten_env.log_error(error_details)
            return False, error_details

        if not isinstance(json_data.get("message"), str):
            error_details = f"Field 'message' must be string, got {type(json_data.get('message'))}"
            ten_env.log_error(error_details)
            return False, error_details

        # 4. Validate vendor_info structure if present
        vendor_info: dict[str, str] | None = json_data.get("vendor_info")
        if vendor_info is not None:
            if not isinstance(vendor_info, dict):
                error_details = "Field 'vendor_info' must be object type"
                ten_env.log_error(error_details)
                return False, error_details

            # Check required vendor_info fields
            vendor_missing_fields = [
                field
                for field in VENDOR_INFO_REQUIRED_FIELDS
                if field not in vendor_info
            ]
            if vendor_missing_fields:
                error_details = f"Missing vendor_info fields: {vendor_missing_fields}, got: {list(vendor_info.keys())}"
                ten_env.log_error(error_details)
                return False, error_details

            # Validate vendor_info field types
            if not isinstance(vendor_info.get("vendor"), str):
                error_details = f"vendor_info.vendor must be string, got {type(vendor_info.get('vendor'))}"
                ten_env.log_error(error_details)
                return False, error_details

            if not isinstance(vendor_info.get("code"), str):
                error_details = f"vendor_info.code must be string, got {type(vendor_info.get('code'))}"
                ten_env.log_error(error_details)
                return False, error_details

            if not isinstance(vendor_info.get("message"), str):
                error_details = f"vendor_info.message must be string, got {type(vendor_info.get('message'))}"
                ten_env.log_error(error_details)
                return False, error_details

            self.vendor_info_received = True
            ten_env.log_info(f"✅ Vendor info validated: {vendor_info}")

        # 5. Validate metadata structure if present
        metadata: dict[str, str] | None = json_data.get("metadata")
        if metadata is not None:
            if not isinstance(metadata, dict):
                error_details = "Field 'metadata' must be object type"
                ten_env.log_error(error_details)
                return False, error_details

            if "session_id" in metadata and not isinstance(
                metadata.get("session_id"), str
            ):
                error_details = (
                    "Field 'metadata.session_id' must be string type"
                )
                ten_env.log_error(error_details)
                return False, error_details

        ten_env.log_info("✅ Error format validation passed")
        return True, ""

    def _validate_error_code_types(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that error code must be exactly 1000."""
        error_code: int | None = json_data.get("code")
        if error_code is None:
            ten_env.log_error("Error code is missing")
            return False

        # Validate that error code is a valid integer
        if not isinstance(error_code, int):
            ten_env.log_error(
                f"Error code must be integer, got: {type(error_code)}"
            )
            return False

        # Validate that error code must be exactly NON_FATAL_ERROR
        if error_code != 1000:
            ten_env.log_error(
                f"Error code must be NON_FATAL_ERROR, got: {error_code}"
            )
            return False

        ten_env.log_info(
            f"✅ Error code {error_code} validated (must be NON_FATAL_ERROR)"
        )
        return True

    def _validate_session_id_consistency(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        """Validate that session_id is correctly passed through the error."""
        metadata = json_data.get("metadata", {})
        session_id: str | None = metadata.get("session_id")

        if session_id is None:
            ten_env.log_info("No session_id found in error metadata")
            return True  # Not a critical failure

        if session_id != self.session_id:
            ten_env.log_error(
                f"Session ID mismatch: expected {self.session_id}, got {session_id}"
            )
            return False

        ten_env.log_info(f"✅ Session ID consistency validated: {session_id}")
        return True

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        """Handle incoming data and validate error responses."""
        data_name = data.get_name()

        if data_name == "error":
            self.error_received = True
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env.log_info(
                f"🔍 Received error data: {json.dumps(data_dict, indent=2)}"
            )

            # Store error data for final validation
            self.error_data = data_dict

            # Validate error format
            is_valid, error_details = self._validate_error_format(
                ten_env, data_dict
            )
            if not is_valid:
                self._stop_test_with_error(
                    ten_env, "Error format validation failed", error_details
                )
                return

            # Validate error code types
            if not self._validate_error_code_types(ten_env, data_dict):
                self._stop_test_with_error(
                    ten_env, "Error code validation failed"
                )
                return

            # Validate session ID consistency
            if not self._validate_session_id_consistency(ten_env, data_dict):
                self._stop_test_with_error(
                    ten_env, "Session ID validation failed"
                )
                return

            ten_env.log_info("✅ All error validations passed")

            # Stop test after successful error validation
            ten_env.stop_test()

        elif data_name == "azure_connection_event":
            # Log connection events for debugging
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            ten_env.log_info(f"Connection event: {data_dict}")

        else:
            # Log other data types for debugging
            ten_env.log_info(f"Received data: {data_name}")

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        """Clean up and provide test summary."""
        if self.sender_task and not self.sender_task.done():
            self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass

        # Final validation summary
        if not self.error_received:
            ten_env.log_error("❌ No error data received during test")
            self._stop_test_with_error(ten_env, "No vendor error detected")
            return

        ten_env.log_info(
            "✅ Vendor error detection test completed successfully"
        )
        ten_env.log_info(f"📊 Test Summary:")
        ten_env.log_info(f"   - Error received: {self.error_received}")
        ten_env.log_info(
            f"   - Vendor info present: {self.vendor_info_received}"
        )
        ten_env.log_info(
            f"   - Error code: {self.error_data.get('code', 'N/A')}"
        )
        ten_env.log_info(
            f"   - Error message: {self.error_data.get('message', 'N/A')}"
        )


def test_vendor_error(extension_name: str, config_dir: str) -> None:
    """Test ASR vendor error detection with invalid credentials."""

    # Audio file path
    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data/16k_en_us_helloworld.pcm"
    )

    # Get config file path
    config_file_path = os.path.join(config_dir, DEFAULT_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    # Load config file
    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    # Create and run tester
    tester = VendorErrorTester(audio_file_path=audio_file_path)
    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()
    assert (
        error is None
    ), f"test_asr_result err code: {error.error_code()} message: {error.error_message()}"
