#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#

import pytest
from unittest.mock import AsyncMock, MagicMock, patch
from whisper_stt_python.extension import WhisperSTTExtension
from whisper_stt_python.config import WhisperSTTConfig


@pytest.fixture
def extension():
    """Create extension instance"""
    return WhisperSTTExtension("test_whisper")


@pytest.fixture
def mock_ten_env():
    """Create mock TenEnv"""
    env = AsyncMock()
    env.log_info = MagicMock()
    env.log_error = MagicMock()
    env.log_debug = MagicMock()
    env.log_warn = MagicMock()
    env.get_property_to_json = AsyncMock(return_value=('{"params": {}}', None))
    return env


def test_extension_initialization(extension):
    """Test extension initialization"""
    assert extension.vendor() == "whisper"
    assert extension.config is None
    assert extension.client is None
    assert extension.audio_dumper is None


def test_vendor_name(extension):
    """Test vendor name"""
    assert extension.vendor() == "whisper"


@pytest.mark.asyncio
async def test_on_init_success(extension, mock_ten_env):
    """Test successful initialization"""
    config_json = """{
        "dump": false,
        "params": {
            "model": "base",
            "device": "cpu",
            "language": "en"
        }
    }"""
    mock_ten_env.get_property_to_json = AsyncMock(
        return_value=(config_json, None)
    )

    extension.ten_env = mock_ten_env
    await extension.on_init(mock_ten_env)

    assert extension.config is not None
    assert extension.config.params["model"] == "base"
    assert extension.reconnect_manager is not None


@pytest.mark.asyncio
async def test_on_init_invalid_config(extension, mock_ten_env):
    """Test initialization with invalid config"""
    mock_ten_env.get_property_to_json = AsyncMock(
        return_value=("invalid json", None)
    )

    extension.ten_env = mock_ten_env
    extension.send_asr_error = AsyncMock()

    await extension.on_init(mock_ten_env)

    assert extension.send_asr_error.called


def test_input_audio_sample_rate(extension):
    """Test input audio sample rate"""
    extension.config = WhisperSTTConfig(params={"sample_rate": 16000})
    assert extension.input_audio_sample_rate() == 16000

    extension.config = WhisperSTTConfig(params={})
    assert extension.input_audio_sample_rate() == 16000


def test_buffer_strategy(extension):
    """Test buffer strategy returns Keep mode"""
    strategy = extension.buffer_strategy()
    assert strategy is not None


def test_is_connected_no_client(extension):
    """Test is_connected when no client"""
    assert extension.is_connected() is False


def test_is_connected_with_client(extension):
    """Test is_connected with client"""
    mock_client = MagicMock()
    mock_client.is_connected = MagicMock(return_value=True)
    extension.client = mock_client

    assert extension.is_connected() is True


@pytest.mark.asyncio
async def test_start_connection_success(extension, mock_ten_env):
    """Test successful connection start"""
    extension.config = WhisperSTTConfig(
        params={
            "model": "base",
            "device": "cpu",
            "compute_type": "int8",
            "language": "en",
        }
    )
    extension.ten_env = mock_ten_env
    extension.reconnect_manager = MagicMock()
    extension.reconnect_manager.mark_connection_successful = MagicMock()
    extension.audio_timeline = MagicMock()
    extension.audio_timeline.get_total_user_audio_duration = MagicMock(
        return_value=0
    )
    extension.audio_timeline.reset = MagicMock()

    with patch(
        "whisper_stt_python.extension.WhisperClient"
    ) as mock_client_class:
        mock_client = AsyncMock()
        mock_client.connect = AsyncMock()
        mock_client_class.return_value = mock_client

        await extension.start_connection()

        assert extension.client is not None
        assert mock_client.connect.called


@pytest.mark.asyncio
async def test_stop_connection(extension, mock_ten_env):
    """Test connection stop"""
    extension.ten_env = mock_ten_env
    mock_client = AsyncMock()
    mock_client.disconnect = AsyncMock()
    extension.client = mock_client

    await extension.stop_connection()

    assert mock_client.disconnect.called
    assert extension.client is None


@pytest.mark.asyncio
async def test_send_audio_no_client(extension):
    """Test send_audio when not connected"""
    mock_frame = MagicMock()
    result = await extension.send_audio(mock_frame, None)
    assert result is False


@pytest.mark.asyncio
async def test_send_audio_success(extension):
    """Test successful audio sending"""
    mock_client = AsyncMock()
    mock_client.is_connected = MagicMock(return_value=True)
    mock_client.send_audio = AsyncMock()
    extension.client = mock_client

    mock_frame = MagicMock()
    mock_buf = b"audio_data"
    mock_frame.lock_buf = MagicMock(return_value=mock_buf)
    mock_frame.unlock_buf = MagicMock()

    result = await extension.send_audio(mock_frame, None)

    assert result is True
    assert mock_client.send_audio.called
    assert mock_frame.unlock_buf.called


@pytest.mark.asyncio
async def test_finalize_disconnect_mode(extension, mock_ten_env):
    """Test finalize in disconnect mode"""
    extension.config = WhisperSTTConfig(finalize_mode="disconnect")
    extension.ten_env = mock_ten_env
    mock_client = AsyncMock()
    mock_client.finalize = AsyncMock()
    extension.client = mock_client

    await extension.finalize(None)

    assert mock_client.finalize.called


@pytest.mark.asyncio
async def test_finalize_silence_mode(extension, mock_ten_env):
    """Test finalize in silence mode"""
    extension.config = WhisperSTTConfig(finalize_mode="silence")
    extension.ten_env = mock_ten_env
    mock_client = AsyncMock()
    mock_client.finalize = AsyncMock()
    extension.client = mock_client

    await extension.finalize(None)

    assert mock_client.finalize.called


@pytest.mark.asyncio
async def test_on_result_callback(extension, mock_ten_env):
    """Test result callback handling"""
    extension.config = WhisperSTTConfig(params={"language": "en"})
    extension.ten_env = mock_ten_env
    extension.audio_timeline = MagicMock()
    extension.audio_timeline.get_audio_duration_before_time = MagicMock(
        return_value=0
    )
    extension.send_asr_result = AsyncMock()
    extension._finalize_end = AsyncMock()

    await extension._on_result(
        text="Hello world",
        start_ms=0,
        duration_ms=1000,
        language="en-US",
        final=True,
    )

    assert extension.send_asr_result.called
    assert extension._finalize_end.called


@pytest.mark.asyncio
async def test_on_error_callback(extension, mock_ten_env):
    """Test error callback handling"""
    extension.ten_env = mock_ten_env
    extension.send_asr_error = AsyncMock()
    extension._handle_reconnect = AsyncMock()

    await extension._on_error("Test error")

    assert extension.send_asr_error.called
    assert extension._handle_reconnect.called
