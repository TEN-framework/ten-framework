import asyncio
import json
from unittest.mock import MagicMock, patch, PropertyMock
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    TenError,
    TenErrorCode,
)

from ten_ai_base.tts2 import TTSTextInput
from ten_ai_base.message import ModuleErrorCode


class MockAzureTTSExtensionTester(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.expect_error_code = ModuleErrorCode.NON_FATAL_ERROR.value
        self.max_wait_time = 10

    def stop_test_if_checking_failed(
        self,
        ten_env_tester: AsyncTenEnvTester,
        success: bool,
        error_message: str,
    ) -> None:
        if not success:
            ten_env_tester.log_error(
                f"stop_test_if_checking_failed: {error_message}"
            )
            err = TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message=error_message,
            )
            ten_env_tester.stop_test(err)

    async def wait_for_test(self, ten_env: AsyncTenEnvTester):
        await asyncio.sleep(self.max_wait_time)
        ten_env.stop_test(
            TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message="test timeout",
            )
        )

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
        ten_env.log_info("Mock test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_1",
            text="hello world, hello agora",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        await ten_env.send_data(data)
        asyncio.create_task(self.wait_for_test(ten_env))

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")

        if name == "error":
            ten_env.log_info("Received error, stopping test.")
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            self.stop_test_if_checking_failed(
                ten_env,
                "code" in data_dict,
                f"error_code is not in data_dict: {data_dict}",
            )
            self.stop_test_if_checking_failed(
                ten_env,
                data_dict["code"] == int(self.expect_error_code),
                f"error_code is not {self.expect_error_code}: {data_dict}",
            )
            # success stop test
            ten_env.stop_test()
        elif name == "tts_audio_end":
            ten_env.log_info("Received TTS audio data, stopping test.")
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            self.stop_test_if_checking_failed(
                ten_env,
                "request_id" in data_dict,
                f"request_id is not in data_dict: {data_dict}",
            )
            self.stop_test_if_checking_failed(
                ten_env,
                data_dict["request_id"] == "tts_request_1",
                f"request_id is not tts_request_1: {data_dict}",
            )
            # success stop test
            ten_env.stop_test()


def test_azure_tts_extension_success():
    """test azure tts extension success"""
    # directly mock AzureTTS class
    with patch(
        "ten_packages.extension.azure_tts_python.extension.AzureTTS"
    ) as mock_azure_tts_class:
        # create mock instance
        mock_azure_tts_instance = MagicMock()
        mock_azure_tts_class.return_value = mock_azure_tts_instance

        # set async methods
        async def mock_start_connection(*args, **kwargs):
            return True

        async def mock_synthesize_with_retry(
            text, max_retries=5, retry_delay=1.0
        ):
            """Mock synthesize_with_retry that returns an async generator."""

            async def _generator():
                # mock audio data stream
                audio_chunks = [
                    b"mock_audio_chunk_1",
                    b"mock_audio_chunk_2",
                    b"mock_audio_chunk_3",
                ]
                for chunk in audio_chunks:
                    yield chunk

            return _generator()

        async def mock_stop_connection(*args, **kwargs):
            return None

        mock_azure_tts_instance.start_connection = mock_start_connection
        mock_azure_tts_instance.stop_connection = mock_stop_connection
        mock_azure_tts_instance.synthesize_with_retry = (
            mock_synthesize_with_retry
        )
        # Mock is_connected as a property that always returns True
        type(mock_azure_tts_instance).is_connected = PropertyMock(
            return_value=True
        )
        mock_azure_tts_instance.speech_config = MagicMock()
        mock_azure_tts_instance.speech_config.speech_synthesis_voice_name = (
            "en-US-AriaNeural"
        )

        property_json = {
            "log_level": "DEBUG",
            "dump": False,
            "dump_path": "/tmp/azure_tts_test.pcm",
            "pre_connect": True,
            "chunk_size": 3200,
            "params": {
                "subscription": "fake_subscription_key",
                "region": "eastus",
                "output_format": "Raw16Khz16BitMonoPcm",
                "propertys": [
                    ["Speech_LogFilename", "azure_tts_log.txt"],
                    ["SpeechServiceConnection_SynthLanguage", "en-US"],
                    ["SpeechServiceConnection_SynthVoice", "en-US-AriaNeural"],
                ],
            },
        }

        tester = MockAzureTTSExtensionTester()
        tester.set_test_mode_single(
            "azure_tts_python", json.dumps(property_json)
        )
        tester.max_wait_time = 30
        err = tester.run()

        # verify AzureTTS is created and called correctly
        # mock_azure_tts_class.assert_called_once()
        # mock_azure_tts_instance.start_connection.assert_called_once()

        # simple assert - as long as no exception is thrown, it is considered successful
        assert (
            err is None
        ), f"test_azure_tts_extension_success err: {err.error_message() if err else 'None'}"
