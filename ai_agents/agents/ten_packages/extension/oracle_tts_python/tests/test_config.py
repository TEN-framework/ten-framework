import json

import pytest

from config import OracleTTSConfig


class TestOracleTTSConfigValidation:
    def test_validate_params_requires_all_oci_fields(self) -> None:
        cfg = OracleTTSConfig(params={})
        with pytest.raises(ValueError, match="Missing required OCI parameters"):
            cfg.validate_params()

    def test_validate_params_reports_specific_missing_fields(self) -> None:
        cfg = OracleTTSConfig(
            params={"tenancy": "t", "user": "u"}
        )
        with pytest.raises(ValueError, match="fingerprint"):
            cfg.validate_params()

    def test_validate_params_passes_with_all_fields(self) -> None:
        cfg = OracleTTSConfig(
            params={
                "tenancy": "t",
                "user": "u",
                "fingerprint": "f",
                "key_file": "/k",
                "compartment_id": "c",
            }
        )
        cfg.validate_params()

    def test_validate_params_empty_string_treated_as_missing(self) -> None:
        cfg = OracleTTSConfig(
            params={
                "tenancy": "",
                "user": "u",
                "fingerprint": "f",
                "key_file": "/k",
                "compartment_id": "c",
            }
        )
        with pytest.raises(ValueError, match="tenancy"):
            cfg.validate_params()


class TestOracleTTSConfigSerialization:
    def test_to_json_is_valid_json_with_masking(self) -> None:
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
        assert parsed["params"]["key_file"] != "/tmp/private.pem"

    def test_to_json_without_masking_preserves_values(self) -> None:
        cfg = OracleTTSConfig(
            params={
                "tenancy": "ocid1.tenancy.oc1..abc",
                "user": "ocid1.user.oc1..def",
                "voice_id": "Annabelle",
            }
        )
        dumped = cfg.to_json(sensitive_handling=False)
        parsed = json.loads(dumped)

        assert parsed["params"]["tenancy"] == "ocid1.tenancy.oc1..abc"
        assert parsed["params"]["user"] == "ocid1.user.oc1..def"
        assert parsed["params"]["voice_id"] == "Annabelle"

    def test_to_json_includes_dump_fields(self) -> None:
        cfg = OracleTTSConfig(dump=True, dump_path="/custom")
        parsed = json.loads(cfg.to_json())

        assert parsed["dump"] is True
        assert parsed["dump_path"] == "/custom"

    def test_to_json_empty_params_no_error(self) -> None:
        cfg = OracleTTSConfig(params={})
        parsed = json.loads(cfg.to_json(sensitive_handling=True))
        assert parsed["params"] == {}

    def test_default_values(self) -> None:
        cfg = OracleTTSConfig()
        assert cfg.dump is False
        assert cfg.dump_path == "/tmp"
        assert cfg.params == {}
