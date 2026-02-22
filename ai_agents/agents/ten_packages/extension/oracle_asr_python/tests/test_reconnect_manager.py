import pytest

from reconnect_manager import ReconnectManager


@pytest.mark.asyncio
async def test_reconnect_manager_succeeds_when_marked() -> None:
    manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=2)

    async def _connect() -> None:
        manager.mark_connection_successful()

    success = await manager.handle_reconnect(connection_func=_connect)
    assert success is True
    assert manager.attempts == 0


@pytest.mark.asyncio
async def test_reconnect_manager_respects_max_attempts() -> None:
    manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=2)
    errors: list[str] = []

    async def _connect() -> None:
        return

    async def _on_error(err) -> None:
        errors.append(err.message)

    assert await manager.handle_reconnect(_connect, _on_error) is False
    assert await manager.handle_reconnect(_connect, _on_error) is False
    assert await manager.handle_reconnect(_connect, _on_error) is False

    assert len(errors) == 1
    assert "Maximum reconnection attempts reached" in errors[0]
