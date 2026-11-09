#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Unit tests for property resolution and validation (design D3).

``resolve_config`` takes the two top-level properties the extension reads
(``channel`` and the ``params`` object) and returns the typed settings a
session needs, or raises ``ValueError`` with a message that names the
offending key. The glue turns that message into the ``-1012`` ModuleError.
"""

from dataclasses import asdict, fields

import pytest

from spatialreal_avatar_python.config import ResolvedConfig, resolve_config
from spatialreal_avatar_python.session_client import SessionSettings

# Test literals, not credentials.
APP_ID = "a" * 32


def params(**overrides) -> dict:
    """A complete, valid params object; tests override one key at a time."""
    base = {
        "spatialreal_api_key": "k",
        "spatialreal_avatar_id": "av",
        "agora_uid": "12345",
        "agora_appid": APP_ID,
    }
    base.update(overrides)
    return base


def test_minimal_valid_config_resolves_with_defaults():
    cfg = resolve_config("room", params())
    assert isinstance(cfg, ResolvedConfig)
    assert cfg.api_key == "k"
    assert cfg.avatar_id == "av"
    assert cfg.channel == "room"
    assert cfg.uid == 12345
    assert cfg.agora_appid == APP_ID
    assert cfg.agora_appcert == ""
    assert cfg.agora_token == ""
    assert cfg.app_id == ""
    assert cfg.environment == "us-west"
    assert cfg.console_endpoint_url == ""
    assert cfg.ingress_endpoint_url == ""
    assert cfg.sample_rate == 24000
    assert cfg.egress_idle_reconnect_ms == 1_080_000
    assert cfg.source_timeline_only is True


@pytest.mark.parametrize("enabled", [False, True])
def test_source_timeline_only_boolean_passes_through(enabled):
    cfg = resolve_config("room", params(source_timeline_only=enabled))
    assert cfg.source_timeline_only is enabled


@pytest.mark.parametrize("invalid", [None, 0, 1, "true", "false", {}, []])
def test_source_timeline_only_rejects_non_boolean(invalid):
    with pytest.raises(ValueError, match="params.source_timeline_only"):
        resolve_config("room", params(source_timeline_only=invalid))


def test_source_profile_defaults_burst_and_resolves_legacy_override():
    assert resolve_config("room", params()).source_playback_profile == 1
    assert (
        resolve_config(
            "room", params(source_timeline_only=False)
        ).source_playback_profile
        == 0
    )
    cfg = resolve_config(
        "room", params(source_timeline_only=True, source_playback_profile=1)
    )
    assert cfg.source_playback_profile == 1
    assert SessionSettings(**asdict(cfg)).source_playback_profile == 1


@pytest.mark.parametrize("invalid", [True, False, None, "1", 1.0, -1, 2, 999])
def test_source_profile_rejects_invalid_values(invalid):
    with pytest.raises(ValueError, match="params.source_playback_profile"):
        resolve_config(
            "room",
            params(source_timeline_only=True, source_playback_profile=invalid),
        )


def test_source_profile_requires_source_only():
    with pytest.raises(ValueError, match="source_timeline_only"):
        resolve_config(
            "room",
            params(source_timeline_only=False, source_playback_profile=1),
        )


def test_agora_channel_in_params_overrides_channel():
    cfg = resolve_config("injected", params(agora_channel="explicit"))
    assert cfg.channel == "explicit"


def test_uid_string_and_int_parse():
    # Hosted graphs carry the uid as a string, self-hosted graphs as an int
    # (Review Focus 5); both must resolve to the same integer.
    assert resolve_config("room", params(agora_uid="12345")).uid == 12345
    assert resolve_config("room", params(agora_uid=12345)).uid == 12345
    assert resolve_config("room", params(agora_uid=" 7 ")).uid == 7


@pytest.mark.parametrize("bad", ["abc", "", "0", 0, "1.5", None, "-0"])
def test_uid_invalid_is_config_error(bad):
    with pytest.raises(ValueError) as exc_info:
        resolve_config("room", params(agora_uid=bad))
    assert "agora_uid" in str(exc_info.value)


def test_channel_precedence_and_hint_message():
    # Top-level channel is used when params.agora_channel is empty ...
    assert resolve_config("room", params()).channel == "room"
    assert resolve_config("room", params(agora_channel="")).channel == "room"
    # ... and when both are empty the message tells the operator how the
    # TEN server injects the channel, and names the explicit alternative.
    with pytest.raises(ValueError) as exc_info:
        resolve_config("", params())
    message = str(exc_info.value)
    assert '"channel": ""' in message
    assert "agora_channel" in message


@pytest.mark.parametrize(
    "rate", [8000, 16000, 22050, 24000, 32000, 44100, 48000, "16000"]
)
def test_sample_rate_accepted_values(rate):
    assert resolve_config("room", params(sample_rate=rate)).sample_rate == int(
        rate
    )


@pytest.mark.parametrize("rate", [11025, 0, -1, "fast", "1.5", True])
def test_sample_rate_must_be_accepted(rate):
    with pytest.raises(ValueError) as exc_info:
        resolve_config("room", params(sample_rate=rate))
    assert "sample_rate" in str(exc_info.value)


def test_appid_required_unless_token():
    with pytest.raises(ValueError) as exc_info:
        resolve_config("room", params(agora_appid=""))
    message = str(exc_info.value)
    assert "agora_appid" in message
    assert "agora_token" in message
    # An explicit token makes the App ID optional.
    cfg = resolve_config("room", params(agora_appid="", agora_token="tok"))
    assert cfg.agora_token == "tok"
    assert cfg.agora_appid == ""


def test_missing_api_key_message_names_key():
    with pytest.raises(ValueError) as exc_info:
        resolve_config("room", params(spatialreal_api_key=""))
    assert "spatialreal_api_key" in str(exc_info.value)
    with pytest.raises(ValueError) as exc_info:
        resolve_config(
            "room", {k: v for k, v in params().items() if "key" not in k}
        )
    assert "spatialreal_api_key" in str(exc_info.value)


def test_missing_avatar_id_message_names_key():
    with pytest.raises(ValueError) as exc_info:
        resolve_config("room", params(spatialreal_avatar_id=""))
    assert "spatialreal_avatar_id" in str(exc_info.value)


def test_optional_strings_and_idle_ms_pass_through():
    cfg = resolve_config(
        "room",
        params(
            spatialreal_app_id="app",
            agora_appcert="cert",
            environment="test",
            console_endpoint_url="https://console.example",
            ingress_endpoint_url="wss://ingress.example/v2",
            egress_idle_reconnect_ms="600000",
        ),
    )
    assert cfg.app_id == "app"
    assert cfg.agora_appcert == "cert"
    assert cfg.environment == "test"
    assert cfg.console_endpoint_url == "https://console.example"
    assert cfg.ingress_endpoint_url == "wss://ingress.example/v2"
    assert cfg.egress_idle_reconnect_ms == 600_000


@pytest.mark.parametrize("bad", ["soon", "1.5", False])
def test_idle_ms_invalid_is_config_error(bad):
    with pytest.raises(ValueError) as exc_info:
        resolve_config("room", params(egress_idle_reconnect_ms=bad))
    assert "egress_idle_reconnect_ms" in str(exc_info.value)


@pytest.mark.parametrize("empty", ["", None])
def test_integer_knobs_fall_back_to_defaults_when_unset(empty):
    # A JSON null or an empty string where an integer belongs is "not set":
    # the package defaults apply rather than a config error.
    cfg = resolve_config(
        "room", params(sample_rate=empty, egress_idle_reconnect_ms=empty)
    )
    assert cfg.sample_rate == 24000
    assert cfg.egress_idle_reconnect_ms == 1_080_000


def test_non_string_values_are_rejected_not_coerced():
    # A nested object where a string belongs is a graph authoring error,
    # not something to str() silently.
    with pytest.raises(ValueError) as exc_info:
        resolve_config("room", params(spatialreal_api_key={"k": 1}))
    assert "spatialreal_api_key" in str(exc_info.value)


def test_resolved_config_maps_onto_session_settings_field_for_field():
    # The glue builds ``SessionSettings(**asdict(cfg))``: the two dataclasses
    # must keep the same field names or a connect loses a setting silently.
    assert [f.name for f in fields(ResolvedConfig)] == [
        f.name for f in fields(SessionSettings)
    ]
    cfg = resolve_config("room", params(agora_token="t", environment="dev"))
    assert asdict(SessionSettings(**asdict(cfg))) == asdict(cfg)
