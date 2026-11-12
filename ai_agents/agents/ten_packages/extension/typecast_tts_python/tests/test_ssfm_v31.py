from typecast.models import TTSRequestStream

from typecast_tts_python.config import TypecastTTSConfig


def test_explicit_v31_model_and_smart_prompt():
    config = TypecastTTSConfig(
        params={
            "api_key": "test",
            "voice_id": "tc_test",
            "model": "ssfm-v31",
            "prompt": {"emotion_type": "smart", "previous_text": "Before"},
        }
    )
    config.update_params()
    config.validate()
    payload = dict(config.params)
    payload.pop("api_key")
    request = TTSRequestStream.model_validate({**payload, "text": "Hello"})
    assert request.model.value == "ssfm-v31"
    assert request.prompt.emotion_type == "smart"
    assert request.prompt.previous_text == "Before"
