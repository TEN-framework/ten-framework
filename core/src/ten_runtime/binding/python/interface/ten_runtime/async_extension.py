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

from libten_runtime_python import _Extension
from .video_frame import VideoFrame
from .audio_frame import AudioFrame
from .ten_env import TenEnv
from .cmd import Cmd
from .data import Data
from .async_ten_env import AsyncTenEnv


class AsyncExtension(_Extension):
    def __new__(cls, name: str):
        instance = super().__new__(cls, name)
        return instance

    def __init__(self, name: str) -> None:
        self.name = name
        self._ten_stop_event = asyncio.Event()

        self._ten_loop = None
        self._async_ten_env = None
        self._ten_thread = None

    def __del__(self) -> None:
        pass

    async def _thread_routine(self, ten_env: TenEnv):
        self._ten_loop = asyncio.get_running_loop()

        assert (
            self._ten_thread is not None
        ), "self._ten_thread should never be None"

        self._async_ten_env = AsyncTenEnv(
            ten_env, self._ten_loop, self._ten_thread
        )

        await self._wrapper_on_config(self._async_ten_env)
        ten_env.on_configure_done()

        # Suspend the thread until stopEvent is set.
        await self._ten_stop_event.wait()

        await self._wrapper_on_deinit(self._async_ten_env)

        # pylint: disable=protected-access
        self._async_ten_env._internal.on_deinit_done()

        # The completion of async `on_deinit()` (i.e.,
        # `await self._wrapper_on_deinit(...)`) means that all subsequent
        # ten_env API calls by the user will fail. However, any
        # `await ten_env.xxx` before this point may not have finished executing
        # yet. We need to wait for these tasks to complete before stopping the
        # event loop.
        await self._async_ten_env._ten_all_tasks_done_event.wait()
        # pylint: enable=protected-access

    async def _stop_thread(self):
        self._ten_stop_event.set()

    @final
    def _proxy_on_configure(self, ten_env: TenEnv) -> None:
        # We pass the TenEnv object to another Python thread without worrying
        # about the thread safety issue of the TenEnv API, because the actual
        # execution logic of all TenEnv APIs occurs in the extension thread.
        # We only need to ensure that the TenEnv object should remain valid
        # while it is being used. The way to achieve this is to ensure that the
        # Python thread remains alive until TenEnv.on_deinit_done is called.
        self._ten_thread = threading.Thread(
            target=asyncio.run, args=(self._thread_routine(ten_env),)
        )
        self._ten_thread.start()

    @final
    def _proxy_on_init(self, ten_env: TenEnv) -> None:
        assert self._ten_loop is not None, "self._ten_loop should never be None"
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
        assert self._ten_loop is not None, "self._ten_loop should never be None"
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
        assert self._ten_loop is not None, "self._ten_loop should never be None"
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
    def _proxy_on_deinit(self, _ten_env: TenEnv) -> None:
        assert self._ten_loop is not None, "self._ten_loop should never be None"
        asyncio.run_coroutine_threadsafe(self._stop_thread(), self._ten_loop)

    @final
    def _proxy_on_cmd(self, _ten_env: TenEnv, cmd: Cmd) -> None:
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        assert self._ten_loop is not None, "self._ten_loop should never be None"
        asyncio.run_coroutine_threadsafe(
            self._wrapper_on_cmd(self._async_ten_env, cmd), self._ten_loop
        )

    @final
    def _proxy_on_data(self, _ten_env: TenEnv, data: Data) -> None:
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        assert self._ten_loop is not None, "self._ten_loop should never be None"
        asyncio.run_coroutine_threadsafe(
            self._wrapper_on_data(self._async_ten_env, data), self._ten_loop
        )

    @final
    def _proxy_on_video_frame(
        self, _ten_env: TenEnv, video_frame: VideoFrame
    ) -> None:
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        assert self._ten_loop is not None, "self._ten_loop should never be None"
        asyncio.run_coroutine_threadsafe(
            self._wrapper_on_video_frame(self._async_ten_env, video_frame),
            self._ten_loop,
        )

    @final
    def _proxy_on_audio_frame(
        self, _ten_env: TenEnv, audio_frame: AudioFrame
    ) -> None:
        assert (
            self._async_ten_env is not None
        ), "self._async_ten_env should never be None"
        assert self._ten_loop is not None, "self._ten_loop should never be None"
        asyncio.run_coroutine_threadsafe(
            self._wrapper_on_audio_frame(self._async_ten_env, audio_frame),
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

        err = async_ten_env.log_fatal(
            f"Uncaught exception: {e} \ntraceback: {traceback_info}"
        )
        if err is not None:
            # If the log_fatal API fails, print the error message to the
            # console.
            print(f"Uncaught exception: {e} \ntraceback: {traceback_info}")

        # `os._exit` directly calls C's `_exit`, but as a result, it does not
        # flush `stdout/stderr`, which may cause some logs to not be output.
        # Therefore, flushing is proactively called to avoid this issue.
        sys.stdout.flush()
        sys.stderr.flush()

        os._exit(1)

    # Override these methods in your extension

    async def on_configure(self, ten_env: AsyncTenEnv) -> None:
        pass

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        pass

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        pass

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        pass

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        pass

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        pass

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        pass

    async def on_video_frame(
        self, ten_env: AsyncTenEnv, video_frame: VideoFrame
    ) -> None:
        pass

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        pass
