import unittest
from unittest.mock import Mock, patch, AsyncMock
import asyncio
from ..polly_tts import PollyTTSClient
from ..config import PollyTTSConfig


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


class TestPollyTTSClient(unittest.TestCase):
    """test polly tts client class"""

    def setUp(self):
        """set up test environment"""
        self.config = PollyTTSConfig(
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
        self.mock_ten_env = Mock()
        self.mock_ten_env.log_debug = Mock()
        self.mock_ten_env.log_error = Mock()
        self.mock_ten_env.log_warning = Mock()

    @patch("boto3.Session")
    def test_init(self, mock_session):
        """test initialization"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        client = PollyTTSClient(self.config, self.mock_ten_env)

        self.assertEqual(client.config, self.config)
        self.assertEqual(
            client.frame_size, 50 * 16000 * 1 * 2 / 1000
        )  # hardcoded 50ms
        self.assertFalse(client._closed)
        self.assertFalse(client._is_cancelled)

    @patch("boto3.Session")
    def test_synthesize_speech_success(self, mock_session):
        """test _synthesize_speech success"""
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

        # create polly tts client
        client = PollyTTSClient(self.config, self.mock_ten_env)
        result = list(
            client._synthesize_speech(
                {
                    "Text": "Hello world",
                    "Engine": "neural",
                    "VoiceId": "Joanna",
                    "SampleRate": "16000",
                    "LanguageCode": "en-US",
                    "OutputFormat": "pcm",
                }
            )
        )

        # check result
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], audio_data)

        # check call
        mock_polly.synthesize_speech.assert_called_once()

    @patch("boto3.Session")
    def test_synthesize_speech_error(self, mock_session):
        """test _synthesize_speech error"""
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

        client = PollyTTSClient(self.config, self.mock_ten_env)

        with self.assertRaises(ClientError):
            list(client._synthesize_speech({"Text": "Hello world"}))

    @patch("boto3.Session")
    def test_async_iter_from_sync(self, mock_session):
        """test _async_iter_from_sync"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        # mock audio data
        audio_data = [b"chunk1", b"chunk2", b"chunk3"]

        def sync_iter():
            for chunk in audio_data:
                yield chunk

        async def test_async():
            client = PollyTTSClient(self.config, self.mock_ten_env)
            result = []
            async for chunk in client._async_iter_from_sync(sync_iter()):
                result.append(chunk)
            return result

        result = asyncio.run(test_async())

        # check result
        self.assertEqual(len(result), 3)
        self.assertEqual(result, audio_data)

    @patch("boto3.Session")
    def test_cancel(self, mock_session):
        """test cancel method"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        async def test_async():
            client = PollyTTSClient(self.config, self.mock_ten_env)
            self.assertFalse(client._is_cancelled)
            await client.cancel()
            self.assertTrue(client._is_cancelled)

        asyncio.run(test_async())

    @patch("boto3.Session")
    def test_clean(self, mock_session):
        """test clean method"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        async def test_async():
            client = PollyTTSClient(self.config, self.mock_ten_env)
            self.assertFalse(client._closed)
            await client.clean()
            self.assertTrue(client._closed)

        asyncio.run(test_async())

    @patch("boto3.Session")
    def test_get_extra_metadata(self, mock_session):
        """test get_extra_metadata method"""
        # set mock
        mock_session_instance = Mock()
        mock_polly = Mock()
        mock_session.return_value = mock_session_instance
        mock_session_instance.client.return_value = mock_polly

        client = PollyTTSClient(self.config, self.mock_ten_env)
        metadata = client.get_extra_metadata()

        self.assertEqual(metadata["engine"], "neural")
        self.assertEqual(metadata["voice"], "Joanna")


if __name__ == "__main__":
    unittest.main()
