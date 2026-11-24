import json
from pathlib import Path

import pytest
from openai_gpt_image_python.config import OpenAIGPTImageConfig

CURRENT_IMAGE_MODELS = (
    "gpt-image-2.6-sunburst",
    "gpt-image-2.6-flare",
)


def test_config_defaults_and_validation():
    cfg_json = json.dumps(
        {
            "params": {
                "api_key": "sk-test",
                "model": "gpt-image-2.6-sunburst",
                "size": "1024x1024",
                "quality": "standard",
                "fallback_model": "gpt-image-2.6-flare",
            }
        }
    )
    cfg = OpenAIGPTImageConfig.model_validate_json(cfg_json)
    # update_params may normalize values if needed
    cfg.update_params()
    cfg.validate()
    assert cfg.params["model"] in CURRENT_IMAGE_MODELS
    assert cfg.params["fallback_model"] in CURRENT_IMAGE_MODELS
    assert cfg.params["quality"] in ["standard", "hd"]


def test_default_models_match_supported_models():
    property_path = Path(__file__).parent.parent / "property.json"
    params = json.loads(property_path.read_text())["params"]

    assert params["model"] == "gpt-image-2.6-sunburst"
    assert params["fallback_model"] == "gpt-image-2.6-flare"


@pytest.mark.parametrize("model", ["gpt-image-1", "gpt-image-1.5", "dall-e-3"])
def test_retired_models_are_rejected(model):
    config = OpenAIGPTImageConfig(params={"api_key": "sk-test", "model": model})

    with pytest.raises(ValueError, match="Invalid model"):
        config.validate()
