"""Tests for the reconnect lock pattern.

Verifies that concurrent reconnect triggers are properly serialized
using asyncio.Lock.locked() guard followed by await lock.acquire().
In a single-threaded async event loop there is no preemption between
locked() and the subsequent acquire(), so the TOCTOU gap is safe.
"""

import asyncio

import pytest


class TestReconnectLockPattern:
    """Test the locked() + acquire() pattern used in _handle_reconnect."""

    @pytest.mark.asyncio
    async def test_concurrent_reconnect_only_one_proceeds(self) -> None:
        lock = asyncio.Lock()
        entered_count = 0
        skipped_count = 0

        async def reconnect_handler():
            nonlocal entered_count, skipped_count
            if lock.locked():
                skipped_count += 1
                return
            await lock.acquire()
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
            if lock.locked():
                return
            await lock.acquire()
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
            if lock.locked():
                return False
            await lock.acquire()
            try:
                raise RuntimeError("reconnect failed")
            finally:
                lock.release()

        with pytest.raises(RuntimeError):
            await reconnect_handler_with_error()

        assert not lock.locked()

        await lock.acquire()
        assert lock.locked()
        lock.release()

    @pytest.mark.asyncio
    async def test_locked_guard_is_safe_in_async(self) -> None:
        """In a single-threaded event loop, locked() + acquire() is safe
        because there is no preemption between the two calls within
        the same coroutine."""
        lock = asyncio.Lock()
        results = []

        async def safe_handler(name: str):
            if lock.locked():
                results.append(f"{name}:skipped")
                return
            await lock.acquire()
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
