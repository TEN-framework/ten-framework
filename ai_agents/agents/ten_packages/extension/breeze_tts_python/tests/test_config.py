import json

from breeze_tts import BreezeTTSParams
from config import BreezeTTSConfig


def test_params_defaults():
    params = BreezeTTSParams(api_key="brz_test")
    assert params.base_url == "https://api.breeze.blue"
    assert params.voice_id == "voc_u3nxkd5578wc"
    assert params.sample_rate == 24000
    assert params.output_format == "pcm"


def test_config_from_dict():
    config = BreezeTTSConfig.model_validate(
        {
            "dump": False,
            "params": {
                "api_key": "brz_test",
                "voice_id": "voc_abc",
                "language": "zh",
                "speed": 0.9,
            },
        }
    )
    assert config.params.voice_id == "voc_abc"
    assert config.params.speed == 0.9
