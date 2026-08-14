from ..config import SarvamASRConfig
from ..extension import SarvamASRExtension


def test_vendor_metadata_from_params_model():
    ext = SarvamASRExtension("test")
    ext.config = SarvamASRConfig.model_validate(
        {"params": {"model": "saaras:v2.5"}}
    )

    assert ext.vendor_metadata() == {"model": "saaras:v2.5"}


def test_vendor_metadata_without_config():
    ext = SarvamASRExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}
