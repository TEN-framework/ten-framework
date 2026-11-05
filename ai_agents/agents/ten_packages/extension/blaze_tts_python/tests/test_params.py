#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from unittest.mock import MagicMock

import pytest

from blaze_tts_python.blaze_tts import BlazeTTSClient
from blaze_tts_python.config import BlazeTTSConfig


def make_config(**params) -> BlazeTTSConfig:
    config = BlazeTTSConfig(
        params={
            "base_url": "https://api.blaze.vn/",
            "api_key": "test-key",
            **params,
        }
    )
    config.update_params()
    return config


def test_defaults_are_realtime():
    config = make_config()
    assert config.model == "2.0-realtime"
    assert config.speaker_id == "HN-Nu-CSKH-HuongGiang"
    assert config.normalization == "basic"
    assert config.ws_url == "wss://api.blaze.vn/v1/tts/realtime"


def test_update_params_moves_known_keys():
    config = make_config(
        speaker_id="HCM-Nar-Nu-SaleThuyVy", sample_rate=16000, style="news"
    )
    assert config.speaker_id == "HCM-Nar-Nu-SaleThuyVy"
    assert config.sample_rate == 16000
    assert config.params == {"style": "news"}


def test_empty_env_values_keep_defaults():
    config = make_config(base_url="", speaker_id="")
    assert config.base_url == "https://api.blaze.vn"
    assert config.speaker_id == "HN-Nu-CSKH-HuongGiang"


def test_ws_url_for_plain_http():
    assert (
        make_config(base_url="http://localhost:9000").ws_url
        == "ws://localhost:9000/v1/tts/realtime"
    )


def test_secret_is_redacted():
    assert "test-key" not in make_config().to_str(sensitive_handling=True)


def test_query_includes_passthrough_params_without_secrets():
    client = BlazeTTSClient(
        config=make_config(style="news", audio_speed=1.2), ten_env=MagicMock()
    )
    query = client.build_query("xin chào")
    assert query["query"] == "xin chào"
    assert query["audio_format"] == "pcm"
    assert query["audio_speed"] == "1.2"
    assert query["style"] == "news"
    assert "api_key" not in query
    assert "base_url" not in query


def test_valid_config_passes():
    make_config().validate_config()


@pytest.mark.parametrize(
    "params",
    [
        {"api_key": ""},
        {"base_url": "not-a-url"},
        {"base_url": "ftp://blaze.vn"},
        {"sample_rate": 12345},
        {"audio_speed": 0},
    ],
)
def test_invalid_params_raise(params):
    with pytest.raises(ValueError):
        make_config(**params).validate_config()
