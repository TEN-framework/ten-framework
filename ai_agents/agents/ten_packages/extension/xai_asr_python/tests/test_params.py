import asyncio
from unittest.mock import AsyncMock, MagicMock

from xai_asr_python.config import XAIASRConfig
from xai_asr_python.extension import XAIASRExtension
from ten_ai_base.utils import encrypt


def test_invalid_sample_rate():
    config = XAIASRConfig(params={"api_key": "xai-test-key", "sample_rate": 1})
    config.apply_defaults()
    try:
        config.validate_config()
    except ValueError as exc:
        assert "Unsupported sample_rate" in str(exc)
    else:
        raise AssertionError("Expected invalid sample rate error")


def test_invalid_encoding():
    config = XAIASRConfig(params={"api_key": "xai-test-key", "encoding": "mp3"})
    config.apply_defaults()
    try:
        config.validate_config()
    except ValueError as exc:
        assert "Unsupported encoding" in str(exc)
    else:
        raise AssertionError("Expected invalid encoding error")


def test_config_redacts_api_key():
    config = XAIASRConfig(
        params={"api_key": "xai-super-secret", "language": "en"}
    )
    config.apply_defaults()

    safe_str = config.to_json(sensitive_handling=True)

    assert "xai-super-secret" not in safe_str
    assert "en" in safe_str
    assert "api_key" in safe_str
    assert encrypt("xai-super-secret") in safe_str


def test_language_normalization():
    config = XAIASRConfig(params={"api_key": "xai-test-key", "language": "zh"})
    config.apply_defaults()
    assert config.normalized_language == "zh-CN"


def test_start_connection_missing_api_key_emits_fatal_error():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.config = XAIASRConfig(params={"api_key": ""})
        extension.send_asr_error = AsyncMock()

        await extension.start_connection()

        error = extension.send_asr_error.await_args.args[0]
        assert error.code == -1000
        assert "API key" in error.message

    asyncio.run(_run())
