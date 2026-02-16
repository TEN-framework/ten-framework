#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#
import pytest
from unittest.mock import Mock, AsyncMock, patch, MagicMock
from nvidia_riva_tts_python.extension import NvidiaRivaTTSExtension
from nvidia_riva_tts_python.config import NvidiaRivaTTSConfig
from nvidia_riva_tts_python.riva_tts import NvidiaRivaTTSClient


@pytest.fixture
def mock_ten_env():
    """Create a mock TenEnv for extension testing"""
    env = Mock()
    env.log_info = Mock()
    env.log_debug = Mock()
    env.log_warn = Mock()
    env.log_error = Mock()
    return env


@pytest.fixture
def valid_config():
    """Create a valid configuration for testing"""
    return NvidiaRivaTTSConfig(
        params={
            "server": "localhost:50051",
            "language_code": "en-US",
            "voice_name": "English-US.Female-1",
            "sample_rate": 16000,
            "use_ssl": False,
        }
    )


class TestNvidiaRivaTTSExtension:
    """Test cases for NvidiaRivaTTSExtension"""

    def test_extension_initialization(self):
        """Test extension can be initialized"""
        extension = NvidiaRivaTTSExtension("test_extension")
        assert extension is not None
        assert extension.vendor() == "nvidia_riva"

    @pytest.mark.asyncio
    async def test_create_config(self):
        """Test configuration creation from JSON"""
        extension = NvidiaRivaTTSExtension("test_extension")
        config_json = """{
            "params": {
                "server": "localhost:50051",
                "language_code": "en-US",
                "voice_name": "English-US.Female-1",
                "sample_rate": 16000
            }
        }"""

        config = await extension.create_config(config_json)
        assert isinstance(config, NvidiaRivaTTSConfig)
        assert config.params["server"] == "localhost:50051"
        assert config.params["language_code"] == "en-US"

    def test_synthesize_audio_sample_rate(self, valid_config):
        """Test sample rate retrieval"""
        extension = NvidiaRivaTTSExtension("test_extension")
        extension.config = valid_config

        sample_rate = extension.synthesize_audio_sample_rate()
        assert sample_rate == 16000


class TestNvidiaRivaTTSClient:
    """Test cases for NvidiaRivaTTSClient"""

    @patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth")
    @patch("nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService")
    def test_client_initialization(
        self, mock_service, mock_auth, valid_config, mock_ten_env
    ):
        """Test client initialization"""
        client = NvidiaRivaTTSClient(config=valid_config, ten_env=mock_ten_env)

        assert client is not None
        assert client.config == valid_config
        mock_auth.assert_called_once()
        mock_service.assert_called_once()

    @patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth")
    @patch("nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService")
    @pytest.mark.asyncio
    async def test_cancel(
        self, mock_service, mock_auth, valid_config, mock_ten_env
    ):
        """Test cancellation"""
        client = NvidiaRivaTTSClient(config=valid_config, ten_env=mock_ten_env)

        await client.cancel()
        assert client._is_cancelled is True

    @patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth")
    @patch("nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService")
    @pytest.mark.asyncio
    async def test_synthesize_empty_text(
        self, mock_service, mock_auth, valid_config, mock_ten_env
    ):
        """Test synthesis with empty text"""
        client = NvidiaRivaTTSClient(config=valid_config, ten_env=mock_ten_env)

        # Should return without yielding anything
        result = [
            chunk async for chunk in client.synthesize("", "test_request")
        ]
        assert len(result) == 0

    @patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth")
    @patch("nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService")
    @pytest.mark.asyncio
    async def test_synthesize_with_text(
        self, mock_service, mock_auth, valid_config, mock_ten_env
    ):
        """Test synthesis with valid text"""
        # Mock the service response
        mock_response = Mock()
        mock_response.audio = b"\x00\x01" * 100  # Mock audio data

        mock_service_instance = Mock()
        mock_service_instance.synthesize_online = Mock(
            return_value=[mock_response]
        )
        mock_service.return_value = mock_service_instance

        client = NvidiaRivaTTSClient(config=valid_config, ten_env=mock_ten_env)
        client.tts_service = mock_service_instance

        # Synthesize text
        chunks = [
            chunk
            async for chunk in client.synthesize("Hello world", "test_request")
        ]

        assert len(chunks) > 0
        assert isinstance(chunks[0], bytes)
        mock_service_instance.synthesize_online.assert_called_once()


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
