import pytest
from pydantic import ValidationError

from iflytek_asr_python.config import IFlytekAsrConfig


def test_config_adds_language_to_engine_parameters() -> None:
    config = IFlytekAsrConfig(
        url="ws://127.0.0.1:9990/tuling/ast/v3",
        biz_id="tenant-1",
        language="zh|en",
        engine={"wfep_param_nOnlineSpkdia_on": "2"},
    )

    assert config.engine_parameters() == {
        "wfep_param_nOnlineSpkdia_on": "2",
        "wrec_param_language_name": "zh|en",
    }


@pytest.mark.parametrize(
    ("configured", "vendor", "output"),
    [
        ("zh", "zh", "zh-CN"),
        ("zh-CN", "zh", "zh-CN"),
        ("en", "en", "en-US"),
        ("en-US", "en", "en-US"),
        ("zh-CN|en-US", "zh|en", "zh-CN|en-US"),
    ],
)
def test_config_maps_standard_and_vendor_language_codes(
    configured: str, vendor: str, output: str
) -> None:
    config = IFlytekAsrConfig(
        url="ws://127.0.0.1:9990/tuling/ast/v3",
        biz_id="tenant-1",
        language=configured,
    )

    assert config.vendor_language() == vendor
    assert config.output_language() == output


def test_config_log_summary_redacts_identifiers_and_url_credentials() -> None:
    config = IFlytekAsrConfig(
        url="wss://user:password@example.com/tuling/ast/v3?token=secret",
        app_id="sensitive-app-id",
        biz_id="sensitive-business-id",
        engine={"private_vendor_option": "sensitive-engine-value"},
        hotwords="confidential phrase",
        voiceprints={"person": "secret-voiceprint"},
    )

    summary = config.log_summary()
    serialized = str(summary)

    assert summary["url"] == "wss://example.com/tuling/ast/v3"
    assert summary["has_app_id"] is True
    assert summary["has_biz_id"] is True
    assert "sensitive" not in serialized
    assert "confidential phrase" not in serialized
    assert "secret-voiceprint" not in serialized


def test_config_sanitizes_credentials_and_control_characters_in_errors() -> (
    None
):
    config = IFlytekAsrConfig(
        url="wss://user:password@example.com/tuling/ast/v3?token=secret",
        app_id="sensitive-app-id",
        biz_id="sensitive-business-id",
        hotwords="confidential phrase",
        voiceprints={"person": "secret-voiceprint"},
    )

    sanitized = config.sanitize_message(
        "connection to "
        f"{config.url} failed for {config.app_id} / {config.biz_id}\n"
        f"hotwords={config.hotwords} voiceprint=secret-voiceprint"
    )

    assert sanitized.startswith(
        "connection to wss://example.com/tuling/ast/v3 failed"
    )
    assert "user" not in sanitized
    assert "password" not in sanitized
    assert "token" not in sanitized
    assert "secret" not in sanitized
    assert "sensitive" not in sanitized
    assert "confidential phrase" not in sanitized
    assert "\n" not in sanitized


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("url", "https://example.com/tuling/ast/v3"),
        ("url", "ws://"),
        ("hotword_weight", 0.9),
        ("hotword_weight", 4.1),
        ("sample_rate", 0),
        ("reconnect_max_attempts", 0),
        ("buffer_max_bytes", 0),
    ],
)
def test_config_rejects_invalid_values(field: str, value: object) -> None:
    properties = {
        "url": "ws://127.0.0.1:9990/tuling/ast/v3",
        "biz_id": "tenant-1",
        field: value,
    }

    with pytest.raises(ValidationError):
        IFlytekAsrConfig.model_validate(properties)
