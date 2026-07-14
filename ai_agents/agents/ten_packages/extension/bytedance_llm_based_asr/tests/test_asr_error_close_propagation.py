from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from ..config import BytedanceASRLLMConfig
from ..extension import BytedanceASRLLMExtension
from .. import volcengine_asr_client as client_module
from ..volcengine_asr_client import VolcengineASRClient


def _minimal_config() -> BytedanceASRLLMConfig:
    return BytedanceASRLLMConfig.model_validate(
        {
            "params": {
                "audio": {"rate": 16000},
                "request": {"model_name": "bigmodel"},
            }
        }
    )


class _FailingWebSocket:
    def __aiter__(self):
        return self

    async def __anext__(self):
        raise RuntimeError("listen failed")


@pytest.mark.asyncio
async def test_asr_error_callback_forwards_vendor_close_fields():
    client = VolcengineASRClient(
        url="wss://example.test/asr",
        app_key="app_key",
        access_key="access_key",
        api_key="api_key",
        auth_method="api_key",
        config=_minimal_config(),
    )
    client.websocket = _FailingWebSocket()
    client.asr_error_callback = MagicMock(
        return_value=(1000, "mapped non-fatal error")
    )

    disconnected = AsyncMock()
    client.set_on_disconnected_callback(disconnected)

    await client._listen_for_responses()

    client.asr_error_callback.assert_called_once()
    disconnected.assert_awaited_once_with(
        1000,
        "closed",
        1000,
        "mapped non-fatal error",
    )


@pytest.mark.asyncio
async def test_extension_disconnected_forwards_vendor_close_fields():
    extension = BytedanceASRLLMExtension("test_extension")
    extension.ten_env = MagicMock()
    extension.on_disconnected = AsyncMock()

    await extension._on_disconnected(
        vendor_close_code=1000,
        vendor_close_message="mapped non-fatal error",
    )

    extension.on_disconnected.assert_awaited_once_with(
        code=1000,
        message="mapped non-fatal error",
        vendor_info=None,
    )


@pytest.mark.asyncio
async def test_connection_error_callback_forwards_vendor_close_fields():
    client = VolcengineASRClient(
        url="wss://example.test/asr",
        app_key="app_key",
        access_key="access_key",
        api_key="api_key",
        auth_method="api_key",
        config=_minimal_config(),
    )
    error = Exception("server rejected WebSocket connection: HTTP 401")
    client.connection_error_callback = MagicMock(
        return_value=(401, "server rejected WebSocket connection: HTTP 401")
    )
    client.disconnect = AsyncMock()

    with patch.object(
        client_module.websockets, "connect", new=AsyncMock(side_effect=error)
    ):
        with pytest.raises(Exception, match="HTTP 401"):
            await client.connect()

    client.connection_error_callback.assert_called_once_with(error)
    client.disconnect.assert_awaited_once_with(
        1000,
        "closed",
        "401",
        "server rejected WebSocket connection: HTTP 401",
    )


def test_extension_connection_error_returns_parsed_http_code():
    extension = BytedanceASRLLMExtension("test_extension")

    with patch("asyncio.create_task", side_effect=lambda coro: coro.close()):
        close_code, close_message = extension._on_connection_error(
            Exception("server rejected WebSocket connection: HTTP 401")
        )

    assert close_code == 401
    assert close_message == "server rejected WebSocket connection: HTTP 401"
