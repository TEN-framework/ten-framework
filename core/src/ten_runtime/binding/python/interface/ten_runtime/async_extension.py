#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import os
import sys
import threading
import traceback
from typing import final

from libten_runtime_python import (
    _Extension,  # pyright: ignore[reportPrivateUsage]
)

from .log_level import LogLevel
from .ten_env import TenEnv
from .async_ten_env import AsyncTenEnv
from .cmd import Cmd
from .data import Data
from .video_frame import VideoFrame
from .audio_frame import AudioFrame


# Thread mode configuration
class ThreadMode:
    """Thread mode enumeration"""

    SINGLE_THREAD: str = "single_thread"
    MULTI_THREAD: str = "multi_thread"


# Cache thread mode at module load time to avoid repeated environment variable reads
_cached_thread_mode: str | None = None


def _get_cached_thread_mode(ten_env: TenEnv) -> str:
    """Get cached thread mode configuration

    Returns:
        str: Thread mode, defaults to single thread mode
    """
    global _cached_thread_mode
    if _cached_thread_mode is None:
        mode = os.getenv("TEN_PYTHON_THREAD_MODE", ThreadMode.SINGLE_THREAD)
        if mode not in [ThreadMode.SINGLE_THREAD, ThreadMode.MULTI_THREAD]:
            ten_env.log_warn(
                f"Warning: Invalid thread mode '{mode}', using default single_thread mode"
            )
            _cached_thread_mode = ThreadMode.SINGLE_THREAD
        else:
            _cached_thread_mode = mode

        ten_env.log_info(
            f"TEN_PYTHON_THREAD_MODE read from environment variable: {_cached_thread_mode}"
        )

    return _cached_thread_mode


def is_single_thread_mode(ten_env: TenEnv) -> bool:
    """Check if single thread mode is used

    Returns:
        bool: True if single thread mode, False otherwise
    """
    return _get_cached_thread_mode(ten_env) == ThreadMode.SINGLE_THREAD


class _GlobalThreadManager:
    """Global thread manager that manages a single event loop thread and reference counting"""

    _instance: "_GlobalThreadManager | None" = None
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
        self._initialized: bool = True

    def get_or_start_thread(self, ten_env: TenEnv) -> asyncio.AbstractEventLoop:
        """Get or start the global main thread"""
        with self._lock:
            if self._main_thread is None or not self._main_thread.is_alive():
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
                # Wait for event loop to start
                while self._main_loop is None:
                    threading.Event().wait(0.01)
            assert (
                self._main_loop is not None
            ), "Main loop should be initialized"
            return self._main_loop

    def get_thread(self) -> asyncio.AbstractEventLoop:
        """Get the global main thread (without starting)"""
        assert (
            self._main_loop is not None
        ), "Main loop should be initialized before calling get_thread"
        return self._main_loop

    def _thread_routine(self, ten_env: TenEnv):
        """Global main thread execution function"""

        self._main_loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._main_loop)

        # Run event loop until stop event is set
        self._main_loop.run_until_complete(self._stop_event.wait())

        # Wait for all pending tasks to complete before closing the loop
        self._main_loop.run_until_complete(
            self._cleanup_pending_tasks(ten_env=ten_env)
        )
        self._main_loop.close()

    async def _cleanup_pending_tasks(self, ten_env: TenEnv):
        """Clean up pending tasks before stopping the event loop"""
        # Get all pending tasks
        pending_tasks = [
            task
            for task in asyncio.all_tasks(self._main_loop)
            if not task.done()
        ]

        if pending_tasks:
            ten_env.log_debug(
                f"Cleaning up {len(pending_tasks)} pending tasks..."
            )

            # Cancel all pending tasks
            for task in pending_tasks:
                task.cancel()

            # Wait for all tasks to complete (they should complete quickly after cancellation)
            try:
                await asyncio.wait_for(
                    asyncio.gather(*pending_tasks, return_exceptions=True),
                    timeout=0.5,  # Give tasks 0.5 second to complete
                )
            except asyncio.TimeoutError:
                ten_env.log_warn(f"Some tasks did not complete within timeout")
            except asyncio.CancelledError:
                # This is expected when tasks are cancelled
                pass
            except Exception as e:
                ten_env.log_warn(f"Error during task cleanup: {e}")

            # Ensure all tasks are properly awaited to avoid "exception was never retrieved" warnings
            for task in pending_tasks:
                if not task.done():
                    try:
                        await task
                    except (asyncio.CancelledError, Exception):
                        # Ignore cancellation and other exceptions during cleanup
                        pass

            ten_env.log_debug(f"Task cleanup completed")

    def increment_ref_count(self):
        """Increment reference count"""
        with self._lock:
            self._ref_count += 1

    def decrement_ref_count(self):
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

    def get_ref_count(self):
        """Get current reference count"""
        with self._lock:
            return self._ref_count


class AsyncExtension(_Extension):
    name: str
    _ten_stop_event: asyncio.Event
    _ten_loop: asyncio.AbstractEventLoop | None
    _ten_thread: threading.Thread | None
    _async_ten_env: AsyncTenEnv | None
    _global_thread_manager: _GlobalThreadManager

    def __new__(cls, name: str):
        instance = super().__new__(cls, name)
        return instance

    def __init__(  # pyright: ignore[reportMissingSuperCall]
        self, name: str
    ) -> None:
        # _Extension is a C module written in C and does not have an __init__
        # method, so we need to ignore pyright's warning.
        #
        # super().__init__(name)

        self.name = name
        self._ten_stop_event = asyncio.Event()

        self._ten_loop = None
        self._ten_thread = None
        self._async_ten_env = None
        self._global_thread_manager = _GlobalThreadManager()

    def __del__(self) -> None:
        pass

    async def _configure_routine(self, ten_env: TenEnv):
        """Configuration routine executed in the global thread"""
        self._ten_loop = asyncio.get_running_loop()

        # Create a virtual thread object for AsyncTenEnv
        # Here we use the current thread identifier
        current_thread = threading.current_thread()

        self._async_ten_env = AsyncTenEnv(
            ten_env, self._ten_loop, current_thread
        )

        await self._wrapper_on_config(self._async_ten_env)
        ten_env.on_configure_done()

        # Suspend until stopEvent is set.
        await self._ten_stop_event.wait()

        await self._wrapper_on_deinit(self._async_ten_env)

        # pylint: disable=protected-access
        self._async_ten_env._internal.on_deinit_done()  # pyright: ignore[reportPrivateUsage] # noqa: E501

        # The completion of async `on_deinit()` (i.e.,
        # `await self._wrapper_on_deinit(...)`) means that all subsequent
        # ten_env API calls by the user will fail. However, any
        # `await ten_env.xxx` before this point may not have finished executing
        # yet. We need to wait for these tasks to complete before stopping the
        # event loop.
        await self._async_ten_env._ten_all_tasks_done_event.wait()  # pyright: ignore[reportPrivateUsage] # noqa: E501
        # pylint: enable=protected-access

        # Decrement reference count when the configuration routine completes
        self._global_thread_manager.decrement_ref_count()

    async def _stop_thread(self):
        self._ten_stop_event.set()

    @final
    def _proxy_on_configure(self, ten_env: TenEnv) -> None:
        if is_single_thread_mode(ten_env):
            # Single thread mode: use global thread manager
            self._proxy_on_configure_single_thread(ten_env)
        else:
            # Multi-thread mode: create independent thread for each extension
            self._proxy_on_configure_multi_thread(ten_env)

    def _proxy_on_configure_single_thread(self, ten_env: TenEnv) -> None:
        """Single thread mode configuration handling"""
        # Increment reference count
        self._global_thread_manager.increment_ref_count()

        # Get or start the global main thread
        main_loop = self._global_thread_manager.get_or_start_thread(ten_env)

        # Submit configuration task to global event loop
        asyncio.run_coroutine_threadsafe(
            self._configure_routine(ten_env), main_loop
        )

    def _proxy_on_configure_multi_thread(self, ten_env: TenEnv) -> None:
        """Multi-thread mode configuration handling"""
        # Create independent event loop and thread
        self._ten_loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._ten_loop)

        # Run configuration in new thread
        self._ten_thread = threading.Thread(
            target=self._run_multi_thread_configure,
            args=(ten_env,),
            daemon=True,
            name=f"AsyncExtension-{self.name}",
        )
        self._ten_thread.start()

    def _run_multi_thread_configure(self, ten_env: TenEnv) -> None:
        """Multi-thread mode configuration execution function"""
        try:
            # Run configuration coroutine
            if self._ten_loop:
                self._ten_loop.run_until_complete(
                    self._configure_routine(ten_env)
                )
        except Exception as e:
            ten_env.log_warn(f"Error in multi-thread configure: {e}")
            traceback.print_exc()
        finally:
            if self._ten_loop and not self._ten_loop.is_closed():
                self._ten_loop.close()

    @final
    def _proxy_on_init(self, ten_env: TenEnv) -> None:
        if is_single_thread_mode(ten_env):
            main_loop = self._global_thread_manager.get_thread()
            asyncio.run_coroutine_threadsafe(
                self._proxy_async_on_init(ten_env), main_loop
            )
        else:
            # Multi-thread mode: run directly in current thread's event loop
            if self._ten_loop and not self._ten_loop.is_closed():
                asyncio.run_coroutine_threadsafe(
                    self._proxy_async_on_init(ten_env), self._ten_loop
                )

    @final
    async def _proxy_async_on_init(self, ten_env: TenEnv):
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        await self._wrapper_on_init(self._async_ten_env)
        ten_env.on_init_done()

    @final
    def _proxy_on_start(self, ten_env: TenEnv) -> None:
        if is_single_thread_mode(ten_env):
            main_loop = self._global_thread_manager.get_thread()
            asyncio.run_coroutine_threadsafe(
                self._proxy_async_on_start(ten_env), main_loop
            )
        else:
            # Multi-thread mode: run directly in current thread's event loop
            if self._ten_loop and not self._ten_loop.is_closed():
                asyncio.run_coroutine_threadsafe(
                    self._proxy_async_on_start(ten_env), self._ten_loop
                )

    @final
    async def _proxy_async_on_start(self, ten_env: TenEnv):
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        await self._wrapper_on_start(self._async_ten_env)
        ten_env.on_start_done()

    @final
    def _proxy_on_stop(self, ten_env: TenEnv) -> None:
        if is_single_thread_mode(ten_env):
            main_loop = self._global_thread_manager.get_thread()
            asyncio.run_coroutine_threadsafe(
                self._proxy_async_on_stop(ten_env), main_loop
            )
        else:
            # Multi-thread mode: run directly in current thread's event loop
            if self._ten_loop and not self._ten_loop.is_closed():
                asyncio.run_coroutine_threadsafe(
                    self._proxy_async_on_stop(ten_env), self._ten_loop
                )

    @final
    async def _proxy_async_on_stop(self, ten_env: TenEnv):
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        await self._wrapper_on_stop(self._async_ten_env)
        ten_env.on_stop_done()

    @final
    def _proxy_on_deinit(self, ten_env: TenEnv) -> None:
        if is_single_thread_mode(ten_env):
            main_loop = self._global_thread_manager.get_thread()
            asyncio.run_coroutine_threadsafe(self._stop_thread(), main_loop)
        else:
            # Multi-thread mode: run directly in current thread's event loop
            if self._ten_loop and not self._ten_loop.is_closed():
                asyncio.run_coroutine_threadsafe(
                    self._stop_thread(), self._ten_loop
                )

    @final
    def _proxy_on_cmd(self, ten_env: TenEnv, cmd: Cmd) -> None:
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        if is_single_thread_mode(ten_env):
            main_loop = self._global_thread_manager.get_thread()
            asyncio.run_coroutine_threadsafe(
                self._wrapper_on_cmd(self._async_ten_env, cmd), main_loop
            )
        else:
            # Multi-thread mode: run directly in current thread's event loop
            if self._ten_loop and not self._ten_loop.is_closed():
                asyncio.run_coroutine_threadsafe(
                    self._wrapper_on_cmd(self._async_ten_env, cmd),
                    self._ten_loop,
                )

    @final
    def _proxy_on_data(self, ten_env: TenEnv, data: Data) -> None:
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        if is_single_thread_mode(ten_env):
            main_loop = self._global_thread_manager.get_thread()
            asyncio.run_coroutine_threadsafe(
                self._wrapper_on_data(self._async_ten_env, data), main_loop
            )
        else:
            # Multi-thread mode: run directly in current thread's event loop
            if self._ten_loop and not self._ten_loop.is_closed():
                asyncio.run_coroutine_threadsafe(
                    self._wrapper_on_data(self._async_ten_env, data),
                    self._ten_loop,
                )

    @final
    def _proxy_on_video_frame(
        self, ten_env: TenEnv, video_frame: VideoFrame
    ) -> None:
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        if is_single_thread_mode(ten_env):
            main_loop = self._global_thread_manager.get_thread()
            asyncio.run_coroutine_threadsafe(
                self._wrapper_on_video_frame(self._async_ten_env, video_frame),
                main_loop,
            )
        else:
            # Multi-thread mode: run directly in current thread's event loop
            if self._ten_loop and not self._ten_loop.is_closed():
                asyncio.run_coroutine_threadsafe(
                    self._wrapper_on_video_frame(
                        self._async_ten_env, video_frame
                    ),
                    self._ten_loop,
                )

    @final
    def _proxy_on_audio_frame(
        self, ten_env: TenEnv, audio_frame: AudioFrame
    ) -> None:
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        if is_single_thread_mode(ten_env):
            main_loop = self._global_thread_manager.get_thread()
            asyncio.run_coroutine_threadsafe(
                self._wrapper_on_audio_frame(self._async_ten_env, audio_frame),
                main_loop,
            )
        else:
            # Multi-thread mode: run directly in current thread's event loop
            if self._ten_loop and not self._ten_loop.is_closed():
                asyncio.run_coroutine_threadsafe(
                    self._wrapper_on_audio_frame(
                        self._async_ten_env, audio_frame
                    ),
                    self._ten_loop,
                )

    # Wrapper methods for handling exceptions in User-defined methods

    async def _wrapper_on_config(self, async_ten_env: AsyncTenEnv):
        try:
            await self.on_configure(async_ten_env)
        except Exception as e:
            self._exit_on_exception(async_ten_env, e)

    async def _wrapper_on_init(self, async_ten_env: AsyncTenEnv):
        try:
            await self.on_init(async_ten_env)
        except Exception as e:
            self._exit_on_exception(async_ten_env, e)

    async def _wrapper_on_start(self, async_ten_env: AsyncTenEnv):
        try:
            await self.on_start(async_ten_env)
        except Exception as e:
            self._exit_on_exception(async_ten_env, e)

    async def _wrapper_on_stop(self, async_ten_env: AsyncTenEnv):
        try:
            await self.on_stop(async_ten_env)
        except Exception as e:
            self._exit_on_exception(async_ten_env, e)

    async def _wrapper_on_deinit(self, async_ten_env: AsyncTenEnv):
        try:
            await self.on_deinit(async_ten_env)
        except Exception as e:
            self._exit_on_exception(async_ten_env, e)

    async def _wrapper_on_cmd(self, async_ten_env: AsyncTenEnv, cmd: Cmd):
        try:
            await self.on_cmd(async_ten_env, cmd)
        except Exception as e:
            self._exit_on_exception(async_ten_env, e)

    async def _wrapper_on_data(self, async_ten_env: AsyncTenEnv, data: Data):
        try:
            await self.on_data(async_ten_env, data)
        except Exception as e:
            self._exit_on_exception(async_ten_env, e)

    async def _wrapper_on_video_frame(
        self, async_ten_env: AsyncTenEnv, video_frame: VideoFrame
    ):
        try:
            await self.on_video_frame(async_ten_env, video_frame)
        except Exception as e:
            self._exit_on_exception(async_ten_env, e)

    async def _wrapper_on_audio_frame(
        self, async_ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ):
        try:
            await self.on_audio_frame(async_ten_env, audio_frame)
        except Exception as e:
            self._exit_on_exception(async_ten_env, e)

    def _exit_on_exception(self, async_ten_env: AsyncTenEnv, e: Exception):
        traceback_info = traceback.format_exc()

        err = async_ten_env.log(
            LogLevel.ERROR,
            f"Uncaught exception: {e} \ntraceback: {traceback_info}",
        )
        if err is not None:
            # If the log_error API fails, print the error message to the
            # console.
            print(f"Uncaught exception: {e} \ntraceback: {traceback_info}")

        # `os._exit` directly calls C's `_exit`, but as a result, it does not
        # flush `stdout/stderr`, which may cause some logs to not be output.
        # Therefore, flushing is proactively called to avoid this issue.
        sys.stdout.flush()
        sys.stderr.flush()

        os._exit(1)

    # Override these methods in your extension

    async def on_configure(self, _ten_env: AsyncTenEnv) -> None:
        pass

    async def on_init(self, _ten_env: AsyncTenEnv) -> None:
        pass

    async def on_start(self, _ten_env: AsyncTenEnv) -> None:
        pass

    async def on_stop(self, _ten_env: AsyncTenEnv) -> None:
        pass

    async def on_deinit(self, _ten_env: AsyncTenEnv) -> None:
        pass

    async def on_cmd(self, _ten_env: AsyncTenEnv, _cmd: Cmd) -> None:
        pass

    async def on_data(self, _ten_env: AsyncTenEnv, _data: Data) -> None:
        pass

    async def on_video_frame(
        self, _ten_env: AsyncTenEnv, _video_frame: VideoFrame
    ) -> None:
        pass

    async def on_audio_frame(
        self, _ten_env: AsyncTenEnv, _audio_frame: AudioFrame
    ) -> None:
        pass
