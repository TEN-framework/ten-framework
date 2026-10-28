"""Mock tests for update_configs functionality."""

import asyncio
from unittest.mock import MagicMock, AsyncMock, patch
import pytest
import sys
import os

# Add project path
extension_dir = os.path.join(os.path.dirname(__file__), "..")
sys.path.insert(0, extension_dir)

# Create mock package structure
import types
bytedance_tts_duplex_package = types.ModuleType('bytedance_tts_duplex')
bytedance_tts_duplex_package.__path__ = [extension_dir]
sys.modules['bytedance_tts_duplex'] = bytedance_tts_duplex_package

from bytedance_tts_duplex import config as config_module
sys.modules['bytedance_tts_duplex.config'] = config_module

from bytedance_tts_duplex import extension as extension_module
sys.modules['bytedance_tts_duplex.extension'] = extension_module

from bytedance_tts_duplex.config import BytedanceTTSDuplexConfig
from bytedance_tts_duplex.extension import BytedanceTTSDuplexExtension


@pytest.fixture
def mock_ten_env():
    """Create mock TenEnv."""
    env = AsyncMock()
    env.log_debug = MagicMock()
    env.log_info = MagicMock()
    env.log_warn = MagicMock()
    env.log_error = MagicMock()
    env.get_property_to_json = AsyncMock(return_value=('{}', None))
    return env


@pytest.fixture
def mock_extension(mock_ten_env):
    """Create mock extension with initialized config."""
    ext = BytedanceTTSDuplexExtension("test_extension")
    ext.ten_env = mock_ten_env

    # Initialize config
    ext.config = BytedanceTTSDuplexConfig(
        app_id="app1",
        token="token1",
        speaker="speaker1",
        sample_rate=16000,
        params={"audio_params": {"sample_rate": 16000}}
    )

    # Mock client
    ext.client = MagicMock()
    ext.client.update_config = MagicMock()
    ext.client.cancel = AsyncMock()
    ext.client.reset_synthesizer = MagicMock()

    ext.current_request_id = None

    return ext


def test_vendor_metadata_does_not_use_x_api_access_key_as_authorization(
    mock_extension,
):
    metadata = mock_extension.vendor_metadata()

    assert "authorization" not in metadata
    assert "key" not in metadata
    assert metadata["token"] == "token1"


def test_vendor_metadata_does_not_use_x_api_key_as_authorization(mock_extension):
    mock_extension.config.app_id = ""
    mock_extension.config.token = ""
    mock_extension.config.api_key = "api-key-1"

    metadata = mock_extension.vendor_metadata()

    assert "authorization" not in metadata
    assert "key" not in metadata
    assert metadata["api_key"] == "api-key-1"


def test_vendor_metadata_does_not_use_config_x_api_key_header_as_authorization(
    mock_extension,
):
    mock_extension.config.headers = {"X-Api-Key": "header-api-key"}

    metadata = mock_extension.vendor_metadata()

    assert "authorization" not in metadata
    assert "key" not in metadata


def test_vendor_metadata_adds_explicit_config_authorization_header(mock_extension):
    mock_extension.config.headers = {"Authorization": "Bearer header-key"}

    metadata = mock_extension.vendor_metadata()

    assert metadata["authorization"] == "Bearer header-key"
    assert "key" not in metadata


def test_vendor_metadata_adds_explicit_lowercase_authorization_header(
    mock_extension,
):
    mock_extension.config.headers = {"authorization": "Bearer lower-header-key"}

    metadata = mock_extension.vendor_metadata()

    assert metadata["authorization"] == "Bearer lower-header-key"
    assert "key" not in metadata


@pytest.mark.asyncio
async def test_update_configs_buffers_update(mock_extension):
    """Test that update_configs buffers update in pending_config."""
    updates = {"speaker": "speaker2", "dump": True}

    await mock_extension.update_configs(updates)

    # Original config unchanged
    assert mock_extension.config.speaker == "speaker1"
    assert mock_extension.config.dump == False

    # Update buffered in pending_config
    assert mock_extension.pending_config is not None
    assert mock_extension.pending_config.speaker == "speaker2"
    assert mock_extension.pending_config.dump == True


@pytest.mark.asyncio
async def test_update_configs_multiple_merges(mock_extension):
    """Test that multiple update_configs calls merge into pending_config."""
    await mock_extension.update_configs({"speaker": "speaker2"})
    await mock_extension.update_configs({"dump": True})

    assert mock_extension.pending_config.speaker == "speaker2"
    assert mock_extension.pending_config.dump == True


@pytest.mark.asyncio
async def test_request_tts_applies_pending_config(mock_extension):
    """Test that request_tts applies pending_config for a new request."""
    from ten_ai_base.struct import TTSTextInput
    
    # 1. Buffer an update
    await mock_extension.update_configs({"speaker": "speaker2"})
    
    # 2. Receive a new request
    t = TTSTextInput(request_id="new_req", text="hello", text_input_end=False)
    
    # We need to mock some methods called in request_tts to avoid errors
    mock_extension.send_tts_audio_end = AsyncMock()
    mock_extension.send_usage_metrics = AsyncMock()
    mock_extension.send_tts_request_metrics = AsyncMock()
    mock_extension.finish_request = AsyncMock()
    mock_extension.client.send_text = AsyncMock()

    await mock_extension.request_tts(t)

    # Config should be updated
    assert mock_extension.config.speaker == "speaker2"
    
    # Client updated and cancelled (for reconnection)
    mock_extension.client.update_config.assert_called_with(mock_extension.config)
    mock_extension.client.cancel.assert_called_once()
    
    # pending_config cleared
    assert mock_extension.pending_config is None
    request_metrics = mock_extension.send_tts_request_metrics.await_args.kwargs
    assert request_metrics["request_id"] == "new_req"
    assert request_metrics["request_text"] == "hello"
    assert request_metrics["request_time_ms"] > 0


@pytest.mark.asyncio
async def test_update_configs_validation_error(mock_extension):
    """Config updates no longer fail fast on missing required params."""
    updates = {"speaker": ""}

    await mock_extension.update_configs(updates)

    assert mock_extension.pending_config is not None
    assert mock_extension.pending_config.speaker == ""
