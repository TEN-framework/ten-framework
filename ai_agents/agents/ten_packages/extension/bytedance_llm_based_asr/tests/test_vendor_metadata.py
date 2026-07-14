from ..config import BytedanceASRLLMConfig
from ..extension import BytedanceASRLLMExtension


def test_vendor_metadata_api_key_auth():
    ext = BytedanceASRLLMExtension("test")
    ext.config = BytedanceASRLLMConfig.model_validate(
        {
            "params": {
                "auth_method": "api_key",
                "api_key": "secret-key",
                "api_url": "wss://example.com/asr",
                "request": {"model_name": "bigmodel"},
            }
        }
    )

    assert ext.vendor_metadata() == {
        "url": "wss://example.com/asr",
        "api_key": "secret-key",
        "auth_method": "api_key",
    }


def test_vendor_metadata_without_config():
    ext = BytedanceASRLLMExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}
