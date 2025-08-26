import unittest
from unittest.mock import Mock, patch
import asyncio
from ten_packages.extension.polly_tts.polly_tts import PollyTTS, PollyTTSParams


def create_mock_audio_stream(audio_data, chunk_size=320):
    """create mock audio stream object, contains iter_chunks method"""

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

    return MockAudioStream(audio_data, chunk_size)


class TestPollyTTSParams(unittest.TestCase):
    """test polly tts params class"""

    def test_valid_params(self):
        """test valid params"""
        params = PollyTTSParams(
            aws_access_key_id="test_key",
            aws_secret_access_key="test_secret",
            region_name="us-west-2",
            engine="neural",
            voice="Joanna",
            sample_rate="16000",
            lang_code="en-US",
            audio_format="pcm",
        )

        self.assertEqual(params.aws_access_key_id, "test_key")
        self.assertEqual(params.aws_secret_access_key, "test_secret")
        self.assertEqual(params.region_name, "us-west-2")
        self.assertEqual(params.engine, "neural")
        self.assertEqual(params.voice, "Joanna")
        self.assertEqual(params.sample_rate, "16000")
        self.assertEqual(params.lang_code, "en-US")
        self.assertEqual(params.audio_format, "pcm")

    def test_default_params(self):
        """test default params"""
        params = PollyTTSParams(
            aws_access_key_id="test_key", aws_secret_access_key="test_secret"
        )

        self.assertEqual(params.engine, "neural")
        self.assertEqual(params.voice, "Joanna")
        self.assertEqual(params.sample_rate, "16000")
        self.assertEqual(params.lang_code, "en-US")
        self.assertEqual(params.audio_format, "pcm")

    def test_to_session_params(self):
        """test to_session_params method"""
        params = PollyTTSParams(
            aws_access_key_id="test_key",
            aws_secret_access_key="test_secret",
            region_name="us-west-2",
            profile_name="test_profile",
            aws_account_id="123456789",
        )

        session_params = params.to_session_params()
        expected = {
            "aws_access_key_id": "test_key",
            "aws_secret_access_key": "test_secret",
            "aws_session_token": None,
            "region_name": "us-west-2",
            "profile_name": "test_profile",
            "aws_account_id": "123456789",
        }
        self.assertEqual(session_params, expected)

    def test_to_synthesize_speech_params(self):
        """test to_synthesize_speech_params method"""
        params = PollyTTSParams(
            aws_access_key_id="test_key",
            aws_secret_access_key="test_secret",
            region_name="us-west-2",
            engine="neural",
            voice="Joanna",
            sample_rate="16000",
            lang_code="en-US",
            audio_format="pcm",
        )

        speech_params = params.to_synthesize_speech_params()
        expected = {
            "Engine": "neural",
            "VoiceId": "Joanna",
            "SampleRate": "16000",
            "LanguageCode": "en-US",
            "OutputFormat": "pcm",
        }
        self.assertEqual(speech_params, expected)


class TestPollyTTS(unittest.TestCase):
    """test polly tts class"""

    def setUp(self):
        """set up test environment"""
        self.params = PollyTTSParams(
            aws_access_key_id="test_key",
            aws_secret_access_key="test_secret",
            region_name="us-west-2",
        )

    @patch("boto3.Session")
    def test_init(self, mock_session):
        """test initialization"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        polly_tts = PollyTTS(
            self.params,
            timeout=10.0,
            max_retries=2,
            retry_delay=0.5,
            chunk_interval_ms=100,
        )

        self.assertEqual(polly_tts.params, self.params)
        self.assertEqual(polly_tts.frame_size, 100 * 16000 * 1 * 2 / 1000)
        self.assertFalse(polly_tts._closed)
        self.assertEqual(polly_tts.timeout, 10.0)
        self.assertEqual(polly_tts.max_retries, 2)
        self.assertEqual(polly_tts.retry_delay, 0.5)

    def test_context_manager(self):
        """test context manager"""
        with patch("boto3.Session") as mock_session:
            mock_session_instance = Mock()
            mock_session.return_value = mock_session_instance

            with PollyTTS(self.params) as polly_tts:
                self.assertFalse(polly_tts._closed)

            # check close method is called
            self.assertTrue(polly_tts._closed)

    def test_close(self):
        """test close method"""
        with patch("boto3.Session") as mock_session:
            mock_session_instance = Mock()
            mock_session.return_value = mock_session_instance

            polly_tts = PollyTTS(self.params)
            polly_tts.close()

            self.assertTrue(polly_tts._closed)

    @patch("boto3.Session")
    def test_synthesize_speech_success(self, mock_session):
        """test synthesize_speech success"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        # mock audio data
        audio_data = b"fake_audio_data_12345"
        mock_response = {
            "AudioStream": create_mock_audio_stream(audio_data),
            "ContentType": "audio/pcm",
            "RequestCharacters": 25,
        }
        mock_polly.synthesize_speech.return_value = mock_response

        # create polly tts instance
        polly_tts = PollyTTS(self.params)
        result = list(polly_tts.synthesize_speech("Hello world"))

        # check result
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], audio_data)

        # check call
        mock_polly.synthesize_speech.assert_called_once()
        call_args = mock_polly.synthesize_speech.call_args[1]
        self.assertEqual(call_args["Text"], "Hello world")
        self.assertEqual(call_args["Engine"], "neural")
        self.assertEqual(call_args["VoiceId"], "Joanna")
        self.assertEqual(call_args["SampleRate"], "16000")
        self.assertEqual(call_args["LanguageCode"], "en-US")
        self.assertEqual(call_args["OutputFormat"], "pcm")

    @patch("boto3.Session")
    def test_synthesize_speech_error(self, mock_session):
        """test synthesize_speech error"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        # mock error
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

        polly_tts = PollyTTS(self.params)

        with self.assertRaises(ClientError):
            list(polly_tts.synthesize_speech("Hello world"))

    @patch("boto3.Session")
    def test_async_synthesize_speech_success(self, mock_session):
        """test async_synthesize_speech success"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        # mock audio data
        audio_data = b"fake_audio_data_12345"
        mock_response = {
            "AudioStream": create_mock_audio_stream(audio_data),
            "ContentType": "audio/pcm",
            "RequestCharacters": 25,
        }
        mock_polly.synthesize_speech.return_value = mock_response

        async def test_async():
            polly_tts = PollyTTS(self.params)
            result = []
            async for chunk in polly_tts.async_synthesize_speech("Hello world"):
                result.append(chunk)
            return result

        result = asyncio.run(test_async())

        # check result
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], audio_data)

    @patch("boto3.Session")
    def test_async_synthesize_speech_timeout(self, mock_session):
        """test async_synthesize_speech timeout"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        # mock timeout
        mock_polly.synthesize_speech.side_effect = asyncio.TimeoutError()

        async def test_async():
            polly_tts = PollyTTS(self.params)
            result = []
            try:
                async for chunk in polly_tts.async_synthesize_speech(
                    "Hello world", timeout=0.1
                ):
                    result.append(chunk)
            except asyncio.TimeoutError:
                pass
            return result

        result = asyncio.run(test_async())

        # check result is empty
        self.assertEqual(len(result), 0)

    @patch("boto3.Session")
    def test_async_synthesize_speech_with_retry_success(self, mock_session):
        """test async_synthesize_speech_with_retry success"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        # mock audio data
        audio_data = b"fake_audio_data_12345"
        mock_response = {
            "AudioStream": create_mock_audio_stream(audio_data),
            "ContentType": "audio/pcm",
            "RequestCharacters": 25,
        }
        mock_polly.synthesize_speech.return_value = mock_response

        async def test_async():
            polly_tts = PollyTTS(self.params)
            result = []
            async for chunk in polly_tts.async_synthesize_speech_with_retry(
                "Hello world"
            ):
                result.append(chunk)
            return result

        result = asyncio.run(test_async())

        # check result
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], audio_data)

    @patch("boto3.Session")
    def test_async_synthesize_speech_with_retry_failure(self, mock_session):
        """test async_synthesize_speech_with_retry failure"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        # mock continuous failure
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

        async def test_async():
            polly_tts = PollyTTS(self.params, max_retries=2, retry_delay=0.1)
            result = []
            try:
                async for chunk in polly_tts.async_synthesize_speech_with_retry(
                    "Hello world"
                ):
                    result.append(chunk)
            except ClientError:
                pass
            return result

        result = asyncio.run(test_async())

        # check result is empty
        self.assertEqual(len(result), 0)
        # check retry count
        self.assertEqual(
            mock_polly.synthesize_speech.call_count, 3
        )  # initial call + 2 retries


if __name__ == "__main__":
    unittest.main()
