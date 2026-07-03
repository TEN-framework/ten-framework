import json
from openai_gpt_image_python.config import OpenAIGPTImageConfig


def test_config_defaults_and_validation():
    cfg_json = json.dumps(
        {
            "params": {
                "api_key": "sk-test",
                "model": "gpt-image-1.5",
                "size": "1024x1024",
                "quality": "standard",
                "fallback_model": "dall-e-3",
            }
        }
    )
    cfg = OpenAIGPTImageConfig.model_validate_json(cfg_json)
    # update_params may normalize values if needed
    cfg.update_params()
    cfg.validate()
    assert cfg.params["model"] in ["gpt-image-1.5", "dall-e-3"]
    assert cfg.params["quality"] in ["standard", "hd"]
