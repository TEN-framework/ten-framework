import json

from config import OracleASRConfig


def test_to_json_is_valid_json_with_masking() -> None:
    cfg = OracleASRConfig(
        params={
            "tenancy": "ocid1.tenancy.oc1..secret",
            "user": "ocid1.user.oc1..secret",
            "fingerprint": "aa:bb:cc",
            "key_file": "/tmp/private.pem",
            "language": "en",
        }
    )

    dumped = cfg.to_json(sensitive_handling=True)
    parsed = json.loads(dumped)

    assert parsed["params"]["language"] == "en"
    assert parsed["params"]["tenancy"] != "ocid1.tenancy.oc1..secret"
    assert parsed["params"]["key_file"] != "/tmp/private.pem"


def test_normalized_language_mapping() -> None:
    cfg = OracleASRConfig(params={"language": "zh"})
    assert cfg.normalized_language == "zh-CN"

    cfg_unknown = OracleASRConfig(params={"language": "en-US"})
    assert cfg_unknown.normalized_language == "en-US"
