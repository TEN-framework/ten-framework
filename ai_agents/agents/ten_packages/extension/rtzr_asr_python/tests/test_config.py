import pytest
from pydantic import ValidationError
from ten_packages.extension.rtzr_asr_python.config import RTZRASRConfig


def config(**params):
    return RTZRASRConfig(
        params={"client_id": "id", "client_secret": "secret", **params}
    )


@pytest.mark.parametrize(
    "model,language",
    [("sommers_ko", "ko-KR"), ("sommers_ja", "ja-JP"), ("sommers_en", "en-US")],
)
def test_model_language(model, language):
    assert config(model_name=model).language == language


def test_endpoints_and_redaction():
    assert config().params["websocket_url"] == "wss://openapi.vito.ai"
    value = config(api_base="http://localhost:9000/", use_itn=True)
    assert value.params["websocket_url"] == "ws://localhost:9000"
    assert value.query_params()["use_itn"] == "true"
    assert "client_secret" not in value.query_params()
    assert "api_base" not in value.query_params()
    assert '"secret"' not in value.to_str()
    assert (
        config(websocket_url="wss://stream.example").params["websocket_url"]
        == "wss://stream.example"
    )


@pytest.mark.parametrize(
    "params",
    [
        {"client_id": ""},
        {"client_secret": ""},
        {"sample_rate": 0},
        {"sample_rate": True},
        {"sample_rate": "16000"},
        {"model_name": "unknown"},
        {"encoding": "OPUS"},
        {"api_base": "ftp://example.com"},
        {"api_base": "http://user:secret@example.com"},
        {"websocket_url": "https://example.com"},
    ],
)
def test_invalid_config(params):
    with pytest.raises(ValidationError):
        config(**params)


def test_comparison_normalization():
    from .test_live import character_error_rate, normalized

    assert normalized("Ｈｅｌｌｏ, WORLD!") == "helloworld"
    assert character_error_rate("abc", "axc") == pytest.approx(1 / 3)
    assert character_error_rate("", "") == 0
