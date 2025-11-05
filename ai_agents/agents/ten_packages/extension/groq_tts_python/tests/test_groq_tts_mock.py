import asyncio
import json
from unittest.mock import AsyncMock, MagicMock, patch
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    TenError,
    TenErrorCode,
)

from ten_ai_base.tts2 import TTSTextInput
from ten_ai_base.message import ModuleErrorCode


class MockGroqTTSExtensionTester(AsyncExtensionTester):
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
            text="hello world, hello groq",
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


class MockAsyncIterator:
    def __init__(self, items):
        self.items = items
        self.index = 0

    def __iter__(self):
        return iter(self.items)

    def __aiter__(self):
        return self

    async def __anext__(self):
        if self.index >= len(self.items):
            raise StopAsyncIteration
        item = self.items[self.index]
        self.index += 1
        return item


def test_groq_tts_extension_success():
    """test groq tts extension success"""
    # mock groq module and its components
    with patch("groq.AsyncGroq") as mock_groq_class:

        # create mock instance
        mock_groq_instance = MagicMock()
        mock_groq_class.return_value = mock_groq_instance

        # Mock close as an async method
        mock_groq_instance.close = AsyncMock()

        # mock the streaming response
        mock_streaming_response = MagicMock()
        mock_groq_instance.with_streaming_response.audio.speech.create.return_value = (
            mock_streaming_response
        )

        # mock the stream context manager
        mock_stream = MagicMock()
        mock_streaming_response.__aenter__.return_value = mock_stream
        mock_streaming_response.__aexit__.return_value = None

        # mock the stream iterator
        async def mock_iter_bytes():
            # simulate wav file chunks
            wav_header = b"RIFF\x24\x08\x00\x00WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00\x40\x1f\x00\x00\x80\x3e\x00\x00\x02\x00\x10\x00data\x00\x08\x00\x00"
            audio_data = b"\x00\x00\x01\x00\x02\x00\x03\x00"
            yield wav_header
            yield audio_data

        mock_stream.iter_bytes.return_value = mock_iter_bytes()

        # mock the wav stream parser
        with patch(
            "groq_tts_python.wav_stream_parser.WavStreamParser"
        ) as mock_wav_parser_class:
            mock_wav_parser_instance = MagicMock()
            mock_wav_parser_class.return_value = mock_wav_parser_instance

            # mock the format info as async function
            async def mock_get_format_info():
                return {"sample_rate": 16000, "channels": 1, "sample_width": 2}

            mock_wav_parser_instance.get_format_info = mock_get_format_info

            # mock the audio chunks using MockAsyncIterator
            audio_chunks = [
                b"mock_audio_chunk_1",
                b"mock_audio_chunk_2",
                b"mock_audio_chunk_3",
            ]
            mock_wav_parser_instance.__aiter__.return_value = MockAsyncIterator(
                audio_chunks
            )

            property_json = {
                "log_level": "DEBUG",
                "dump": False,
                "dump_path": "/tmp/groq_tts_test.pcm",
                "params": {
                    "api_key": "fake_api_key",
                    "model": "playai-tts",
                    "voice": "Arista-PlayAI",
                    "response_format": "wav",
                    "sample_rate": 16000,
                    "speed": 1.0,
                },
            }

            tester = MockGroqTTSExtensionTester()
            tester.set_test_mode_single(
                "groq_tts_python", json.dumps(property_json)
            )
            tester.max_wait_time = 30

            err = tester.run()

            # simple assert - as long as no exception is thrown, it is considered successful
            assert (
                err is None
            ), f"test_groq_tts_extension_success err: {err.error_message() if err else 'None'}"
