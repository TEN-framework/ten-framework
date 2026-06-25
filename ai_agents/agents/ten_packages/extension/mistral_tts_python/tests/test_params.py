import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
# The project root is 6 levels up from the parent directory of this file.
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from unittest.mock import patch, MagicMock, AsyncMock


# ================ test config defaults ================
def test_update_params_defaults():
    """update_params() sets the Mistral defaults and forces response_format=pcm."""
    from mistral_tts_python.config import MistralTTSConfig

    config = MistralTTSConfig(params={"api_key": "test_key"})
    config.update_params()

    assert config.params["model"] == "voxtral-mini-tts-2603"
    # Always raw float32 `pcm` regardless of what the caller passed.
    assert config.params["response_format"] == "pcm"
    # We do not inject a voice default — voice is optional for Voxtral.
    assert "voice" not in config.params
    # Default endpoint is the Mistral OpenAI-compatible speech endpoint.
    assert config.url == "https://api.mistral.ai/v1/audio/speech"
    print("✅ Defaults test passed.")


def test_response_format_is_forced_to_pcm():
    """Even if the caller asks for wav/mp3, we override to pcm."""
    from mistral_tts_python.config import MistralTTSConfig

    config = MistralTTSConfig(params={"api_key": "k", "response_format": "wav"})
    config.update_params()
    assert config.params["response_format"] == "pcm"
    print("✅ response_format override test passed.")


# ================ test endpoint URL resolution ================
@patch("mistral_tts_python.mistral_tts.AsyncClient")
@patch("mistral_tts_python.mistral_tts.Timeout")
@patch("mistral_tts_python.mistral_tts.Limits")
def test_url_and_base_url_configuration(
    MockLimits, MockTimeout, MockAsyncClient
):
    """The endpoint URL is resolved from `url` (top-level) or `base_url` (params)."""
    from mistral_tts_python.mistral_tts import MistralTTSClient
    from mistral_tts_python.config import MistralTTSConfig
    from ten_runtime import AsyncTenEnv

    MockTimeout.return_value = MagicMock()
    MockLimits.return_value = MagicMock()
    mock_client = AsyncMock()
    mock_client.aclose = AsyncMock()
    MockAsyncClient.return_value = mock_client

    mock_ten_env = MagicMock(spec=AsyncTenEnv)
    for attr in ("log_info", "log_debug", "log_error", "log_warn"):
        setattr(mock_ten_env, attr, MagicMock())

    common_params = {"api_key": "test_key", "model": "voxtral-mini-tts-2603"}

    # Case 1: explicit top-level url wins.
    cfg = MistralTTSConfig(
        url="https://custom-server.com/v1/tts", params=dict(common_params)
    )
    cfg.update_params()
    client = MistralTTSClient(cfg, mock_ten_env)
    assert client.config.url == "https://custom-server.com/v1/tts"

    # Case 2: base_url (trailing slash) -> {base_url}/audio/speech.
    cfg = MistralTTSConfig(
        params={**common_params, "base_url": "https://api.custom.com/v1/"}
    )
    cfg.update_params()
    assert cfg.url == "https://api.custom.com/v1/audio/speech"

    # Case 3: base_url (no trailing slash).
    cfg = MistralTTSConfig(
        params={**common_params, "base_url": "https://api.custom.com/v1"}
    )
    cfg.update_params()
    assert cfg.url == "https://api.custom.com/v1/audio/speech"

    # Case 4: default Mistral endpoint.
    cfg = MistralTTSConfig(params=dict(common_params))
    cfg.update_params()
    assert cfg.url == "https://api.mistral.ai/v1/audio/speech"

    # Case 5: url in params is extracted and removed.
    cfg = MistralTTSConfig(
        params={**common_params, "url": "https://params-url.com/tts"}
    )
    cfg.update_params()
    assert cfg.url == "https://params-url.com/tts"
    assert "url" not in cfg.params
    print("✅ URL/base_url configuration test passed.")


# ================ test headers ================
@patch("mistral_tts_python.mistral_tts.AsyncClient")
@patch("mistral_tts_python.mistral_tts.Timeout")
@patch("mistral_tts_python.mistral_tts.Limits")
def test_headers_configuration(MockLimits, MockTimeout, MockAsyncClient):
    """Default Authorization/Content-Type headers, with user override/merge."""
    from mistral_tts_python.mistral_tts import MistralTTSClient
    from mistral_tts_python.config import MistralTTSConfig
    from ten_runtime import AsyncTenEnv

    MockTimeout.return_value = MagicMock()
    MockLimits.return_value = MagicMock()
    mock_client = AsyncMock()
    mock_client.aclose = AsyncMock()
    MockAsyncClient.return_value = mock_client

    mock_ten_env = MagicMock(spec=AsyncTenEnv)
    for attr in ("log_info", "log_debug", "log_error", "log_warn"):
        setattr(mock_ten_env, attr, MagicMock())

    common_params = {
        "api_key": "test_api_key_123",
        "model": "voxtral-mini-tts-2603",
    }

    # Defaults.
    cfg = MistralTTSConfig(params=dict(common_params))
    cfg.update_params()
    client = MistralTTSClient(cfg, mock_ten_env)
    assert client.headers["Authorization"] == "Bearer test_api_key_123"
    assert client.headers["Content-Type"] == "application/json"
    assert len(client.headers) == 2

    # Merge custom header.
    cfg = MistralTTSConfig(
        headers={"X-Custom-Header": "custom-value"}, params=dict(common_params)
    )
    cfg.update_params()
    client = MistralTTSClient(cfg, mock_ten_env)
    assert client.headers["Authorization"] == "Bearer test_api_key_123"
    assert client.headers["X-Custom-Header"] == "custom-value"
    assert len(client.headers) == 3

    # User override of Authorization.
    cfg = MistralTTSConfig(
        headers={"Authorization": "Bearer custom_token"},
        params=dict(common_params),
    )
    cfg.update_params()
    client = MistralTTSClient(cfg, mock_ten_env)
    assert client.headers["Authorization"] == "Bearer custom_token"
    print("✅ Headers configuration test passed.")


# ================ test validate ================
def test_validate():
    """validate() requires api_key (or Authorization header) and model; voice optional."""
    from mistral_tts_python.config import MistralTTSConfig

    # api_key in params -> ok.
    cfg = MistralTTSConfig(
        params={"api_key": "k", "model": "voxtral-mini-tts-2603"}
    )
    cfg.update_params()
    cfg.validate()

    # Authorization header instead of api_key -> ok.
    cfg = MistralTTSConfig(
        params={"model": "voxtral-mini-tts-2603"},
        headers={"Authorization": "Bearer t"},
    )
    cfg.update_params()
    cfg.validate()

    # Neither -> error.
    cfg = MistralTTSConfig(params={"model": "voxtral-mini-tts-2603"})
    cfg.update_params()
    try:
        cfg.validate()
        assert False, "expected ValueError when no auth provided"
    except ValueError as e:
        assert "API key or Authorization header is required" in str(e)

    # Voice is NOT required.
    cfg = MistralTTSConfig(params={"api_key": "k"})
    cfg.update_params()
    cfg.validate()  # should not raise
    print("✅ Validate test passed.")
