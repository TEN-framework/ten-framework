from ..config import SonioxASRConfig
from ..extension import SonioxASRExtension


def test_vendor_metadata_from_config():
    ext = SonioxASRExtension("test")
    ext.config = SonioxASRConfig.model_validate(
        {
            "url": "wss://example.com/asr",
            "params": {"api_key": "secret-key", "model": "stt-rt-preview"},
        }
    )

    metadata = ext.vendor_metadata()

    assert metadata == {
        "api_key": "secret-key",
        "url": "wss://example.com/asr",
        "model": "stt-rt-preview",
    }


def test_vendor_metadata_without_config():
    ext = SonioxASRExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}
