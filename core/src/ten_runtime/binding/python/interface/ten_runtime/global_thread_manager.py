#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import threading
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .ten_env import TenEnv


class GlobalThreadManager:
    """Global thread manager that manages a single event loop thread and reference counting"""

    _instance: "GlobalThreadManager | None" = None
    _lock: threading.Lock = threading.Lock()

    def __new__(cls):
        if cls._instance is None:
            with cls._lock:
                if cls._instance is None:
                    cls._instance = super().__new__(cls)
                    cls._instance._initialized = False
        return cls._instance

    def __init__(self):
        if self._initialized:
            return

        self._ref_count: int = 0
        self._main_thread: threading.Thread | None = None
        self._main_loop: asyncio.AbstractEventLoop | None = None
        self._stop_event: asyncio.Event = asyncio.Event()
        self._loop_ready_event: threading.Event = threading.Event()
        self._initialized: bool = True

    def reset(self):
        with self._lock:
            self._ref_count = 0
            self._main_thread = None
            self._main_loop = None
            self._stop_event = asyncio.Event()
            self._loop_ready_event = threading.Event()
            self._initialized = True

    def get_or_start_thread(
        self, ten_env: "TenEnv"
    ) -> asyncio.AbstractEventLoop:
        """Get or start the global main thread"""
        with self._lock:
            if self._main_thread is None or not self._main_thread.is_alive():
                # Clear the ready event before starting a new thread
                self._loop_ready_event.clear()

                try:
                    import namedthreads

                    namedthreads.patch()
                except ImportError:
                    ten_env.log_warn(
                        "Warning: namedthreads not available, thread names will not be set in system level"
                    )

                self._main_thread = threading.Thread(
                    target=self._thread_routine,
                    args=(ten_env,),
                    daemon=True,
                    name="PythonGlobalMainThread",
                )
                self._main_thread.start()

        # Wait for event loop to start outside the lock
        # Always wait to ensure the loop is ready, even if the thread already exists
        # If the loop is already ready, wait() will return immediately
        self._loop_ready_event.wait()

        assert self._main_loop is not None, "Main loop should be initialized"
        return self._main_loop

    def get_thread(self) -> asyncio.AbstractEventLoop:
        """Get the global main thread (without starting)"""
        assert (
            self._main_loop is not None
        ), "Main loop should be initialized before calling get_thread"
        return self._main_loop

    def _thread_routine(self, ten_env: "TenEnv"):
        """Global main thread execution function"""

        try:
            import uvloop

            self._main_loop = uvloop.new_event_loop()
        except ImportError:
            ten_env.log_warn(
                "Warning: uvloop not available, using default event loop"
            )
            self._main_loop = asyncio.new_event_loop()

        asyncio.set_event_loop(self._main_loop)

        # Signal that the event loop is ready
        self._loop_ready_event.set()

        # Run event loop until stop event is set
        self._main_loop.run_until_complete(self._stop_event.wait())

        # Wait for all pending tasks to complete before closing the loop
        self._main_loop.run_until_complete(
            self._cleanup_pending_tasks(ten_env=ten_env)
        )
        self._main_loop.close()

    async def _cleanup_pending_tasks(self, ten_env: "TenEnv"):
        """Clean up pending tasks before stopping the event loop"""
        # Get all pending tasks, excluding the current task (this cleanup task itself)
        current_task = asyncio.current_task(self._main_loop)
        pending_tasks = [
            task
            for task in asyncio.all_tasks(self._main_loop)
            if not task.done() and task is not current_task
        ]

        if pending_tasks:
            ten_env.log_debug(
                f"Cleaning up {len(pending_tasks)} pending tasks..."
            )

            # Cancel all pending tasks
            for task in pending_tasks:
                task.cancel()

            # Wait for all tasks to complete (they should complete quickly
            # after cancellation)
            try:
                # Use asyncio.wait instead of gather to avoid CancelledError issues
                _, pending = await asyncio.wait_for(
                    asyncio.wait(
                        pending_tasks, return_when=asyncio.ALL_COMPLETED
                    ),
                    timeout=0.5,  # Give tasks 0.5 second to complete
                )
                if pending:
                    ten_env.log_warn(
                        f"Some tasks did not complete within timeout"
                    )
            except asyncio.TimeoutError:
                ten_env.log_warn(f"Some tasks did not complete within timeout")
            except Exception as e:
                ten_env.log_warn(f"Error during task cleanup: {e}")

            # Ensure all tasks are properly awaited to avoid "exception was
            # never retrieved" warnings
            for task in pending_tasks:
                if not task.done():
                    try:
                        await task
                    except (asyncio.CancelledError, Exception):
                        # Ignore cancellation and other exceptions during
                        # cleanup
                        pass

            ten_env.log_debug(f"Task cleanup completed")

    def increment_ref_count(self):
        """Increment reference count"""
        with self._lock:
            self._ref_count += 1

    def decrement_ref_count(self) -> int:
        """Decrement reference count, stop thread if count reaches 0"""
        with self._lock:
            self._ref_count -= 1
            if self._ref_count <= 0:
                self._ref_count = 0
                if self._main_loop is not None:
                    # Create a coroutine to set the stop event
                    async def set_stop_event():
                        self._stop_event.set()

                    asyncio.run_coroutine_threadsafe(
                        set_stop_event(), self._main_loop
                    )

            return self._ref_count

    def get_ref_count(self) -> int:
        """Get current reference count"""
        with self._lock:
            return self._ref_count
