import asyncio
import json
from unittest.mock import MagicMock, patch
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    TenError,
    TenErrorCode,
)

from ten_ai_base.tts2 import TTSTextInput
from ten_ai_base.message import ModuleErrorCode


class MockPollyTTSExtensionTester(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.expect_error_code = ModuleErrorCode.NON_FATAL_ERROR.value
        self.max_wait_time = 10
        self.error_received = False

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
            # If we already received and validated an error, ignore subsequent errors
            if self.error_received:
                ten_env.log_info(
                    "Already received an error, ignoring this one."
                )
                return

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
            # Mark that we received an error
            self.error_received = True
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


def create_mock_polly_response():
    """Create mock polly response data"""
    # mock pcm audio data (16kHz, 16bit, single channel)
    sample_rate = 16000
    duration_ms = 1000  # 1 second audio
    bytes_per_sample = 2  # 16bit = 2 bytes
    channels = 1  # single channel

    # calculate audio data size
    total_samples = int(sample_rate * duration_ms / 1000)
    audio_data_size = total_samples * bytes_per_sample * channels

    # generate mock audio data (random bytes)
    import random

    audio_data = bytes([random.randint(0, 255) for _ in range(audio_data_size)])

    # create mock stream object, contains iter_chunks method
    class MockAudioStream:
        def __init__(self, data, chunk_size=320):
            self.data = data
            self.chunk_size = chunk_size
            self.position = 0

        def iter_chunks(self, chunk_size=None):
            if chunk_size is None:
                chunk_size = self.chunk_size

            while self.position < len(self.data):
                end_pos = min(self.position + chunk_size, len(self.data))
                yield self.data[self.position : end_pos]
                self.position = end_pos

        def close(self):
            pass

    return MockAudioStream(audio_data)


@patch("boto3.Session")
@patch("boto3.client")
def test_polly_tts_success_mock(mock_boto_client, mock_boto_session):
    """test polly tts success mock"""
    # set mock
    mock_session = MagicMock()
    mock_polly = MagicMock()
    mock_boto_session.return_value = mock_session
    mock_session.client.return_value = mock_polly

    # mock polly synthesize_speech response
    mock_polly.synthesize_speech.side_effect = lambda **kwargs: {
        "AudioStream": create_mock_polly_response(),
        "ContentType": "audio/pcm",
        "RequestCharacters": 25,
    }

    property_json = {
        "log_level": "DEBUG",
        "params": {
            "region_name": "us-west-2",
            "aws_access_key_id": "fake_access_key_id",
            "aws_secret_access_key": "fake_secret_access_key",
            "engine": "neural",
            "voice": "Joanna",
            "sample_rate": "16000",
            "lang_code": "en-US",
            "audio_format": "pcm",
        },
    }

    tester = MockPollyTTSExtensionTester()
    tester.set_test_mode_single("polly_tts", json.dumps(property_json))
    tester.max_wait_time = 30
    err = tester.run()
    assert (
        err is None
    ), f"test_polly_tts_success_mock err: {err.error_message()}"


@patch("boto3.Session")
@patch("boto3.client")
def test_polly_tts_error_mock(mock_boto_client, mock_boto_session):
    """test polly tts error mock"""
    # set mock
    mock_session = MagicMock()
    mock_polly = MagicMock()
    mock_boto_session.return_value = mock_session
    mock_session.client.return_value = mock_polly

    # mock polly synthesize_speech throw exception
    from botocore.exceptions import ClientError

    error_response = {
        "Error": {
            "Code": "InvalidParameterValue",
            "Message": "Invalid parameter value",
        }
    }
    mock_polly.synthesize_speech.side_effect = ClientError(
        error_response, "synthesize_speech"
    )

    property_json = {
        "log_level": "DEBUG",
        "params": {
            "region_name": "us-west-2",
            "aws_access_key_id": "fake_access_key_id",
            "aws_secret_access_key": "fake_secret_access_key",
            "engine": "neural",
            "voice": "InvalidVoice",  # invalid voice
            "sample_rate": "16000",
            "lang_code": "en-US",
            "audio_format": "pcm",
        },
    }

    tester = MockPollyTTSExtensionTester()
    tester.expect_error_code = ModuleErrorCode.FATAL_ERROR.value
    tester.set_test_mode_single("polly_tts", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_polly_tts_error_mock err: {err.error_message}"


@patch("boto3.Session")
def test_polly_tts_invalid_credentials_mock(mock_boto_session):
    """test polly tts invalid credentials mock"""
    # set mock throw authentication error
    from botocore.exceptions import NoCredentialsError

    mock_boto_session.side_effect = NoCredentialsError()

    property_json = {
        "log_level": "DEBUG",
        "params": {
            "region_name": "us-west-2",
            "aws_access_key_id": "invalid_key",
            "aws_secret_access_key": "invalid_secret",
        },
    }

    tester = MockPollyTTSExtensionTester()
    tester.expect_error_code = ModuleErrorCode.FATAL_ERROR.value
    tester.set_test_mode_single("polly_tts", json.dumps(property_json))
    err = tester.run()
    assert (
        err is None
    ), f"test_polly_tts_invalid_credentials_mock err: {err.error_message()}"


@patch("boto3.Session")
@patch("boto3.client")
def test_polly_tts_network_timeout_mock(mock_boto_client, mock_boto_session):
    """test polly tts network timeout mock"""
    # set mock
    mock_session = MagicMock()
    mock_polly = MagicMock()
    mock_boto_session.return_value = mock_session
    mock_session.client.return_value = mock_polly

    # mock network timeout
    from botocore.exceptions import ReadTimeoutError

    mock_polly.synthesize_speech.side_effect = ReadTimeoutError(
        endpoint_url="https://polly.us-west-2.amazonaws.com",
        operation_name="synthesize_speech",
    )

    property_json = {
        "log_level": "DEBUG",
        "params": {
            "region_name": "us-west-2",
            "aws_access_key_id": "fake_access_key_id",
            "aws_secret_access_key": "fake_secret_access_key",
            "engine": "neural",
            "voice": "Joanna",
            "sample_rate": "16000",
            "lang_code": "en-US",
            "audio_format": "pcm",
        },
    }

    tester = MockPollyTTSExtensionTester()
    tester.expect_error_code = ModuleErrorCode.NON_FATAL_ERROR.value
    tester.set_test_mode_single("polly_tts", json.dumps(property_json))
    err = tester.run()
    assert (
        err is None
    ), f"test_polly_tts_network_timeout_mock err: {err.error_message()}"


def test_polly_tts_config_validation():
    """test polly tts config validation"""
    from ..config import PollyTTSConfig

    # test valid config
    valid_config = PollyTTSConfig(
        params={
            "aws_access_key_id": "test_key",
            "aws_secret_access_key": "test_secret",
            "region_name": "us-west-2",
            "engine": "neural",
            "voice": "Joanna",
            "sample_rate": "16000",
            "lang_code": "en-US",
            "audio_format": "pcm",
        }
    )

    assert valid_config.params["aws_access_key_id"] == "test_key"
    assert valid_config.params["aws_secret_access_key"] == "test_secret"
    assert valid_config.params["region_name"] == "us-west-2"
    assert valid_config.params["engine"] == "neural"
    assert valid_config.params["voice"] == "Joanna"

    # test config with missing required params (should raise validation error)
    try:
        invalid_config = PollyTTSConfig(params={})
        invalid_config.validate()
        assert (
            False
        ), "Should have raised ValueError for missing aws_access_key_id"
    except ValueError as e:
        assert "aws_access_key_id" in str(e)


if __name__ == "__main__":
    # run all tests
    test_polly_tts_success_mock()
    test_polly_tts_error_mock()
    test_polly_tts_invalid_credentials_mock()
    test_polly_tts_network_timeout_mock()
    test_polly_tts_config_validation()
    print("all mock tests passed!")
