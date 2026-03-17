"""Tests for OracleTTS client methods (cancel, clean, get) with mocked OCI SDK."""

import asyncio
import sys
from unittest.mock import MagicMock, patch, AsyncMock

import pytest

from config import OracleTTSConfig


def _make_config(**overrides) -> OracleTTSConfig:
    defaults = {
        "tenancy": "test-tenancy",
        "user": "test-user",
        "fingerprint": "aa:bb:cc",
        "key_file": "/tmp/test.pem",
        "compartment_id": "test-compartment",
        "region": "us-phoenix-1",
        "model_name": "TTS_2_NATURAL",
        "voice_id": "Annabelle",
        "language_code": "en-US",
        "sample_rate": 16000,
        "output_format": "PCM",
    }
    defaults.update(overrides)
    return OracleTTSConfig(params=defaults)


@pytest.fixture
def mock_oci():
    """Patch the OCI SDK so OracleTTS can be instantiated without real credentials."""
    mock_client_cls = MagicMock()
    mock_client_instance = MagicMock()
    mock_client_cls.return_value = mock_client_instance

    with (
        patch.dict(sys.modules, {
            "oci": MagicMock(),
            "oci.ai_speech": MagicMock(),
            "oci.ai_speech.models": MagicMock(),
            "oci.exceptions": MagicMock(),
        }),
        patch("oracle_tts.oci") as oci_mock,
    ):
        oci_mock.ai_speech.AIServiceSpeechClient = mock_client_cls
        oci_mock.config.validate_config = MagicMock()
        yield oci_mock, mock_client_instance


class TestOracleTTSCancel:
    def test_cancel_sets_flag(self, mock_oci) -> None:
        from oracle_tts import OracleTTS

        config = _make_config()
        ten_env = MagicMock()
        tts = OracleTTS(config=config, ten_env=ten_env)

        assert tts._is_cancelled is False
        tts.cancel()
        assert tts._is_cancelled is True

    def test_clean_sets_flag_and_clears_client(self, mock_oci) -> None:
        from oracle_tts import OracleTTS

        config = _make_config()
        ten_env = MagicMock()
        tts = OracleTTS(config=config, ten_env=ten_env)

        assert tts.client is not None
        tts.clean()
        assert tts._is_cancelled is True
        assert tts.client is None


class TestOracleTTSGet:
    @pytest.mark.asyncio
    async def test_get_yields_error_when_client_none(self, mock_oci) -> None:
        from oracle_tts import OracleTTS, EVENT_TTS_ERROR

        config = _make_config()
        ten_env = MagicMock()
        tts = OracleTTS(config=config, ten_env=ten_env)
        tts.client = None

        chunks = []
        async for chunk, event, ttfb in tts.get("hello", "req-1"):
            chunks.append((chunk, event, ttfb))

        assert len(chunks) == 1
        assert chunks[0][1] == EVENT_TTS_ERROR

    @pytest.mark.asyncio
    async def test_get_cancel_stops_iteration(self, mock_oci) -> None:
        from oracle_tts import OracleTTS, EVENT_TTS_RESPONSE

        _, client_instance = mock_oci
        config = _make_config()
        ten_env = MagicMock()
        tts = OracleTTS(config=config, ten_env=ten_env)

        pcm_data = b"\x01\x02" * 8192  # >4096 to get multiple chunks

        response_mock = MagicMock()
        response_mock.data.content = b"RIFF" + b"\x00" * 4 + b"XXXX" + pcm_data
        tts.client.synthesize_speech.return_value = response_mock

        tts._strip_wav_header = staticmethod(lambda x: pcm_data)

        chunks = []
        async for chunk, event, ttfb in tts.get("hello", "req-1"):
            chunks.append((chunk, event))
            if len(chunks) == 1:
                tts.cancel()

        response_events = [c for c in chunks if c[1] == EVENT_TTS_RESPONSE]
        assert len(response_events) >= 1
        assert len(response_events) < len(pcm_data) // 4096

    @pytest.mark.asyncio
    async def test_get_successful_yields_chunks_and_end(self, mock_oci) -> None:
        from oracle_tts import (
            OracleTTS,
            EVENT_TTS_RESPONSE,
            EVENT_TTS_REQUEST_END,
        )

        _, client_instance = mock_oci
        config = _make_config()
        ten_env = MagicMock()
        tts = OracleTTS(config=config, ten_env=ten_env)

        pcm_data = b"\xAA\xBB" * 2048  # 4096 bytes = 1 chunk

        response_mock = MagicMock()
        response_mock.data.content = pcm_data
        tts.client.synthesize_speech.return_value = response_mock

        tts._strip_wav_header = staticmethod(lambda x: pcm_data)

        events = []
        async for chunk, event, ttfb in tts.get("test", "req-2"):
            events.append(event)

        assert EVENT_TTS_RESPONSE in events
        assert events[-1] == EVENT_TTS_REQUEST_END

    @pytest.mark.asyncio
    async def test_get_first_chunk_has_ttfb(self, mock_oci) -> None:
        from oracle_tts import OracleTTS, EVENT_TTS_RESPONSE

        _, client_instance = mock_oci
        config = _make_config()
        ten_env = MagicMock()
        tts = OracleTTS(config=config, ten_env=ten_env)

        pcm_data = b"\x01\x02" * 100

        response_mock = MagicMock()
        response_mock.data.content = pcm_data
        tts.client.synthesize_speech.return_value = response_mock
        tts._strip_wav_header = staticmethod(lambda x: pcm_data)

        ttfb_values = []
        async for chunk, event, ttfb in tts.get("hello", "req-3"):
            if event == EVENT_TTS_RESPONSE:
                ttfb_values.append(ttfb)

        assert ttfb_values[0] is not None
        assert isinstance(ttfb_values[0], int)
        assert ttfb_values[0] >= 0
        for subsequent in ttfb_values[1:]:
            assert subsequent is None


class TestOracleTTSRegionFallback:
    def test_empty_region_falls_back_to_default(self, mock_oci) -> None:
        from oracle_tts import OracleTTS

        config = _make_config(region="")
        ten_env = MagicMock()
        oci_mock, _ = mock_oci

        tts = OracleTTS(config=config, ten_env=ten_env)

        call_args = oci_mock.config.validate_config.call_args[0][0]
        assert call_args["region"] == "us-phoenix-1"

    def test_explicit_region_used(self, mock_oci) -> None:
        from oracle_tts import OracleTTS

        config = _make_config(region="eu-frankfurt-1")
        ten_env = MagicMock()
        oci_mock, _ = mock_oci

        tts = OracleTTS(config=config, ten_env=ten_env)

        call_args = oci_mock.config.validate_config.call_args[0][0]
        assert call_args["region"] == "eu-frankfurt-1"
