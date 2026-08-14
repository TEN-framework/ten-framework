from ..config import OpenAIASRConfig
from ..extension import OpenAIASRExtension


def test_vendor_metadata_from_input_audio_transcription_model():
    ext = OpenAIASRExtension("test")
    ext.config = OpenAIASRConfig.model_validate(
        {
            "params": {
                "api_key": "secret-key",
                "input_audio_transcription": {
                    "model": "gpt-4o-mini-transcribe"
                },
            }
        }
    )

    assert ext.vendor_metadata() == {"model": "gpt-4o-mini-transcribe"}


def test_vendor_metadata_without_config():
    ext = OpenAIASRExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}
