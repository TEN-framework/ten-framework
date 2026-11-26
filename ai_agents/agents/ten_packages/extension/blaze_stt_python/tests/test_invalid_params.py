#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from .. import extension as extension_module
from ..config import BlazeSTTConfig
from ..extension import BlazeSTTExtension


def _config(**params) -> BlazeSTTConfig:
    config = BlazeSTTConfig(params={"api_key": "k", **params})
    config.apply_defaults()
    return config


def test_defaults_are_valid_and_realtime():
    config = _config()
    config.validate_config()
    assert config.params["base_url"] == "https://api.blaze.vn"
    assert config.params["model"] == "stt-stream-1.5"
    assert config.ws_url() == "wss://api.blaze.vn/v1/stt/realtime"


def test_ws_url_for_plain_http():
    assert (
        _config(base_url="http://localhost:9000/").ws_url()
        == "ws://localhost:9000/v1/stt/realtime"
    )


@pytest.mark.parametrize(
    "params",
    [
        {"api_key": ""},
        {"base_url": "not-a-url"},
        {"base_url": "ftp://blaze.vn"},
        {"sample_rate": 24000},
    ],
)
def test_invalid_params_raise(params):
    with pytest.raises(ValueError):
        _config(**params).validate_config()


def test_config_json_is_valid_and_redacted():
    dumped = _config(api_key="super-secret").to_json(sensitive_handling=True)
    assert "super-secret" not in dumped
    assert json.loads(dumped)["params"]["language"] == "vi"


def test_init_message_skips_empty_options():
    assert _config(topic="", context="sales").init_message() == {
        "token": "k",
        "language": "vi",
        "model": "stt-stream-1.5",
        "context": "sales",
    }


def test_invalid_config_reports_one_fatal_error_and_never_connects():
    async def _run():
        extension = BlazeSTTExtension("blaze_stt_python")
        ten_env = MagicMock()
        ten_env.get_property_to_json = AsyncMock(
            return_value=(json.dumps({"params": {"api_key": ""}}), None)
        )
        extension.send_asr_error = AsyncMock()

        with patch(
            "ten_ai_base.asr.AsyncASRBaseExtension.on_init", new=AsyncMock()
        ):
            await extension.on_init(ten_env)

        # The base class wraps start_connection() and emits a
        # connection-status event before the plugin code runs.
        ten_env.send_data = AsyncMock()
        extension.ten_env = ten_env
        with patch.object(
            extension_module, "BlazeSTTRecognition"
        ) as recognition_cls:
            await extension.start_connection()
            recognition_cls.assert_not_called()

        extension.send_asr_error.assert_awaited_once()
        assert extension.send_asr_error.await_args.args[0].code == -1000

    asyncio.run(_run())


@pytest.mark.parametrize(
    "base_url",
    [
        "https://user:hunter2@api.blaze.vn",
        "https://api.blaze.vn?token=hunter2",
        "https://api.blaze.vn#hunter2",
    ],
)
def test_urls_with_secrets_are_rejected_without_echoing_them(base_url):
    with pytest.raises(ValueError) as raised:
        _config(base_url=base_url).validate_config()
    assert "hunter2" not in str(raised.value)
