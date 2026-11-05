#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ..config import BlazeSTTConfig
from ..extension import BlazeSTTExtension


def test_vendor_metadata():
    ext = BlazeSTTExtension("test")
    ext.config = BlazeSTTConfig.model_validate(
        {
            "params": {
                "api_key": "api-secret",
                "base_url": "https://api.blaze.vn",
                "language": "vi",
            }
        }
    )

    assert ext.vendor_metadata() == {
        "key": "api-secret",
        "url": "https://api.blaze.vn",
        "language": "vi",
    }


def test_vendor_metadata_without_config():
    ext = BlazeSTTExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}


def test_config_defaults_and_language():
    config = BlazeSTTConfig.model_validate(
        {
            "dump": True,
            "params": {
                "base_url": "https://api.blaze.vn/",
                "api_key": "secret",
                "language": "vi",
            },
        }
    )
    # Same sequence as BlazeSTTExtension.on_init.
    config.apply_defaults()
    config.validate_config()

    assert config.dump is True
    assert config.api_key == "secret"
    assert "secret" not in config.to_json(sensitive_handling=True)

    assert config.params["base_url"] == "https://api.blaze.vn"
    assert config.sample_rate == 16000
    assert config.asr_result_language == "vi-VN"
