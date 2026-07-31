from ..config import GoogleASRConfig
from ..extension import GoogleASRExtension


def test_vendor_metadata_from_params_model():
    ext = GoogleASRExtension("test")
    ext.config = GoogleASRConfig.model_validate(
        {"params": {"model": "chirp_3"}}
    )

    assert ext.vendor_metadata() == {"model": "chirp_3"}


def test_vendor_metadata_without_config():
    ext = GoogleASRExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}
