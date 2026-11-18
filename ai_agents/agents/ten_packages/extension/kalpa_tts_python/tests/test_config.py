#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import pytest
from websockets.exceptions import ConnectionClosedError
from websockets.frames import Close

from ten_packages.extension.kalpa_tts_python.config import (
    DEFAULT_MODEL,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_VOICE_ID,
    SAMPLE_RATES,
    KalpaTTSConfig,
)
from ten_packages.extension.kalpa_tts_python.extension import _split
from ten_packages.extension.kalpa_tts_python.kalpa_tts import (
    _error_from_close,
    _error_from_message,
)

KEY = "sk-live-0123456789abcdefghij"


def load(params: dict | None = None, **properties) -> KalpaTTSConfig:
    params = {"api_key": KEY, **(params or {})}
    config = KalpaTTSConfig(params=params, **properties)
    config.update_params()
    return config


def test_defaults_need_only_a_key():
    from ten_packages.extension import kalpa_tts_python

    # Importing the package is what registers the addon.
    assert kalpa_tts_python.addon.KalpaTTSExtensionAddon
    config = load()
    config.check()
    path = f"/v1/tts/{DEFAULT_VOICE_ID}/stream"
    assert config.websocket_url == "wss://api.kalpalabs.ai" + path
    assert config.session_message() == {
        "type": "initializeConnection",
        "api_key": KEY,
        "model": DEFAULT_MODEL,
        "output_format": "pcm_s16le",
        "sample_rate": DEFAULT_SAMPLE_RATE,
        "generation_config": {},
    }


def test_settings_leave_params_and_the_rest_is_passed_to_kalpa():
    generation = {"temperature": 0.7, "audio_quality": "medium"}
    config = load(
        {
            "voice_id": "voice-1",
            "model": "kalpa-tts-2",
            "sample_rate": 16000,
            "chunk_length_schedule": [50, 120],
            **generation,
            # The extension sets these itself.
            "output_format": "mp3",
            "type": "sendText",
        }
    )
    config.check()
    assert config.params == generation
    message = config.session_message()
    assert (message["model"], message["sample_rate"]) == ("kalpa-tts-2", 16000)
    assert message["output_format"] == "pcm_s16le"
    assert message["generation_config"] == {"chunk_length_schedule": [50, 120]}
    assert message["params"] == generation
    assert config.websocket_url.endswith("/v1/tts/voice-1/stream")
    # Empty values fall back to the defaults, and an empty schedule leaves
    # the choice to Kalpa.
    empty = load({"voice_id": "", "model": None, "chunk_length_schedule": []})
    empty.check()
    assert (empty.voice_id, empty.model) == (DEFAULT_VOICE_ID, DEFAULT_MODEL)
    assert empty.session_message()["generation_config"] == {}
    assert "params" not in empty.session_message()


@pytest.mark.parametrize(
    "rate",
    [True, "fast", "16000", 24000.0, 24000.5, 0, -24000, 12345, None, [24000]],
)
def test_only_the_sample_rates_kalpa_offers_are_accepted(rate):
    for accepted in SAMPLE_RATES:
        config = load({"sample_rate": accepted})
        config.check()
        assert config.session_message()["sample_rate"] == accepted
    with pytest.raises(ValueError, match="sample_rate"):
        load({"sample_rate": rate}).check()
    with pytest.raises(ValueError, match="sample_rate"):
        load(sample_rate=rate).check()


@pytest.mark.parametrize(
    "schedule",
    [
        "80",
        80,
        [50, "soon"],
        [True],
        [50.5],
        {"a": 50},
        [49],
        [2001],
        [50] * 11,
    ],
)
def test_other_settings_that_cannot_work_are_rejected(schedule):
    with pytest.raises(ValueError, match="chunk_length_schedule"):
        load({"chunk_length_schedule": schedule})
    with pytest.raises(ValueError, match="api_key"):
        load({"api_key": ""}).check()
    with pytest.raises(ValueError, match="voice_id"):
        load(voice_id="").check()


def test_endpoint_precedence():
    urls = {
        "url": "wss://nested.test/tts/{voice_id}",
        "base_url": "wss://b.test/",
    }
    nested = load({**urls, "voice_id": "v1"}, url="wss://top.test/tts")
    assert nested.websocket_url == "wss://nested.test/tts/v1"
    # A blank `params.url` is no endpoint.
    top = load({**urls, "url": " "}, url="wss://top.test/tts")
    assert top.websocket_url == "wss://top.test/tts"
    base = load({"base_url": urls["base_url"], "voice_id": "v1"})
    assert base.websocket_url == "wss://b.test/v1/tts/v1/stream"
    # The endpoint is never sent to Kalpa as a generation parameter.
    assert "params" not in nested.session_message()


@pytest.mark.parametrize(
    "setting",
    [
        {"base_url": "https://api.kalpalabs.ai"},
        {"base_url": "api.kalpalabs.ai"},
        {"base_url": "wss://"},
        {"base_url": "wss://api.kalpalabs.ai:port"},
        {"url": f"http://example.test/tts?token={KEY}"},
    ],
)
def test_an_endpoint_that_is_not_a_websocket_address_is_rejected(setting):
    with pytest.raises(ValueError, match="endpoint") as raised:
        load(setting).check()
    assert KEY not in str(raised.value)


def test_the_key_is_not_logged_or_reported():
    address = f"wss://user:{KEY}@example.test:8443/tts/{{voice_id}}"
    config = load({"token": KEY}, url=f"{address}?token={KEY}#{KEY}")
    assert config.safe_url == f"wss://example.test:8443/tts/{DEFAULT_VOICE_ID}"
    assert KEY not in config.to_str(sensitive_handling=True)
    # The connection itself still uses the address as it was given.
    assert KEY in config.websocket_url
    assert KEY in config.to_str(sensitive_handling=False)
    local = load({"base_url": "ws://[::1]:9000/"})
    assert local.safe_url == f"ws://[::1]:9000/v1/tts/{DEFAULT_VOICE_ID}/stream"


def test_long_text_is_cut_at_a_space():
    assert _split("short", 10) == ["short"]
    assert _split("aaaa bbbb cccc", 10) == ["aaaa bbbb ", "cccc"]
    assert _split("a" * 25, 10) == ["a" * 10, "a" * 10, "a" * 5]


@pytest.mark.parametrize(
    "code, error_type, opening",
    [
        (4401, "authentication_error", False),
        (4402, "allowance_exhausted", False),
        (4403, "account_suspended", False),
        (4404, "not_found", False),
        # Session settings Kalpa refuses are refused again on every retry.
        (4400, "invalid_request", True),
    ],
)
def test_failures_that_no_reconnect_can_fix(code, error_type, opening):
    closed = ConnectionClosedError(Close(code, ""), None)
    error = _error_from_close(closed, opening=opening)
    assert (error.permanent, error.error_type, error.code) == (
        True,
        error_type,
        code,
    )
    message = {"error": {"type": error_type}}
    assert _error_from_message(message, opening=opening).permanent


def test_other_failures_are_worth_a_reconnect():
    # 4400 on an open session is one refused message, not the settings.
    for code in (1011, 4429, 4400):
        closed = ConnectionClosedError(Close(code, "try again"), None)
        error = _error_from_close(closed)
        assert (error.permanent, error.error_type, error.message) == (
            False,
            "connection_closed",
            "try again",
        )
    assert not _error_from_message(
        {"error": {"type": "invalid_request"}}
    ).permanent
    failed = {"error": {"type": "inference_error"}, "response_id": "resp_1"}
    error = _error_from_message(failed)
    assert (error.permanent, error.response_id) == (False, "resp_1")
