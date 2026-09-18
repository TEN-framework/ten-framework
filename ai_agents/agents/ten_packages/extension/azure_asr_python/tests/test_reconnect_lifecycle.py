import asyncio
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock

from ..config import AzureASRConfig
from ..extension import AzureASRExtension


def make_extension(
    transport_reconnect_grace_sec: float = 10,
) -> AzureASRExtension:
    extension = AzureASRExtension("azure_asr_python")
    extension.ten_env = MagicMock()
    extension.config = AzureASRConfig(
        transport_reconnect_grace_sec=transport_reconnect_grace_sec
    )
    extension.reconnect_manager = MagicMock()
    extension.on_disconnected = AsyncMock()  # type: ignore[method-assign]
    extension.on_connected = AsyncMock()  # type: ignore[method-assign]
    extension.send_connect_delay_metrics = AsyncMock()  # type: ignore[method-assign]
    extension.stop_connection = AsyncMock()  # type: ignore[method-assign]
    extension._handle_reconnect = AsyncMock()  # type: ignore[method-assign]
    extension._recognizer_epoch = 1
    return extension


def test_transport_recovery_runs_after_grace_when_sdk_does_not_reconnect():
    async def run_test() -> None:
        extension = make_extension(transport_reconnect_grace_sec=0.05)
        evt = SimpleNamespace(session_id="session-1")

        await extension._azure_event_handler_on_disconnected(evt, 1)
        await asyncio.sleep(0.1)

        extension.stop_connection.assert_awaited_once()  # type: ignore[attr-defined]
        extension._handle_reconnect.assert_awaited_once()  # type: ignore[attr-defined]

    asyncio.run(run_test())


def test_transport_recovery_cancelled_when_sdk_reconnects():
    async def run_test() -> None:
        extension = make_extension(transport_reconnect_grace_sec=0.2)
        disconnect_evt = SimpleNamespace(session_id="session-1")
        connect_evt = SimpleNamespace(session_id="session-1")

        await extension._azure_event_handler_on_disconnected(disconnect_evt, 1)
        await extension._azure_event_handler_on_connected(connect_evt, 1)
        await asyncio.sleep(0.3)

        extension.stop_connection.assert_not_awaited()  # type: ignore[attr-defined]
        extension._handle_reconnect.assert_not_awaited()  # type: ignore[attr-defined]

    asyncio.run(run_test())


def test_transport_reconnect_grace_sec_from_params():
    config = AzureASRConfig()
    config.update({"transport_reconnect_grace_sec": 15})
    assert config.transport_reconnect_grace_sec == 15


def test_stale_session_stopped_is_ignored():
    async def run_test() -> None:
        extension = make_extension()
        extension._recognizer_epoch = 2
        evt = SimpleNamespace(session_id="old-session")

        await extension._azure_event_handler_on_session_stopped(evt, 1)

        extension._handle_reconnect.assert_not_awaited()  # type: ignore[attr-defined]

    asyncio.run(run_test())


def test_session_stopped_does_not_reconnect_during_transport_recovery():
    async def run_test() -> None:
        extension = make_extension()
        extension._transport_recovery_in_flight = True
        evt = SimpleNamespace(session_id="session-1")

        await extension._azure_event_handler_on_session_stopped(evt, 1)

        extension._handle_reconnect.assert_not_awaited()  # type: ignore[attr-defined]
        assert extension.connected is False

    asyncio.run(run_test())
