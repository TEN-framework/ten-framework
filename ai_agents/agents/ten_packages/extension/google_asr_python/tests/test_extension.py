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

    def fake_init(config, ten_env, on_result_callback, on_error_callback):
        mock_instance = MagicMock()
        mock_instance._on_result_callback = on_result_callback

        async def fake_start():
            await asyncio.sleep(0)
            final_result = ASRResult(
                final=True,
                text="hello world",
                words=[],
                start_ms=0,
                duration_ms=1000,
                confidence=0.9,
                language="en-US",
            )
            await mock_instance._on_result_callback(final_result)

        mock_instance.start = fake_start
        mock_instance.stop = AsyncMock()
        mock_instance.send_audio = AsyncMock()
        mock_instance.finalize = AsyncMock()
        return mock_instance

    MockGoogleASRClient = MagicMock(side_effect=fake_init)

    with patch(
        "ten_packages.extension.google_asr_python.extension.GoogleASRClient",
        MockGoogleASRClient,
    ):
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
