from ..config import AzureASRConfig
from ..extension import AzureASRExtension


def test_vendor_metadata_from_config():
    ext = AzureASRExtension("test")
    ext.config = AzureASRConfig.model_validate(
        {
            "key": "secret-key",
            "region": "eastus",
        }
    )

    metadata = ext.vendor_metadata()

    assert metadata == {
        "key": "secret-key",
        "region": "eastus",
    }


def test_vendor_metadata_without_config():
    ext = AzureASRExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}
