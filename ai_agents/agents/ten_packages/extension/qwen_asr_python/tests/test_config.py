import json
from pathlib import Path

import pytest

from qwen_asr_python.config import (
    LANGUAGE_HINT_TO_ASR_RESULT_LANGUAGE,
    MODEL_NAME,
    QwenASRConfig,
)


def make_config(language: str = "zh-CN", **params):
    config = QwenASRConfig(
        language=language,
        params={"api_key": "secret", **params},
    )
    config.apply_defaults()
    config.validate_config()
    return config


def test_recommended_request_uses_qwen_report_settings():
    config = make_config()
    request = config.build_run_task("task-1")

    assert request["header"] == {
        "action": "run-task",
        "task_id": "task-1",
        "streaming": "duplex",
    }
    assert request["payload"]["model"] == MODEL_NAME
    assert request["payload"]["parameters"]["format"] == "pcm"
    assert request["payload"]["parameters"]["sample_rate"] == 16000
    assert request["payload"]["parameters"]["vad_model"] == "near_meeting_16k"
    assert request["payload"]["parameters"]["max_sentence_silence"] == 240
    assert request["payload"]["parameters"]["language_hints"] == ["zh"]
    assert request["payload"]["input"] == {}


def test_vocabulary_and_context_are_explicit_and_mutually_exclusive():
    config = make_config(vocabulary={"TEN": 5})
    request = config.build_run_task("task-1")
    assert request["payload"]["parameters"]["vocabulary"] == {"TEN": 5}

    with pytest.raises(ValueError, match="not both"):
        invalid = QwenASRConfig(
            params={
                "api_key": "secret",
                "vocabulary": {"TEN": 5},
                "vocabulary_id": "v1",
            }
        )
        invalid.apply_defaults()
        invalid.validate_config()


def test_context_is_nested_under_input():
    context = [
        {"role": "user", "content": [{"type": "input_text", "text": "TEN"}]}
    ]
    request = make_config(context=context).build_run_task("task-1")
    assert request["payload"]["input"] == {"context": context}


@pytest.mark.parametrize(
    "params, message",
    [
        ({"api_key": ""}, "API key"),
        (
            {"api_key": "secret", "url": "wss:///asr"},
            "valid ws:// or wss:// host",
        ),
        ({"api_key": "secret", "format": "wav"}, "pcm"),
        ({"api_key": "secret", "max_sentence_silence": 100}, "between"),
    ],
)
def test_invalid_configuration_is_rejected(params, message):
    config = QwenASRConfig(params=params)
    with pytest.raises(ValueError, match=message):
        config.apply_defaults()
        config.validate_config()


def test_sensitive_config_is_redacted():
    config = make_config()
    redacted = json.loads(config.to_json(sensitive_handling=True))
    assert "secret" not in json.dumps(redacted)


def test_default_normalized_language_is_zh_cn():
    config = make_config()
    assert config.normalized_language == "zh-CN"


@pytest.mark.parametrize(
    "hint,expected",
    sorted(LANGUAGE_HINT_TO_ASR_RESULT_LANGUAGE.items()),
)
def test_language_param_iso_maps_to_bcp47(hint, expected):
    config = make_config(language=hint)  # property.language
    assert config.normalized_language == expected
    assert config.params["language_hints"] == [hint]


def test_zh_cn_maps_to_language_hint_zh():
    config = make_config(language="zh-CN")
    assert config.params["language_hints"] == ["zh"]


def test_unknown_asr_language_yields_empty_language_hints():
    config = make_config(language="zz-ZZ")
    assert config.normalized_language == "zz-ZZ"
    assert config.params["language_hints"] == []


def test_en_us_maps_to_language_hint_en():
    config = make_config(language="en-US")
    assert config.params["language_hints"] == ["en"]


def test_params_language_hints_override_property_language():
    config = make_config(language="zh-CN", language_hints=["en"])
    assert config.params["language_hints"] == ["en"]
    request = config.build_run_task("task-1")
    assert request["payload"]["parameters"]["language_hints"] == ["en"]


@pytest.mark.parametrize(
    "payload,output,sentence,expected",
    [
        ({}, {}, {"language": "en"}, "en-US"),
        ({}, {"language_hints": ["ja"]}, {}, "ja-JP"),
        ({"language": "zh-CN"}, {}, {}, "zh-CN"),
    ],
)
def test_vendor_language_maps_to_asr_result_language(
    payload, output, sentence, expected
):
    assert (
        QwenASRConfig.asr_result_language_from_vendor(payload, output, sentence)
        == expected
    )


def test_property_json_uses_top_level_language():
    fixture = Path(__file__).parent / "configs" / "property_en.json"
    config = QwenASRConfig.model_validate_json(fixture.read_text())
    config.apply_defaults()
    config.validate_config()
    assert config.language == "en-US"
    assert config.normalized_language == "en-US"
    assert config.params["language_hints"] == ["en"]
    assert "language" not in config.params


def test_sensitive_config_redacts_url_query_credentials():
    config = make_config(
        url="wss://example.test/asr?api_key=query-secret&region=us"
    )
    redacted = json.loads(config.to_json(sensitive_handling=True))
    assert "query-secret" not in json.dumps(redacted)
    assert "region=us" in redacted["params"]["url"]


def test_guarder_invalid_credentials_pass_local_config_validation():
    fixture = Path(__file__).parent / "configs" / "property_invalid.json"
    config = QwenASRConfig.model_validate_json(fixture.read_text())
    config.apply_defaults()
    config.validate_config()
    assert config.api_key == "invalid"
    assert config.url.startswith("wss://")
