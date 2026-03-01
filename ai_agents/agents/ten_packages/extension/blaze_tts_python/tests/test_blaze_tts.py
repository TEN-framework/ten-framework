"""
Unit tests for BlazeTTSExtension
"""

import pytest
from unittest.mock import Mock, patch
import httpx

from blaze_tts_python import BlazeTTSExtension, BlazeTTSConfig


class TestBlazeTTSExtension:
    """Test suite for BlazeTTSExtension"""

    def test_init_with_config_dict(self, mock_config):
        """Test initialization with dict config"""
        tts = BlazeTTSExtension(config=mock_config)
        assert tts.config.api_url == "http://localhost:8000"
        assert tts.config.api_key == "test-api-key"
        assert tts.config.default_language == "vi"
        assert tts.config.default_speaker_id == "test-speaker-123"
        assert tts.endpoint == "http://localhost:8000/v1/tts"

    def test_init_with_config_object(self):
        """Test initialization with BlazeTTSConfig object"""
        config = BlazeTTSConfig(
            api_url="http://test.com",
            api_key="test-key",
            default_language="en",
            default_speaker_id="speaker-456",
        )
        tts = BlazeTTSExtension(config=config)
        assert tts.config.api_url == "http://test.com"
        assert tts.config.api_key == "test-key"
        assert tts.config.default_language == "en"
        assert tts.config.default_speaker_id == "speaker-456"

    def test_init_with_env_vars(self, monkeypatch):
        """Test initialization with environment variables"""
        monkeypatch.setenv("BLAZE_TTS_API_URL", "http://env-test.com")
        monkeypatch.setenv("BLAZE_TTS_API_KEY", "env-key")

        tts = BlazeTTSExtension(config=None)
        assert tts.config.api_url == "http://env-test.com"
        assert tts.config.api_key == "env-key"

    @patch("httpx.Client")
    def test_synthesize(self, mock_client_class, mock_api_response_synthesize):
        """Test synthesize() method"""
        # Setup mock response
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_synthesize
        mock_response.raise_for_status = Mock()

        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client

        # Initialize extension
        tts = BlazeTTSExtension(
            config={
                "api_url": "http://localhost:8000",
                "api_key": "test-key",
                "speaker_id": "test-speaker-123",
            }
        )

        # Call synthesize
        result = tts.synthesize(
            text="Xin chào",
            speaker_id="test-speaker-123",
            language="vi",
        )

        # Verify request was made correctly
        mock_client.post.assert_called_once()
        call_args = mock_client.post.call_args

        # Check endpoint
        assert call_args[0][0] == "http://localhost:8000/v1/tts"

        # Check JSON body
        json_data = call_args[1]["json"]
        assert json_data["text"] == "Xin chào"
        assert json_data["speaker_id"] == "test-speaker-123"
        assert json_data["language"] == "vi"

        # Check headers
        headers = call_args[1]["headers"]
        assert headers["Authorization"] == "Bearer test-key"

        # Verify result
        assert result["job_id"] == "test-tts-job-123"
        assert result["job_status"] == "completed"
        assert (
            result["audio_url"]
            == "https://example.com/audio/test-tts-job-123.mp3"
        )

    def test_synthesize_empty_text(self):
        """Test synthesize() with empty text raises ValueError"""
        tts = BlazeTTSExtension(config={"api_url": "http://localhost:8000"})

        with pytest.raises(ValueError, match="text cannot be empty"):
            tts.synthesize(text="", speaker_id="test-speaker")

    def test_synthesize_missing_speaker_id(self):
        """Test synthesize() without speaker_id raises ValueError"""
        tts = BlazeTTSExtension(config={"api_url": "http://localhost:8000"})

        with pytest.raises(ValueError, match="speaker_id is required"):
            tts.synthesize(text="Hello")

    @patch("httpx.Client")
    def test_synthesize_with_default_speaker_id(
        self, mock_client_class, mock_api_response_synthesize
    ):
        """Test synthesize() uses default speaker_id from config"""
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_synthesize
        mock_response.raise_for_status = Mock()

        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client

        tts = BlazeTTSExtension(
            config={
                "api_url": "http://localhost:8000",
                "speaker_id": "default-speaker",
            }
        )

        result = tts.synthesize(text="Hello")

        call_args = mock_client.post.call_args
        json_data = call_args[1]["json"]
        assert json_data["speaker_id"] == "default-speaker"

    @patch("httpx.Client")
    def test_get_speakers(self, mock_client_class, mock_api_response_speakers):
        """Test get_speakers() method"""
        # Setup mock response
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_speakers
        mock_response.raise_for_status = Mock()

        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.get.return_value = mock_response
        mock_client_class.return_value = mock_client

        # Initialize extension
        tts = BlazeTTSExtension(
            config={
                "api_url": "http://localhost:8000",
                "api_key": "test-key",
            }
        )

        # Call get_speakers
        result = tts.get_speakers()

        # Verify request was made correctly
        mock_client.get.assert_called_once()
        call_args = mock_client.get.call_args

        # Check endpoint
        assert (
            call_args[0][0] == "http://localhost:8000/v1/tts/list-speaker-ids"
        )

        # Check headers
        headers = call_args[1]["headers"]
        assert headers["Authorization"] == "Bearer test-key"

        # Verify result
        assert len(result["list_speakers"]) == 2
        assert result["list_speakers"][0]["id"] == "speaker-1"
        assert result["list_speakers"][1]["id"] == "speaker-2"

    @patch("httpx.Client")
    def test_get_job_info(self, mock_client_class, mock_api_response_job_info):
        """Test get_job_info() method"""
        # Setup mock response
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_job_info
        mock_response.raise_for_status = Mock()

        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.get.return_value = mock_response
        mock_client_class.return_value = mock_client

        # Initialize extension
        tts = BlazeTTSExtension(
            config={
                "api_url": "http://localhost:8000",
                "api_key": "test-key",
            }
        )

        # Call get_job_info
        result = tts.get_job_info("test-tts-job-123")

        # Verify request was made correctly
        mock_client.get.assert_called_once()
        call_args = mock_client.get.call_args

        # Check endpoint
        assert (
            call_args[0][0]
            == "http://localhost:8000/v1/tts/test-tts-job-123/info"
        )

        # Check headers
        headers = call_args[1]["headers"]
        assert headers["Authorization"] == "Bearer test-key"

        # Verify result
        assert result["job_id"] == "test-tts-job-123"
        assert result["job_status"] == "completed"
        assert (
            result["audio_url"]
            == "https://example.com/audio/test-tts-job-123.mp3"
        )

    @patch("httpx.Client")
    def test_download_audio(self, mock_client_class):
        """Test download_audio() method"""
        # Setup mock response with audio bytes
        mock_audio_bytes = b"fake audio data"
        mock_response = Mock()
        mock_response.content = mock_audio_bytes
        mock_response.raise_for_status = Mock()

        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.get.return_value = mock_response
        mock_client_class.return_value = mock_client

        # Initialize extension
        tts = BlazeTTSExtension(
            config={
                "api_url": "http://localhost:8000",
                "api_key": "test-key",
            }
        )

        # Call download_audio
        audio_data = tts.download_audio("test-tts-job-123")

        # Verify request was made correctly
        mock_client.get.assert_called_once()
        call_args = mock_client.get.call_args

        # Check endpoint
        assert (
            call_args[0][0]
            == "http://localhost:8000/v1/tts/test-tts-job-123/download"
        )

        # Check headers
        headers = call_args[1]["headers"]
        assert headers["Authorization"] == "Bearer test-key"

        # Verify result
        assert audio_data == mock_audio_bytes

    def test_process_method(self, mock_api_response_synthesize):
        """Test process() method (TEN framework interface)"""
        with patch("httpx.Client") as mock_client_class:
            # Setup mock response
            mock_response = Mock()
            mock_response.json.return_value = mock_api_response_synthesize
            mock_response.raise_for_status = Mock()

            mock_client = Mock()
            mock_client.__enter__ = Mock(return_value=mock_client)
            mock_client.__exit__ = Mock(return_value=False)
            mock_client.post.return_value = mock_response
            mock_client_class.return_value = mock_client

            # Initialize extension
            tts = BlazeTTSExtension(
                config={
                    "api_url": "http://localhost:8000",
                    "speaker_id": "test-speaker",
                }
            )

            # Call process
            result = tts.process(
                {
                    "text": "Xin chào",
                    "speaker_id": "test-speaker",
                    "language": "vi",
                }
            )

            # Verify result format
            assert result["job_id"] == "test-tts-job-123"
            assert result["status"] == "completed"
            assert "audio_url" in result

    def test_process_method_missing_text(self):
        """Test process() method raises error when text is missing"""
        tts = BlazeTTSExtension(config={"api_url": "http://localhost:8000"})

        with pytest.raises(ValueError, match="text is required in input_data"):
            tts.process({})

    def test_get_metadata(self):
        """Test get_metadata() method"""
        tts = BlazeTTSExtension(config={"api_url": "http://localhost:8000"})

        metadata = tts.get_metadata()

        assert metadata["name"] == "blaze_tts_python"
        assert metadata["version"] == "1.0.0"
        assert "tts" in metadata["capabilities"]
        assert "text_to_speech" in metadata["capabilities"]
        assert "audio/wav" in metadata["supported_formats"]
        assert "vi" in metadata["supported_languages"]

    @patch("httpx.Client")
    def test_synthesize_with_audio_speed(
        self, mock_client_class, mock_api_response_synthesize
    ):
        """Test synthesize() with custom audio_speed"""
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_synthesize
        mock_response.raise_for_status = Mock()

        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client

        tts = BlazeTTSExtension(config={"api_url": "http://localhost:8000"})

        result = tts.synthesize(
            text="Hello",
            speaker_id="test-speaker",
            audio_speed=1.5,
        )

        call_args = mock_client.post.call_args
        json_data = call_args[1]["json"]
        assert json_data["audio_speed"] == 1.5

    @patch("httpx.Client")
    def test_synthesize_with_audio_quality(
        self, mock_client_class, mock_api_response_synthesize
    ):
        """Test synthesize() with custom audio_quality"""
        mock_response = Mock()
        mock_response.json.return_value = mock_api_response_synthesize
        mock_response.raise_for_status = Mock()

        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client

        tts = BlazeTTSExtension(config={"api_url": "http://localhost:8000"})

        result = tts.synthesize(
            text="Hello",
            speaker_id="test-speaker",
            audio_quality=128,
        )

        call_args = mock_client.post.call_args
        json_data = call_args[1]["json"]
        assert json_data["audio_quality"] == 128

    @patch("httpx.Client")
    def test_synthesize_http_error(self, mock_client_class):
        """Test synthesize() handles HTTP errors"""
        # Setup mock response with error
        mock_response = Mock()
        mock_response.status_code = 400
        mock_response.text = "Bad Request"
        mock_response.raise_for_status.side_effect = httpx.HTTPStatusError(
            "Bad Request", request=Mock(), response=mock_response
        )

        mock_client = Mock()
        mock_client.__enter__ = Mock(return_value=mock_client)
        mock_client.__exit__ = Mock(return_value=False)
        mock_client.post.return_value = mock_response
        mock_client_class.return_value = mock_client

        tts = BlazeTTSExtension(config={"api_url": "http://localhost:8000"})

        with pytest.raises(httpx.HTTPStatusError):
            tts.synthesize(text="Hello", speaker_id="test-speaker")
