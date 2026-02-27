import json

import pytest

from config import OracleASRConfig


class TestOracleASRConfigSerialization:
    def test_to_json_is_valid_json_with_masking(self) -> None:
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

    def test_to_json_without_masking_preserves_values(self) -> None:
        cfg = OracleASRConfig(
            params={
                "tenancy": "ocid1.tenancy.oc1..abc",
                "user": "ocid1.user.oc1..def",
                "language": "ja",
            }
        )
        dumped = cfg.to_json(sensitive_handling=False)
        parsed = json.loads(dumped)

        assert parsed["params"]["tenancy"] == "ocid1.tenancy.oc1..abc"
        assert parsed["params"]["user"] == "ocid1.user.oc1..def"

    def test_to_json_includes_dump_fields(self) -> None:
        cfg = OracleASRConfig(dump=True, dump_path="/custom/path")
        parsed = json.loads(cfg.to_json())

        assert parsed["dump"] is True
        assert parsed["dump_path"] == "/custom/path"

    def test_to_json_empty_params_no_error(self) -> None:
        cfg = OracleASRConfig(params={})
        parsed = json.loads(cfg.to_json(sensitive_handling=True))
        assert parsed["params"] == {}

    def test_default_values(self) -> None:
        cfg = OracleASRConfig()
        assert cfg.dump is False
        assert cfg.dump_path == "/tmp"
        assert cfg.params == {}


class TestOracleASRConfigNormalizedLanguage:
    EXPECTED_MAPPINGS = {
        "zh": "zh-CN",
        "en": "en-US",
        "ja": "ja-JP",
        "ko": "ko-KR",
        "de": "de-DE",
        "fr": "fr-FR",
        "es": "es-ES",
        "pt": "pt-BR",
        "it": "it-IT",
        "hi": "hi-IN",
        "ar": "ar-AE",
    }

    @pytest.mark.parametrize(
        "short,expected",
        list(EXPECTED_MAPPINGS.items()),
        ids=list(EXPECTED_MAPPINGS.keys()),
    )
    def test_short_code_mapped(self, short: str, expected: str) -> None:
        cfg = OracleASRConfig(params={"language": short})
        assert cfg.normalized_language == expected

    def test_full_locale_passthrough(self) -> None:
        cfg = OracleASRConfig(params={"language": "en-US"})
        assert cfg.normalized_language == "en-US"

    def test_unknown_language_passthrough(self) -> None:
        cfg = OracleASRConfig(params={"language": "sv-SE"})
        assert cfg.normalized_language == "sv-SE"

    def test_empty_language_returns_empty(self) -> None:
        cfg = OracleASRConfig(params={"language": ""})
        assert cfg.normalized_language == ""

    def test_missing_language_returns_empty(self) -> None:
        cfg = OracleASRConfig(params={})
        assert cfg.normalized_language == ""


class TestOracleASRConfigUpdate:
    def test_update_sets_known_attributes(self) -> None:
        cfg = OracleASRConfig(dump=False, dump_path="/tmp")
        cfg.update({"dump": True, "dump_path": "/custom"})
        assert cfg.dump is True
        assert cfg.dump_path == "/custom"

    def test_update_ignores_unknown_attributes(self) -> None:
        cfg = OracleASRConfig()
        cfg.update({"nonexistent_field": "value"})
        assert not hasattr(cfg, "nonexistent_field")

    def test_update_does_not_overwrite_params(self) -> None:
        """update() only sets attributes that exist on the model;
        params is a dict so it does exist, but updating it replaces
        the entire dict."""
        original_params = {"language": "en"}
        cfg = OracleASRConfig(params=original_params)
        new_params = {"language": "zh", "region": "us-phoenix-1"}
        cfg.update({"params": new_params})
        assert cfg.params == new_params
