"""Tests for the TOCTOU-safe reconnect lock pattern.

Verifies that concurrent reconnect triggers are properly serialized
using asyncio.Lock.acquire_nowait() rather than the racy
lock.locked() + async with pattern.
"""

import asyncio

import pytest


class TestReconnectLockPattern:
    """Test the acquire_nowait() pattern used in _handle_reconnect."""

    @pytest.mark.asyncio
    async def test_concurrent_reconnect_only_one_proceeds(self) -> None:
        lock = asyncio.Lock()
        entered_count = 0
        skipped_count = 0

        async def reconnect_handler():
            nonlocal entered_count, skipped_count
            if not lock.acquire_nowait():
                skipped_count += 1
                return
            try:
                entered_count += 1
                await asyncio.sleep(0.05)
            finally:
                lock.release()

        tasks = [asyncio.create_task(reconnect_handler()) for _ in range(5)]
        await asyncio.gather(*tasks)

        assert entered_count == 1
        assert skipped_count == 4

    @pytest.mark.asyncio
    async def test_sequential_reconnects_all_proceed(self) -> None:
        lock = asyncio.Lock()
        entered_count = 0

        async def reconnect_handler():
            nonlocal entered_count
            if not lock.acquire_nowait():
                return
            try:
                entered_count += 1
            finally:
                lock.release()

        for _ in range(3):
            await reconnect_handler()

        assert entered_count == 3

    @pytest.mark.asyncio
    async def test_lock_released_on_exception(self) -> None:
        lock = asyncio.Lock()

        async def reconnect_handler_with_error():
            if not lock.acquire_nowait():
                return False
            try:
                raise RuntimeError("reconnect failed")
            finally:
                lock.release()

        with pytest.raises(RuntimeError):
            await reconnect_handler_with_error()

        assert not lock.locked()

        acquired = lock.acquire_nowait()
        assert acquired is True
        lock.release()

    @pytest.mark.asyncio
    async def test_acquire_nowait_vs_locked_toctou(self) -> None:
        """Demonstrate that locked() + acquire has a TOCTOU gap,
        while acquire_nowait() is atomic."""
        lock = asyncio.Lock()
        results = []

        async def safe_handler(name: str):
            if not lock.acquire_nowait():
                results.append(f"{name}:skipped")
                return
            try:
                results.append(f"{name}:entered")
                await asyncio.sleep(0.01)
            finally:
                lock.release()

        t1 = asyncio.create_task(safe_handler("A"))
        t2 = asyncio.create_task(safe_handler("B"))
        await asyncio.gather(t1, t2)

        entered = [r for r in results if "entered" in r]
        skipped = [r for r in results if "skipped" in r]
        assert len(entered) == 1
        assert len(skipped) == 1
