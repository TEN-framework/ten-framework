import json
from pathlib import Path
from unittest.mock import AsyncMock, Mock

import pytest
from openai_gpt_image_python.config import OpenAIGPTImageConfig
from openai_gpt_image_python.extension import OpenAIGPTImageExtension

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


def test_doodler_graph_uses_supported_models():
    repo_ai_agents_root = Path(__file__).resolve().parents[5]
    graph_path = (
        repo_ai_agents_root
        / "agents"
        / "examples"
        / "doodler"
        / "tenapp"
        / "property.json"
    )
    graphs = json.loads(graph_path.read_text())["ten"]["predefined_graphs"]
    image_node = next(
        node
        for graph in graphs
        for node in graph["graph"]["nodes"]
        if node["addon"] == "openai_gpt_image_python"
    )
    params = image_node["property"]["params"]

    assert params["model"] in CURRENT_IMAGE_MODELS
    assert params["fallback_model"] in CURRENT_IMAGE_MODELS


@pytest.mark.parametrize("model", ["gpt-image-1", "gpt-image-1.5", "dall-e-3"])
def test_retired_models_are_rejected(model):
    config = OpenAIGPTImageConfig(params={"api_key": "sk-test", "model": model})

    with pytest.raises(ValueError, match="Invalid model"):
        config.validate()


def test_retired_fallback_model_is_rejected():
    config = OpenAIGPTImageConfig(
        params={
            "api_key": "sk-test",
            "model": "gpt-image-2.6-sunburst",
            "fallback_model": "dall-e-3",
        }
    )

    with pytest.raises(ValueError, match="Invalid fallback_model"):
        config.validate()


@pytest.mark.asyncio
async def test_send_image_logs_size_without_payload_data():
    extension = OpenAIGPTImageExtension("openai_gpt_image_python")
    ten_env = AsyncMock()
    ten_env.log_info = Mock()
    image_data = "data:image/png;base64,private-image-bytes"

    await extension.send_image(ten_env, image_data)

    logged = " ".join(
        str(call.args[0]) for call in ten_env.log_info.call_args_list
    )
    assert "private-image-bytes" not in logged
    assert f"data_url_bytes={len(image_data)}" in logged
