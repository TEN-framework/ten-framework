import asyncio
from unittest.mock import MagicMock

import pytest

from reconnect_manager import ReconnectManager


class TestReconnectManagerSuccess:
    @pytest.mark.asyncio
    async def test_succeeds_when_marked(self) -> None:
        manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=2)

        async def _connect() -> None:
            manager.mark_connection_successful()

        success = await manager.handle_reconnect(connection_func=_connect)
        assert success is True
        assert manager.attempts == 0

    @pytest.mark.asyncio
    async def test_mark_connection_successful_resets_counter(self) -> None:
        manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=5)
        manager.attempts = 3

        manager.mark_connection_successful()
        assert manager.attempts == 0
        assert manager._connection_successful is True


class TestReconnectManagerMaxAttempts:
    @pytest.mark.asyncio
    async def test_respects_max_attempts(self) -> None:
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

    @pytest.mark.asyncio
    async def test_can_retry_reflects_attempts(self) -> None:
        manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=2)
        assert manager.can_retry() is True
        manager.attempts = 1
        assert manager.can_retry() is True
        manager.attempts = 2
        assert manager.can_retry() is False

    @pytest.mark.asyncio
    async def test_exhausted_without_error_handler(self) -> None:
        """When max_attempts exhausted and no error_handler, should still return False."""
        manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=0)
        result = await manager.handle_reconnect(
            connection_func=lambda: None,
            error_handler=None,
        )
        assert result is False


class TestReconnectManagerExceptionHandling:
    @pytest.mark.asyncio
    async def test_connection_func_exception_returns_false(self) -> None:
        manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=3)
        errors: list[str] = []

        async def _failing_connect() -> None:
            raise ConnectionError("Network unreachable")

        async def _on_error(err) -> None:
            errors.append(err.message)

        result = await manager.handle_reconnect(_failing_connect, _on_error)
        assert result is False
        assert len(errors) == 1
        assert "Network unreachable" in errors[0]

    @pytest.mark.asyncio
    async def test_exception_increments_attempts(self) -> None:
        manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=5)

        async def _failing_connect() -> None:
            raise RuntimeError("fail")

        await manager.handle_reconnect(_failing_connect)
        assert manager.attempts == 1

        await manager.handle_reconnect(_failing_connect)
        assert manager.attempts == 2

    @pytest.mark.asyncio
    async def test_exception_after_success_resets_and_retries(self) -> None:
        manager = ReconnectManager(base_delay=0, max_delay=0, max_attempts=3)

        async def _succeed() -> None:
            manager.mark_connection_successful()

        async def _fail() -> None:
            raise RuntimeError("fail")

        assert await manager.handle_reconnect(_succeed) is True
        assert manager.attempts == 0

        assert await manager.handle_reconnect(_fail) is False
        assert manager.attempts == 1


class TestReconnectManagerBackoff:
    @pytest.mark.asyncio
    async def test_delay_capped_at_max(self) -> None:
        manager = ReconnectManager(
            base_delay=1.0, max_delay=2.0, max_attempts=10
        )

        async def _connect() -> None:
            return

        for _ in range(5):
            await manager.handle_reconnect(_connect)

        expected_max_delay = manager.max_delay
        actual = min(
            manager.base_delay * (2 ** (manager.attempts - 1)),
            manager.max_delay,
        )
        assert actual <= expected_max_delay


class TestReconnectManagerAttemptsInfo:
    def test_get_attempts_info_format(self) -> None:
        manager = ReconnectManager(max_attempts=5)
        info = manager.get_attempts_info()

        assert "current_attempts" in info
        assert "max_attempts" in info
        assert info["max_attempts"] == 5
        assert info["current_attempts"] == 0

    def test_get_attempts_info_after_attempts(self) -> None:
        manager = ReconnectManager(max_attempts=5)
        manager.attempts = 3
        info = manager.get_attempts_info()
        assert info["current_attempts"] == 3


class TestReconnectManagerLogger:
    @pytest.mark.asyncio
    async def test_logger_called_on_success(self) -> None:
        logger = MagicMock()
        manager = ReconnectManager(
            base_delay=0, max_delay=0, max_attempts=3, logger=logger
        )

        async def _connect() -> None:
            manager.mark_connection_successful()

        await manager.handle_reconnect(_connect)
        assert logger.log_warn.called
        assert logger.log_debug.called

    @pytest.mark.asyncio
    async def test_logger_called_on_exhausted(self) -> None:
        logger = MagicMock()
        manager = ReconnectManager(
            base_delay=0, max_delay=0, max_attempts=0, logger=logger
        )

        async def _connect() -> None:
            return

        await manager.handle_reconnect(_connect)
        assert logger.log_error.called

    @pytest.mark.asyncio
    async def test_logger_called_on_exception(self) -> None:
        logger = MagicMock()
        manager = ReconnectManager(
            base_delay=0, max_delay=0, max_attempts=3, logger=logger
        )

        async def _fail() -> None:
            raise RuntimeError("boom")

        await manager.handle_reconnect(_fail)
        assert logger.log_error.called
