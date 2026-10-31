import json
from pathlib import Path

import pytest

from qwen_asr_python.config import MODEL_NAME, QwenASRConfig


def make_config(**params):
    config = QwenASRConfig(params={"api_key": "secret", **params})
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
        ({"api_key": "secret", "format": "wav"}, "pcm"),
        ({"api_key": "secret", "max_sentence_silence": 100}, "between"),
        ({"api_key": "secret", "language_hints": ["xx"]}, "unsupported"),
    ],
)
def test_invalid_configuration_is_rejected(params, message):
    config = QwenASRConfig(params=params)
    config.apply_defaults()
    with pytest.raises(ValueError, match=message):
        config.validate_config()


def test_sensitive_config_is_redacted():
    config = make_config()
    redacted = json.loads(config.to_json(sensitive_handling=True))
    assert "secret" not in json.dumps(redacted)


def test_guarder_invalid_credentials_pass_local_config_validation():
    fixture = Path(__file__).parent / "configs" / "property_invalid.json"
    config = QwenASRConfig.model_validate_json(fixture.read_text())
    config.apply_defaults()
    config.validate_config()
    assert config.api_key == "invalid"
    assert config.url.startswith("wss://")
