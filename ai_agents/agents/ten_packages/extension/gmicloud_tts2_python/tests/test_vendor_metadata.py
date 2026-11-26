from gmicloud_tts2_python.client import GmicloudTTSClient
from tests.helpers import make_opts


def test_extra_metadata_has_no_secrets():
    client = GmicloudTTSClient(make_opts())
    metadata = client.get_extra_metadata()
    assert metadata["model"] == "inworld-tts-2"
    assert "api_key" not in metadata
    assert "test-key-value" not in str(metadata)
    assert set(metadata.keys()) <= {"model", "voice"}
