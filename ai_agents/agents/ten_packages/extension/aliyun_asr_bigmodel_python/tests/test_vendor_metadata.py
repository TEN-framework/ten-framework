from ..config import AliyunASRBigmodelConfig
from ..extension import AliyunASRBigmodelExtension


def test_vendor_metadata_from_params_model():
    ext = AliyunASRBigmodelExtension("test")
    ext.config = AliyunASRBigmodelConfig.model_validate(
        {"params": {"model": "paraformer-realtime-v2"}}
    )

    assert ext.vendor_metadata() == {"model": "paraformer-realtime-v2"}


def test_vendor_metadata_without_config():
    ext = AliyunASRBigmodelExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}
