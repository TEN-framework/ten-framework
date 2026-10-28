"""Test URL configuration and headers generation for Bytedance TTS."""

import asyncio
import sys
import os
from unittest.mock import AsyncMock, MagicMock, patch
import pytest

# Add project path to sys.path
extension_dir = os.path.join(os.path.dirname(__file__), "..")
sys.path.insert(0, extension_dir)

# Create a mock package structure to allow relative imports
import types
bytedance_tts_duplex_package = types.ModuleType('bytedance_tts_duplex')
bytedance_tts_duplex_package.__path__ = [extension_dir]
sys.modules['bytedance_tts_duplex'] = bytedance_tts_duplex_package

# Now we can import normally
from bytedance_tts_duplex import config as config_module
sys.modules['bytedance_tts_duplex.config'] = config_module

from bytedance_tts_duplex import bytedance_tts as bytedance_tts_module
sys.modules['bytedance_tts_duplex.bytedance_tts'] = bytedance_tts_module

BytedanceTTSDuplexConfig = config_module.BytedanceTTSDuplexConfig
BytedanceV3Synthesizer = bytedance_tts_module.BytedanceV3Synthesizer


@pytest.fixture
def mock_ten_env():
    """Create a mock TenEnv for testing."""
    env = MagicMock()
    env.log_debug = MagicMock()
    env.log_info = MagicMock()
    env.log_warn = MagicMock()
    env.log_error = MagicMock()
    return env


@pytest.fixture
def base_config():
    """Base configuration."""
    return BytedanceTTSDuplexConfig(
        app_id="test_app_id",
        token="test_token",
        speaker="test_speaker",
        sample_rate=16000,
    )


def test_url_override_in_update_params(base_config):
    """Test that config.url overrides api_url in update_params."""
    base_config.url = "wss://custom.bytedance.com/api/v3/tts"
    base_config.update_params()
    assert base_config.api_url == "wss://custom.bytedance.com/api/v3/tts"


def test_to_str_masks_sensitive_headers():
    config = BytedanceTTSDuplexConfig(
        speaker="test_speaker",
        headers={
            "X-Api-App-Key": "header_app_id",
            "X-Api-Access-Key": "header_token",
            "X-Api-Key": "header_api_key",
        },
    )
    rendered = config.to_str(sensitive_handling=True)
    assert "header_app_id" not in rendered
    assert "header_token" not in rendered
    assert "header_api_key" not in rendered


@pytest.mark.asyncio
async def test_get_headers_with_app_id_token(mock_ten_env, base_config):
    """Test that headers are correctly generated when app_id and token are provided."""
    # We need to mock _process_websocket in __init__ because it starts a task
    with patch.object(BytedanceV3Synthesizer, '_process_websocket', return_value=None):
        synthesizer = BytedanceV3Synthesizer(
            config=base_config,
            ten_env=mock_ten_env,
            vendor="bytedance_duplex",
            response_msgs=asyncio.Queue(),
        )
        
        headers = synthesizer.get_headers()
        
        assert headers["X-Api-App-Key"] == "test_app_id"
        assert headers["X-Api-Access-Key"] == "test_token"
        assert "X-Api-Connect-Id" in headers
        assert "X-Tt-Logid" in headers


@pytest.mark.asyncio
async def test_get_headers_with_api_key(mock_ten_env):
    """Test that headers are correctly generated when only api_key is provided."""
    config = BytedanceTTSDuplexConfig(
        api_key="test_api_key",
        speaker="test_speaker",
    )
    
    with patch.object(BytedanceV3Synthesizer, '_process_websocket', return_value=None):
        synthesizer = BytedanceV3Synthesizer(
            config=config,
            ten_env=mock_ten_env,
            vendor="bytedance_duplex",
            response_msgs=asyncio.Queue(),
        )
        
        headers = synthesizer.get_headers()
        
        assert headers["X-Api-Key"] == "test_api_key"
        assert "X-Api-App-Key" not in headers


@pytest.mark.asyncio
async def test_get_headers_merges_custom_headers(mock_ten_env, base_config):
    """Test that custom headers from config are merged."""
    base_config.headers = {"X-Custom-Header": "custom_value"}
    
    with patch.object(BytedanceV3Synthesizer, '_process_websocket', return_value=None):
        synthesizer = BytedanceV3Synthesizer(
            config=base_config,
            ten_env=mock_ten_env,
            vendor="bytedance_duplex",
            response_msgs=asyncio.Queue(),
        )
        
        headers = synthesizer.get_headers()
        
        assert headers["X-Custom-Header"] == "custom_value"
        assert headers["X-Api-App-Key"] == "test_app_id"


@pytest.mark.asyncio
async def test_explicit_x_api_key_header_takes_priority(mock_ten_env, base_config):
    base_config.headers = {"X-Api-Key": "header_api_key"}

    with patch.object(BytedanceV3Synthesizer, '_process_websocket', return_value=None):
        synthesizer = BytedanceV3Synthesizer(
            config=base_config,
            ten_env=mock_ten_env,
            vendor="bytedance_duplex",
            response_msgs=asyncio.Queue(),
        )

        headers = synthesizer.get_headers()

        assert headers["X-Api-Key"] == "header_api_key"
        assert "X-Api-App-Key" not in headers
        assert "X-Api-Access-Key" not in headers
