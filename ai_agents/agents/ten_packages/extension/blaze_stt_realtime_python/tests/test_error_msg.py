"""Error-path tests for BlazeSTTRealtimeExtension.

These run the extension inside the TEN runtime via AsyncExtensionTester and
assert that connection/credential failures are surfaced as `error` data
(through `send_asr_error`), rather than being swallowed.
"""

import json
from unittest.mock import patch

from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
)


class ExtensionTesterInvalidApiKey(AsyncExtensionTester):
    """Stops the test as soon as an `error` data message is received."""

    def __init__(self):
        super().__init__()
        self.error_received = False
        self.error_payload = None

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        ten_env_tester.log_info("on_start")

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        if data.get_name() == "error":
            error_json, _ = data.get_property_to_json("")
            print(f"received error data: {error_json}")
            self.error_payload = json.loads(error_json)
            self.error_received = True
            ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass


@patch(
    "ten_packages.extension.blaze_stt_realtime_python.extension"
    ".websockets.connect"
)
def test_invalid_api_key_error(mock_websocket_connect):
    """A failed websocket handshake is reported as a FATAL error message."""
    print("Starting test_invalid_api_key_error with mock...")

    # Mock the websocket connect to raise a 401 unauthorized error.
    mock_websocket_connect.side_effect = Exception(
        "401 Unauthorized - Invalid API key"
    )

    invalid_key_config = {
        "params": {
            "api_url": "http://localhost:8000",
            "api_key": "invalid_api_key_test",
            "language": "vi",
            "model": "stt-stream-1.5",
        },
    }

    tester = ExtensionTesterInvalidApiKey()
    tester.set_test_mode_single(
        "blaze_stt_realtime_python", json.dumps(invalid_key_config)
    )

    print("Running test with mock...")
    err = tester.run()
    print("Test with mock completed.")

    assert err is None, f"Unexpected tester error: {err}"
    assert tester.error_received, "Expected to receive error message"
    assert tester.error_payload is not None
    # FATAL_ERROR is -1000; accept int or string serialization.
    assert (
        str(tester.error_payload.get("code")) == "-1000"
    ), f"Expected FATAL_ERROR (-1000), got: {tester.error_payload}"


def test_missing_api_key_error():
    """An empty/missing API key is reported as a FATAL error message."""
    missing_key_config = {
        "params": {
            "api_url": "http://localhost:8000",
            "api_key": "",
            "language": "vi",
            "model": "stt-stream-1.5",
        },
    }

    tester = ExtensionTesterInvalidApiKey()
    tester.set_test_mode_single(
        "blaze_stt_realtime_python", json.dumps(missing_key_config)
    )
    err = tester.run()

    assert err is None, f"Unexpected tester error: {err}"
    assert tester.error_received, "Expected to receive error message"
