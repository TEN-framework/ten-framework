import asyncio
import json
import threading
from unittest.mock import patch, MagicMock, AsyncMock

from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    TenError,
    TenErrorCode,
)
from ten_ai_base.struct import ASRResult


class GoogleAsrExtensionTester(AsyncExtensionTester):
    """
    A dedicated tester for the Google ASR Extension.
    Its main role is to check the results received from the extension.
    """

    def __init__(self):
        super().__init__()
        self.asr_final_result_received = False

    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        data_name = data.get_name()
        if data_name == "asr_result":
            self.asr_final_result_received = True
            ten_env_tester.log_info("Final ASR result received, stopping test.")
            ten_env_tester.stop_test()

    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if not self.asr_final_result_received:
            err = TenError.create(
                TenErrorCode.ErrorCodeGeneric,
                "Test finished without receiving a final ASR result",
            )
            ten_env_tester.set_error(err)


def test_initialization_and_connection():
    """
    Tests the basic initialization and connection flow by mocking the GoogleASRClient.
    """

    captured_on_result_callback = None
    main_loop = asyncio.get_event_loop()

    def trigger_final_response():
        """
        Runs in a thread, sends a result back to the main event loop.
        """
        if not captured_on_result_callback:
            return

        final_result = ASRResult(
            is_final=True,
            text="hello world",
            words=[],
            confidence=0.9,
            language="en-US",
        )

        asyncio.run_coroutine_threadsafe(
            captured_on_result_callback(final_result), loop=main_loop
        )

    # This is now a synchronous function, just like in the Azure example
    def fake_start():
        threading.Timer(0.5, trigger_final_response).start()

    def fake_init(
        self,
        config,
        ten_env,
        on_result_callback: callable,
        on_error_callback: callable,
    ):
        nonlocal captured_on_result_callback
        captured_on_result_callback = on_result_callback

        # Assign the synchronous fake_start directly to the start method.
        # The 'await' in the extension will handle the None return value correctly.
        self.start = fake_start
        self.stop = AsyncMock()
        self.send_audio = AsyncMock()
        self.finalize = AsyncMock()

    MockGoogleASRClient = MagicMock()
    MockGoogleASRClient.side_effect = fake_init

    with patch("extension.GoogleASRClient", MockGoogleASRClient):
        property_json = {
            "params": {
                "project_id": "fake-project-id",
                "language_codes": ["en-US"],
                "model": "long",
            }
        }

        tester = GoogleAsrExtensionTester()
        tester.set_test_mode_single(
            "google_asr_python", json.dumps(property_json)
        )

        err = tester.run()

        assert (
            err is None
        ), f"test_initialization_and_connection failed with error: {err}"
        assert tester.asr_final_result_received is True
