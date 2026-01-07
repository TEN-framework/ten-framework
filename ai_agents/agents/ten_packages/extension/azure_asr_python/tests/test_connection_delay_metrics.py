import asyncio
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
from .mock import patch_azure_ws  # noqa: F401


class ConnectionDelayMetricsExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False
        self.connection_delay_ms: int | None = None
        self.metrics_received = False
        self.test_timeout_task: asyncio.Task[None] | None = None

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        while not self.stopped:
            chunk = b"\x01\x02" * 160  # 320 bytes (16-bit * 160 samples)
            if not chunk:
                break
            audio_frame = AudioFrame.create("pcm_frame")
            metadata = {"session_id": "test_connection_delay"}
            audio_frame.set_property_from_json("metadata", json.dumps(metadata))
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)
            await asyncio.sleep(0.1)

    async def timeout_handler(self, ten_env_tester: AsyncTenEnvTester):
        # Wait for connection delay metrics to be received
        await asyncio.sleep(3.0)

        # If metrics not received by this time, fail the test
        if not self.metrics_received:
            err = TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message="Connection delay metrics not received within timeout",
            )
            ten_env_tester.stop_test(err)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        self.sender_task = asyncio.create_task(
            self.audio_sender(ten_env_tester)
        )
        self.test_timeout_task = asyncio.create_task(
            self.timeout_handler(ten_env_tester)
        )

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
        if data_name == "metrics":
            # Check the data structure.
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            ten_env_tester.log_info(
                f"tester recv metrics, data_dict: {data_dict}"
            )

            # Validate basic metrics structure
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "id" in data_dict,
                f"id is not in data_dict: {data_dict}",
            )
            self.stop_test_if_checking_failed(
                ten_env_tester,
                "module" in data_dict and data_dict["module"] == "asr",
                f"module is not in data_dict or not 'asr': {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "vendor" in data_dict and data_dict["vendor"] == "microsoft",
                f"vendor is not in data_dict or not 'microsoft': {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "metrics" in data_dict,
                f"metrics is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "metadata" in data_dict,
                f"metadata is not in data_dict: {data_dict}",
            )

            metrics = data_dict["metrics"]

            # Check if connect_delay is present (the actual field name used by the extension)
            if "connect_delay" in metrics:
                self.connection_delay_ms = metrics["connect_delay"]
                self.metrics_received = True

                ten_env_tester.log_info(
                    f"Connection delay metrics received: {self.connection_delay_ms}ms"
                )

                # Validate that connect_delay is a positive integer
                self.stop_test_if_checking_failed(
                    ten_env_tester,
                    isinstance(self.connection_delay_ms, int),
                    f"connect_delay is not an integer: {self.connection_delay_ms}",
                )

                if isinstance(self.connection_delay_ms, int):
                    self.stop_test_if_checking_failed(
                        ten_env_tester,
                        self.connection_delay_ms >= 0,
                        f"connect_delay is negative: {self.connection_delay_ms}",
                    )

                    # For the simulated connection, delay should be reasonable (< 2000ms)
                    self.stop_test_if_checking_failed(
                        ten_env_tester,
                        self.connection_delay_ms < 2000,
                        f"connect_delay is too large (should be < 2000ms for test): {self.connection_delay_ms}",
                    )

                ten_env_tester.log_info(
                    f"✓ Connection delay metrics validation passed: {self.connection_delay_ms}ms"
                )

                # Test passed, stop the test
                ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass
        if self.test_timeout_task:
            _ = self.test_timeout_task.cancel()
            try:
                await self.test_timeout_task
            except asyncio.CancelledError:
                pass


def test_connection_delay_metrics(patch_azure_ws):
    """
    Test that connect_delay is properly reported in metrics.

    This test verifies that:
    1. The extension measures the time between start_continuous_recognition
       and the connected event
    2. The connection delay is reported via send_connect_delay_metrics
    3. The metrics data contains connect_delay field
    4. The connect_delay value is reasonable
    """

    def fake_start_continuous_recognition():
        def triggerConnected():
            # Trigger connected event after a simulated delay
            event = SimpleNamespace(session_id="test_connection_delay")
            patch_azure_ws.event_handlers["connected"](event)

            # Also trigger session_started for proper initialization
            threading.Timer(0.1, triggerSessionStarted).start()

        def triggerSessionStarted():
            event = SimpleNamespace(session_id="test_connection_delay")
            patch_azure_ws.event_handlers["session_started"](event)

        # Simulate connection establishment with a delay
        # The connection happens 0.5 seconds after start_continuous_recognition
        threading.Timer(0.5, triggerConnected).start()
        return None

    def fake_stop_continuous_recognition():
        return None

    # Inject into recognizer
    patch_azure_ws.recognizer_instance.start_continuous_recognition.side_effect = (
        fake_start_continuous_recognition
    )

    patch_azure_ws.recognizer_instance.stop_continuous_recognition.side_effect = (
        fake_stop_continuous_recognition
    )

    property_json = {
        "params": {
            "key": "fake_key",
            "region": "fake_region",
        }
    }

    tester = ConnectionDelayMetricsExtensionTester()
    tester.set_test_mode_single("azure_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_connection_delay_metrics err: {err}"

    # Verify that connection delay was actually captured
    assert (
        tester.connection_delay_ms is not None
    ), "connect_delay was not received in metrics"
    assert (
        tester.connection_delay_ms >= 400
    ), f"connect_delay ({tester.connection_delay_ms}ms) should be >= 400ms given 0.5s simulated delay"

    print(
        f"✓ Test passed: Connection delay metrics = {tester.connection_delay_ms}ms"
    )
