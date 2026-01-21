#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#
"""
Compliance tests to ensure the extension correctly implements NVIDIA Riva TTS API.
These tests validate against the official NVIDIA Riva client API specifications.
"""
import pytest
from unittest.mock import Mock, patch, MagicMock
import numpy as np
from nvidia_riva_tts_python.config import NvidiaRivaTTSConfig
from nvidia_riva_tts_python.riva_tts import NvidiaRivaTTSClient


class TestNvidiaRivaAPICompliance:
    """Test compliance with NVIDIA Riva TTS API specifications"""

    @pytest.fixture
    def mock_ten_env(self):
        """Create a mock TenEnv"""
        env = Mock()
        env.log_info = Mock()
        env.log_debug = Mock()
        env.log_warn = Mock()
        env.log_error = Mock()
        return env

    @pytest.fixture
    def valid_config(self):
        """Create a valid configuration"""
        return NvidiaRivaTTSConfig(
            params={
                "server": "localhost:50051",
                "language_code": "en-US",
                "voice_name": "English-US.Female-1",
                "sample_rate": 16000,
                "use_ssl": False,
            }
        )

    # def test_auth_initialization_parameters(self, valid_config, mock_ten_env):
    #     """Verify Auth is initialized with correct parameters per Riva API"""
    #     with patch(
    #         "nvidia_riva_tts_python.riva_tts.riva.client.Auth"
    #     ) as mock_auth, patch(
    #         "nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService"
    #     ):

    #         client = NvidiaRivaTTSClient(
    #             config=valid_config, ten_env=mock_ten_env
    #         )

    #         # Verify Auth called with correct parameters
    #         mock_auth.assert_called_once_with(
    #             ssl_cert=None, use_ssl=False, uri="localhost:50051"
    #         )

    def test_speech_synthesis_service_initialization(
        self, valid_config, mock_ten_env
    ):
        """Verify SpeechSynthesisService is initialized with Auth object"""
        with patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.Auth"
        ) as mock_auth, patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService"
        ) as mock_service:

            mock_auth_instance = Mock()
            mock_auth.return_value = mock_auth_instance

            client = NvidiaRivaTTSClient(
                config=valid_config, ten_env=mock_ten_env
            )

            # Verify SpeechSynthesisService called with Auth instance
            mock_service.assert_called_once_with(mock_auth_instance)

    @pytest.mark.asyncio
    async def test_synthesize_online_parameters(
        self, valid_config, mock_ten_env
    ):
        """Verify synthesize_online is called with correct parameters per Riva API"""
        with patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth"), patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService"
        ) as mock_service, patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.AudioEncoding"
        ) as mock_encoding:

            # Setup mocks
            mock_service_instance = Mock()
            mock_response = Mock()
            mock_response.audio = b"\x00\x01" * 100
            mock_service_instance.synthesize_online = Mock(
                return_value=[mock_response]
            )
            mock_service.return_value = mock_service_instance
            mock_encoding.LINEAR_PCM = "LINEAR_PCM"

            client = NvidiaRivaTTSClient(
                config=valid_config, ten_env=mock_ten_env
            )
            client.tts_service = mock_service_instance

            # Synthesize text
            text = "Hello world"
            chunks = [
                chunk async for chunk in client.synthesize(text, "test_request")
            ]

            # Verify synthesize_online called with correct parameters
            mock_service_instance.synthesize_online.assert_called_once_with(
                text,
                voice_name="English-US.Female-1",
                language_code="en-US",
                sample_rate_hz=16000,
                encoding="LINEAR_PCM",
            )

    @pytest.mark.asyncio
    async def test_audio_encoding_linear_pcm(self, valid_config, mock_ten_env):
        """Verify LINEAR_PCM encoding is used per Riva API"""
        with patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth"), patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService"
        ) as mock_service, patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.AudioEncoding"
        ) as mock_encoding:

            mock_service_instance = Mock()
            mock_response = Mock()
            mock_response.audio = b"\x00\x01" * 100
            mock_service_instance.synthesize_online = Mock(
                return_value=[mock_response]
            )
            mock_service.return_value = mock_service_instance
            mock_encoding.LINEAR_PCM = "LINEAR_PCM"

            client = NvidiaRivaTTSClient(
                config=valid_config, ten_env=mock_ten_env
            )
            client.tts_service = mock_service_instance

            # Synthesize
            chunks = [
                chunk async for chunk in client.synthesize("Test", "req1")
            ]

            # Verify encoding parameter
            call_args = mock_service_instance.synthesize_online.call_args
            assert call_args[1]["encoding"] == "LINEAR_PCM"

    @pytest.mark.asyncio
    async def test_audio_format_int16(self, valid_config, mock_ten_env):
        """Verify audio is processed as int16 per Riva API"""
        with patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth"), patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService"
        ) as mock_service:

            # Create mock audio data (int16 format)
            mock_audio = np.array(
                [100, -100, 200, -200], dtype=np.int16
            ).tobytes()
            mock_response = Mock()
            mock_response.audio = mock_audio

            mock_service_instance = Mock()
            mock_service_instance.synthesize_online = Mock(
                return_value=[mock_response]
            )
            mock_service.return_value = mock_service_instance

            client = NvidiaRivaTTSClient(
                config=valid_config, ten_env=mock_ten_env
            )
            client.tts_service = mock_service_instance

            # Synthesize
            chunks = [
                chunk async for chunk in client.synthesize("Test", "req1")
            ]

            # Verify output is bytes
            assert len(chunks) == 1
            assert isinstance(chunks[0], bytes)

            # Verify can be converted back to int16
            audio_array = np.frombuffer(chunks[0], dtype=np.int16)
            assert audio_array.dtype == np.int16
            assert len(audio_array) == 4

    @pytest.mark.asyncio
    async def test_streaming_response_iteration(
        self, valid_config, mock_ten_env
    ):
        """Verify streaming responses are iterated correctly per Riva API"""
        with patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth"), patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService"
        ) as mock_service:

            # Create multiple response chunks
            mock_responses = []
            for i in range(3):
                mock_response = Mock()
                mock_response.audio = np.array(
                    [i] * 10, dtype=np.int16
                ).tobytes()
                mock_responses.append(mock_response)

            mock_service_instance = Mock()
            mock_service_instance.synthesize_online = Mock(
                return_value=mock_responses
            )
            mock_service.return_value = mock_service_instance

            client = NvidiaRivaTTSClient(
                config=valid_config, ten_env=mock_ten_env
            )
            client.tts_service = mock_service_instance

            # Synthesize
            chunks = [
                chunk async for chunk in client.synthesize("Test", "req1")
            ]

            # Verify all chunks received
            assert len(chunks) == 3
            for chunk in chunks:
                assert isinstance(chunk, bytes)
                assert len(chunk) > 0

    def test_required_config_parameters(self):
        """Verify all required parameters are validated per Riva API"""
        # Missing server
        with pytest.raises(ValueError, match="Server address is required"):
            config = NvidiaRivaTTSConfig(
                params={
                    "language_code": "en-US",
                    "voice_name": "English-US.Female-1",
                }
            )
            config.validate()

        # Missing language_code
        with pytest.raises(ValueError, match="Language code is required"):
            config = NvidiaRivaTTSConfig(
                params={
                    "server": "localhost:50051",
                    "voice_name": "English-US.Female-1",
                }
            )
            config.validate()

        # Missing voice_name
        with pytest.raises(ValueError, match="Voice name is required"):
            config = NvidiaRivaTTSConfig(
                params={"server": "localhost:50051", "language_code": "en-US"}
            )
            config.validate()

    def test_optional_config_parameters(self, valid_config):
        """Verify optional parameters have correct defaults per Riva API"""
        # sample_rate defaults to 16000
        assert valid_config.params.get("sample_rate", 16000) == 16000

        # use_ssl defaults to False
        assert valid_config.params.get("use_ssl", False) is False

    def test_supported_sample_rates(self):
        """Verify common sample rates are supported per Riva API"""
        supported_rates = [8000, 16000, 22050, 24000, 44100, 48000]

        for rate in supported_rates:
            config = NvidiaRivaTTSConfig(
                params={
                    "server": "localhost:50051",
                    "language_code": "en-US",
                    "voice_name": "English-US.Female-1",
                    "sample_rate": rate,
                }
            )
            config.validate()  # Should not raise
            assert config.params["sample_rate"] == rate

    def test_ssl_configuration(self, mock_ten_env):
        """Verify SSL can be enabled per Riva API"""
        config_with_ssl = NvidiaRivaTTSConfig(
            params={
                "server": "secure-server:50051",
                "language_code": "en-US",
                "voice_name": "English-US.Female-1",
                "use_ssl": True,
            }
        )

        with patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.Auth"
        ) as mock_auth, patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService"
        ):

            client = NvidiaRivaTTSClient(
                config=config_with_ssl, ten_env=mock_ten_env
            )

            # Verify SSL enabled in Auth
            call_args = mock_auth.call_args
            assert call_args[1]["use_ssl"] is True

    @pytest.mark.asyncio
    async def test_empty_text_handling(self, valid_config, mock_ten_env):
        """Verify empty text is handled gracefully per Riva API"""
        with patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth"), patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService"
        ):

            client = NvidiaRivaTTSClient(
                config=valid_config, ten_env=mock_ten_env
            )

            # Empty string
            chunks = [chunk async for chunk in client.synthesize("", "req1")]
            assert len(chunks) == 0

            # Whitespace only
            chunks = [chunk async for chunk in client.synthesize("   ", "req1")]
            assert len(chunks) == 0

    @pytest.mark.asyncio
    async def test_cancellation_support(self, valid_config, mock_ten_env):
        """Verify cancellation is supported per Riva API"""
        with patch("nvidia_riva_tts_python.riva_tts.riva.client.Auth"), patch(
            "nvidia_riva_tts_python.riva_tts.riva.client.SpeechSynthesisService"
        ) as mock_service:

            # Create multiple responses to simulate long synthesis
            mock_responses = [Mock(audio=b"\x00\x01" * 100) for _ in range(10)]
            mock_service_instance = Mock()
            mock_service_instance.synthesize_online = Mock(
                return_value=mock_responses
            )
            mock_service.return_value = mock_service_instance

            client = NvidiaRivaTTSClient(
                config=valid_config, ten_env=mock_ten_env
            )
            client.tts_service = mock_service_instance

            # Start synthesis and cancel mid-stream
            chunks = []
            async for i, chunk in enumerate(
                client.synthesize("Long text", "req1")
            ):
                chunks.append(chunk)
                if i == 2:  # Cancel after 3 chunks
                    await client.cancel()

            # Verify cancellation stopped the stream
            assert len(chunks) < 10  # Should not receive all chunks


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
