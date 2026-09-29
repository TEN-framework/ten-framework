#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Unit tests for AssemblyAIASRConfig (pure Python, no runtime needed)."""

import pytest

from ..config import AssemblyAIASRConfig


def _config(**params) -> AssemblyAIASRConfig:
    config = AssemblyAIASRConfig.model_validate({"params": params})
    config.update(config.params)
    return config


def test_default_model_is_universal_3_6_pro():
    config = _config(api_key="k")

    assert config.speech_model == "universal-3-6-pro"
    assert config.is_pro_model is True


def test_explicit_speech_model_overrides_default():
    assert _config(speech_model="universal-3-5-pro").speech_model == (
        "universal-3-5-pro"
    )


def test_pro_family_detection_covers_future_and_legacy_pro_models():
    assert _config(speech_model="universal-3-5-pro").is_pro_model is True
    assert _config(speech_model="universal-3-7-pro").is_pro_model is True
    assert _config(speech_model="u3-rt-pro").is_pro_model is True
    assert _config(speech_model="u3-rt-pro-beta-1").is_pro_model is True
    assert (
        _config(speech_model="universal-streaming-english").is_pro_model
        is False
    )
    assert (
        _config(speech_model="universal-streaming-multilingual").is_pro_model
        is False
    )


def test_update_sets_known_fields_and_keeps_unknown_as_extra_params():
    config = _config(
        api_key="k",
        mode="min_latency",
        prompt="Customer support call.",
        future_knob="on",
    )

    assert config.api_key == "k"
    assert config.mode == "min_latency"
    assert config.prompt == "Customer support call."
    assert config.extra_params == {"future_knob": "on"}


def test_default_connection_params_are_minimal():
    params = _config(api_key="k").to_connection_params()

    assert params == {
        "speech_model": "universal-3-6-pro",
        "sample_rate": 16000,
        "encoding": "pcm_s16le",
        "format_turns": True,
        "language_codes": ["en"],
    }


def test_language_codes_derived_from_language_when_unset():
    assert _config(language="zh-CN").to_connection_params()[
        "language_codes"
    ] == ["zh"]
    assert _config(language="es").to_connection_params()["language_codes"] == [
        "es"
    ]


def test_explicit_language_codes_win_and_empty_list_disables_steering():
    explicit = _config(language="en-US", language_codes=["en", "es"])
    assert explicit.to_connection_params()["language_codes"] == ["en", "es"]

    disabled = _config(language="en-US", language_codes=[])
    assert "language_codes" not in disabled.to_connection_params()


def test_pro_only_params_are_sent_for_pro_models():
    params = _config(
        mode="max_accuracy",
        prompt="Medical dictation.",
        keyterms_prompt=["AssemblyAI"],
        agent_context="What is your email address?",
        previous_context_n_turns=8,
        language_detection=True,
        voice_focus="near-field",
        voice_focus_threshold=0.8,
        min_turn_silence=200,
        max_turn_silence=1200,
        vad_threshold=0.3,
        interruption_delay=100,
        continuous_partials=False,
        include_partial_turns=True,
    ).to_connection_params()

    assert params["mode"] == "max_accuracy"
    assert params["prompt"] == "Medical dictation."
    assert params["keyterms_prompt"] == ["AssemblyAI"]
    assert params["agent_context"] == "What is your email address?"
    assert params["previous_context_n_turns"] == 8
    assert params["language_detection"] is True
    assert params["voice_focus"] == "near-field"
    assert params["voice_focus_threshold"] == 0.8
    assert params["min_turn_silence"] == 200
    assert params["max_turn_silence"] == 1200
    assert params["vad_threshold"] == 0.3
    assert params["interruption_delay"] == 100
    assert params["continuous_partials"] is False
    assert params["include_partial_turns"] is True


def test_pro_only_params_are_dropped_for_universal_streaming_models():
    params = _config(
        speech_model="universal-streaming-english",
        mode="min_latency",
        prompt="ignored",
        agent_context="ignored",
        previous_context_n_turns=3,
        language_codes=["en"],
        voice_focus="near-field",
        interruption_delay=100,
        continuous_partials=True,
        keyterms_prompt=["kept"],
        min_turn_silence=200,
    ).to_connection_params()

    for dropped in (
        "mode",
        "prompt",
        "agent_context",
        "previous_context_n_turns",
        "language_codes",
        "voice_focus",
        "interruption_delay",
        "continuous_partials",
    ):
        assert dropped not in params, dropped
    assert params["keyterms_prompt"] == ["kept"]
    assert params["min_turn_silence"] == 200


def test_legacy_turn_params_only_sent_to_universal_streaming_models():
    legacy = dict(
        end_of_turn_confidence_threshold=0.4,
        min_end_of_turn_silence_when_confident=160,
    )

    streaming = _config(
        speech_model="universal-streaming-english", **legacy
    ).to_connection_params()
    assert streaming["end_of_turn_confidence_threshold"] == 0.4
    assert streaming["min_end_of_turn_silence_when_confident"] == 160

    pro = _config(**legacy).to_connection_params()
    assert "end_of_turn_confidence_threshold" not in pro
    assert "min_end_of_turn_silence_when_confident" not in pro


def test_empty_keyterms_and_none_values_are_omitted():
    params = _config(keyterms_prompt=[], prompt=None).to_connection_params()

    assert "keyterms_prompt" not in params
    assert "prompt" not in params


def test_unknown_params_are_forwarded_verbatim():
    params = _config(future_knob="on", another=3).to_connection_params()

    assert params["future_knob"] == "on"
    assert params["another"] == 3


def test_connection_params_never_include_secrets_or_local_settings():
    params = _config(
        api_key="secret", ws_url="wss://x", dump=True, dump_path="/tmp"
    ).to_connection_params()

    for key in ("api_key", "ws_url", "dump", "dump_path", "params", "language"):
        assert key not in params


def test_language_for_code_maps_iso_to_locale_with_fallback():
    config = _config(language="en-US")

    assert config.language_for_code("es") == "es-ES"
    assert config.language_for_code("zh") == "zh-CN"
    assert config.language_for_code("xx") == "xx"
    assert config.language_for_code(None) == "en-US"


def test_to_json_redacts_api_key():
    config = _config(api_key="super-secret-key")

    dumped = config.to_json(sensitive_handling=True)

    assert "super-secret-key" not in dumped
    assert "universal-3-6-pro" in dumped


def test_language_codes_not_derived_when_language_detection_enabled():
    params = _config(language="en-US", language_detection=True)

    assert "language_codes" not in params.to_connection_params()


def test_update_coerces_scalar_types_from_property_overrides():
    config = _config(
        sample_rate="16000", format_turns="false", vad_threshold="0.3"
    )

    assert config.sample_rate == 16000
    assert config.format_turns is False
    assert config.vad_threshold == 0.3


def test_validate_config_accepts_a_full_valid_config():
    _config(
        api_key="k",
        mode="min_latency",
        encoding="pcm_mulaw",
        voice_focus="far-field",
        vad_threshold=0.5,
        previous_context_n_turns=10,
        language_codes=["en", "es"],
        redact_pii_sub="hash",
    ).validate_config()


@pytest.mark.parametrize(
    "bad",
    [
        {"api_key": ""},
        {"ws_url": "https://streaming.assemblyai.com/v3/ws"},
        {"sample_rate": 0},
        {"encoding": "mp3"},
        {"mode": "turbo"},
        {"voice_focus": "mid-field"},
        {"redact_pii_sub": "stars"},
        {"vad_threshold": 1.5},
        {"voice_focus_threshold": -0.1},
        {"interruption_delay": 5000},
        {"previous_context_n_turns": 101},
        {"prompt": "x" * 1751},
        {"agent_context": "x" * 1751},
        {"language_codes": [f"l{i}" for i in range(11)]},
        {"keyterms_prompt": [f"t{i}" for i in range(101)]},
        {"finalize_timeout_ms": 0},
    ],
)
def test_validate_config_rejects_invalid_values(bad):
    config = _config(**{"api_key": "k", **bad})

    with pytest.raises(ValueError):
        config.validate_config()


def test_dropped_connection_params_lists_gated_out_keys():
    pro = _config(end_of_turn_confidence_threshold=0.4, mode="balanced")
    assert pro.dropped_connection_params() == [
        "end_of_turn_confidence_threshold"
    ]

    streaming = _config(
        speech_model="universal-streaming-english", mode="balanced", prompt="p"
    )
    assert streaming.dropped_connection_params() == ["mode", "prompt"]


def test_finalize_timeout_is_local_and_not_sent_to_vendor():
    config = _config(finalize_timeout_ms=1500)

    assert config.finalize_timeout_ms == 1500
    assert "finalize_timeout_ms" not in config.to_connection_params()
