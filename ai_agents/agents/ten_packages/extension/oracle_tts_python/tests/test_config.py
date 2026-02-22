import json

import pytest

from config import OracleTTSConfig


def test_validate_params_requires_oci_fields() -> None:
    cfg = OracleTTSConfig(params={})
    with pytest.raises(ValueError):
        cfg.validate_params()


def test_to_json_is_valid_json_with_masking() -> None:
    cfg = OracleTTSConfig(
        params={
            "tenancy": "ocid1.tenancy.oc1..secret",
            "user": "ocid1.user.oc1..secret",
            "fingerprint": "aa:bb:cc",
            "key_file": "/tmp/private.pem",
            "voice_id": "Annabelle",
        }
    )

    dumped = cfg.to_json(sensitive_handling=True)
    parsed = json.loads(dumped)

    assert parsed["params"]["voice_id"] == "Annabelle"
    assert parsed["params"]["tenancy"] != "ocid1.tenancy.oc1..secret"
