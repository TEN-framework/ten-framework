import threading
from types import SimpleNamespace
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

# We must import it, which means this test fixture will be automatically executed
from .mock import patch_sarvam_ws  # noqa: F401


class SarvamAsrExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.recv_error_count = 0

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass

    def stop_test_if_checking_failed(
        self,
        ten_env_tester: AsyncTenEnvTester,
        success: bool,
        error_message: str,
    ) -> None:
        if not success:
            err = TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message=error_message,
            )
            ten_env_tester.stop_test(err)

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        data_name = data.get_name()
        if data_name == "error":
            self.recv_error_count += 1
        elif data_name == "asr_result":
            self.stop_test_if_checking_failed(
                ten_env_tester,
                self.recv_error_count == 3,
                f"recv_error_count is not 3: {self.recv_error_count}",
            )
            ten_env_tester.stop_test()


# For the first three start_connection calls, ws_connect will raise an exception.
# On the fourth start_connection call, a successful transcript will be received.
def test_reconnect(patch_sarvam_ws):
    start_connection_attempts = 0

    def trigger_transcript_messages():
        """Add WebSocket messages to simulate successful Sarvam API response."""
        transcript_message = {
            "type": "data",
            "data": {
                "transcript": "hello world",
                "language_code": "en-IN",
                "request_id": "test-request-123",
                "metrics": {
                    "audio_duration": 2.0,
                    "processing_latency": 0.1,
                },
            },
        }

        msg = patch_sarvam_ws.MockWebSocketMessage(
            msg_type=patch_sarvam_ws.WSMsgType.TEXT,
            data=json.dumps(transcript_message),
        )
        patch_sarvam_ws.add_message(msg)

    # Create a new session class that tracks attempts
    from unittest.mock import patch

    class MockSessionWithTracking:
        def __init__(self, *args, **kwargs) -> None:
            self.closed: bool = False

        async def ws_connect(self, url, headers=None, timeout=None):
            nonlocal start_connection_attempts
            start_connection_attempts += 1

            if start_connection_attempts <= 3:
                # Fail the first 3 connection attempts by raising an exception
                raise Exception("WebSocket connection error")

            # On 4th attempt, allow connection to succeed
            # Reset closed state and exception for successful connection
            patch_sarvam_ws.ws.closed = False
            patch_sarvam_ws.ws._exception = None

            # Schedule transcript message after a short delay
            def delayed_transcript():
                import time

                time.sleep(0.5)
                trigger_transcript_messages()

            sender_thread = threading.Thread(
                target=delayed_transcript, daemon=True
            )
            sender_thread.start()
            return patch_sarvam_ws.ws

        async def close(self) -> None:
            self.closed = True

    # Patch the ClientSession for the duration of the test
    with patch(
        "ten_packages.extension.sarvam_asr_python.extension.aiohttp.ClientSession",
        MockSessionWithTracking,
    ):
        property_json = {
            "params": {
                "api_key": "fake_api_key",
                "sample_rate": 16000,
            }
        }

        tester = SarvamAsrExtensionTester()
        tester.set_test_mode_single(
            "sarvam_asr_python", json.dumps(property_json)
        )
        err = tester.run()
        assert (
            err is None
        ), f"test_reconnect err code: {err.error_code()} message: {err.error_message()}"
