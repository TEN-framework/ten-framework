from ..config import DeepgramASRConfig
from ..extension import DeepgramASRExtension


def test_vendor_metadata_prefers_api_key():
    ext = DeepgramASRExtension("test")
    ext.config = DeepgramASRConfig.model_validate(
        {
            "params": {
                "api_key": "api-secret",
                "key": "legacy-key",
                "url": "wss://api.deepgram.com/v1/listen",
                "model": "nova-3",
            }
        }
    )

    assert ext.vendor_metadata() == {
        "key": "api-secret",
        "url": "wss://api.deepgram.com/v1/listen",
        "model": "nova-3",
    }


def test_vendor_metadata_without_config():
    ext = DeepgramASRExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}
