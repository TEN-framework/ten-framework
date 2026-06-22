#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import pytest

from generic_video_python.config import GenericVideoConfig


def test_config_accepts_legacy_aliases():
    config = GenericVideoConfig.model_validate(
        {
            "agora_appid": "appid",
            "agora_channel_name": "room-a",
            "agora_video_uid": 42,
            "generic_video_api_key": "secret",
            "avatar_id": "avatar",
            "start_endpoint": "https://example.test/start",
            "stop_endpoint": "https://example.test/stop",
        }
    )

    assert config.channel == "room-a"
    assert config.agora_avatar_uid == 42


def test_config_masks_sensitive_fields():
    config = GenericVideoConfig.model_validate(
        {
            "agora_appid": "appid",
            "channel": "room-a",
            "generic_video_api_key": "secret",
            "avatar_id": "avatar",
            "start_endpoint": "https://example.test/start",
            "stop_endpoint": "https://example.test/stop",
        }
    )

    config_str = config.to_str(sensitive_handling=True)

    assert "secret" not in config_str
    assert "******" in config_str or "*" in config_str


def test_config_normalizes_known_params_and_keeps_vendor_passthrough():
    config = GenericVideoConfig.model_validate(
        {
            "agora_appid": "appid",
            "channel": "room-a",
            "generic_video_api_key": "secret",
            "avatar_id": "avatar",
            "start_endpoint": "https://example.test/start",
            "stop_endpoint": "https://example.test/stop",
            "params": {
                "api_key": "secret-2",
                "agora_channel_name": "room-b",
                "agora_video_uid": 77,
                "area": "JAPAN",
                "model": "vendor-model-1",
                "style": "cinematic",
            },
        }
    )

    config.normalize_params()

    assert config.generic_video_api_key == "secret-2"
    assert config.channel == "room-b"
    assert config.agora_avatar_uid == 77
    assert config.area == "JAPAN"
    assert config.vendor_params == {
        "model": "vendor-model-1",
        "style": "cinematic",
    }


@pytest.mark.parametrize("quality", ["bad", "HIGH", ""])
def test_config_rejects_invalid_quality(quality: str):
    with pytest.raises(Exception):
        GenericVideoConfig.model_validate(
            {
                "agora_appid": "appid",
                "channel": "room-a",
                "generic_video_api_key": "secret",
                "avatar_id": "avatar",
                "start_endpoint": "https://example.test/start",
                "stop_endpoint": "https://example.test/stop",
                "quality": quality,
            }
        )


def test_config_rejects_invalid_area():
    with pytest.raises(Exception):
        GenericVideoConfig.model_validate(
            {
                "agora_appid": "appid",
                "channel": "room-a",
                "generic_video_api_key": "secret",
                "avatar_id": "avatar",
                "start_endpoint": "https://example.test/start",
                "stop_endpoint": "https://example.test/stop",
                "area": "MARS",
            }
        )
