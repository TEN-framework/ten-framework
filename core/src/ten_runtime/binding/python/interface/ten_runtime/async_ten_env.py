#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import threading
from asyncio import AbstractEventLoop
from collections.abc import AsyncGenerator
from typing import TypeVar

from .error import TenError
from .ten_env_base import TenEnvBase
from .ten_env import TenEnv
from .cmd import Cmd
from .data import Data
from .video_frame import VideoFrame
from .audio_frame import AudioFrame
from .cmd_result import CmdResult
from .global_thread_manager import GlobalThreadManager
from .send_options import SendOptions

CmdResultTuple = tuple[CmdResult | None, TenError | None]
ResultHandlerResultType = TypeVar(
    "ResultHandlerResultType", bound=CmdResult | str | bool | int | float | None
)


class AsyncTenEnv(TenEnvBase):
    _ten_loop: AbstractEventLoop
    _ten_thread: threading.Thread
    _ten_all_tasks_done_event: asyncio.Event
    _global_thread_manager: GlobalThreadManager | None

    def __init__(
        self,
        ten_env: TenEnv,
        loop: AbstractEventLoop,
        thread: threading.Thread,
        global_thread_manager: GlobalThreadManager | None,
    ) -> None:
        super().__init__(ten_env._internal)

        self._ten_loop = loop
        self._ten_thread = thread
        self._ten_all_tasks_done_event = asyncio.Event()
        self._global_thread_manager = global_thread_manager
        ten_env._set_release_handler(  # pyright: ignore[reportPrivateUsage]
            self._on_release
        )

    def _result_handler(
        self,
        result: ResultHandlerResultType,
        error: TenError | None,
        queue: asyncio.Queue[tuple[ResultHandlerResultType, TenError | None]],
    ) -> None:
        asyncio.run_coroutine_threadsafe(
            queue.put((result, error)),
            self._ten_loop,
        )

    def _error_handler(
        self,
        error: TenError | None,
        queue: asyncio.Queue[TenError | None],
    ) -> None:
        asyncio.run_coroutine_threadsafe(
            queue.put(error),
            self._ten_loop,
        )

    async def send_cmd(self, cmd: Cmd) -> CmdResultTuple:
        q = asyncio.Queue[tuple[CmdResult | None, TenError | None]](maxsize=1)
        err = self._internal.send_cmd(
            cmd,
            lambda _, result, error: self._result_handler(result, error, q),
            False,
        )
        if err is not None:
            return None, err

        [result, err] = await q.get()

        if result is not None:
            assert result.is_completed()

        return result, err

    async def send_cmd_ex(
        self, cmd: Cmd
    ) -> AsyncGenerator[CmdResultTuple, None]:
        q = asyncio.Queue[tuple[CmdResult | None, TenError | None]](maxsize=10)
        err = self._internal.send_cmd(
            cmd,
            lambda _, result, error: self._result_handler(result, error, q),
            True,
        )
        if err is not None:
            yield None, err
            return

        while True:
            [result, err] = await q.get()
            yield result, err

            if err is not None:
                break
            elif result is not None and result.is_completed():
                # This is the final result, so break the while loop.
                break

    async def send_data(
        self, data: Data, options: SendOptions | None = None
    ) -> TenError | None:
        # If options is None or doesn't wait for result, use fire-and-forget
        # mode.
        if options is None or not options.wait_for_result:
            err = self._internal.send_data(data, None)
            return err

        # If wait for result, use the original async waiting mode.
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.send_data(
            data,
            lambda _, error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def send_video_frame(
        self, video_frame: VideoFrame, options: SendOptions | None = None
    ) -> TenError | None:
        # If options is None or doesn't wait for result, use fire-and-forget
        # mode.
        if options is None or not options.wait_for_result:
            err = self._internal.send_video_frame(video_frame, None)
            return err

        # If wait for result, use the original async waiting mode.
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.send_video_frame(
            video_frame,
            lambda _, error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def send_audio_frame(
        self, audio_frame: AudioFrame, options: SendOptions | None = None
    ) -> TenError | None:
        # If options is None or doesn't wait for result, use fire-and-forget
        # mode.
        if options is None or not options.wait_for_result:
            err = self._internal.send_audio_frame(audio_frame, None)
            return err

        # If wait for result, use the original async waiting mode.
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.send_audio_frame(
            audio_frame,
            lambda _, error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def return_result(
        self, result: CmdResult, options: SendOptions | None = None
    ) -> TenError | None:
        # If options is None or doesn't wait for result, use fire-and-forget
        # mode.
        if options is None or not options.wait_for_result:
            err = self._internal.return_result(result, None)
            return err

        # If wait for result, use the original async waiting mode.
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.return_result(
            result,
            lambda _, error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def get_property_to_json(
        self, path: str | None = None
    ) -> tuple[str, TenError | None]:
        q = asyncio.Queue[tuple[str, TenError | None]](maxsize=1)
        err = self._internal.get_property_to_json_async(
            path,
            lambda result, error: self._result_handler(result, error, q),
        )
        if err is not None:
            return "", err

        [result, err] = await q.get()

        return result, err

    async def set_property_from_json(
        self, path: str, json_str: str
    ) -> TenError | None:
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.set_property_from_json_async(
            path,
            json_str,
            lambda error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def get_property_int(self, path: str) -> tuple[int, TenError | None]:
        q = asyncio.Queue[tuple[int, TenError | None]](maxsize=1)
        err = self._internal.get_property_int_async(
            path,
            lambda result, error: self._result_handler(result, error, q),
        )
        if err is not None:
            return 0, err

        [result, err] = await q.get()

        return result, err

    async def set_property_int(self, path: str, value: int) -> TenError | None:
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.set_property_int_async(
            path,
            value,
            lambda error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def get_property_string(
        self, path: str
    ) -> tuple[str, TenError | None]:
        q = asyncio.Queue[tuple[str, TenError | None]](maxsize=1)
        err = self._internal.get_property_string_async(
            path,
            lambda result, error: self._result_handler(result, error, q),
        )
        if err is not None:
            return "", err

        [result, err] = await q.get()

        return result, err

    async def set_property_string(
        self, path: str, value: str
    ) -> TenError | None:
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.set_property_string_async(
            path,
            value,
            lambda error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def get_property_bool(
        self, path: str
    ) -> tuple[bool, TenError | None]:
        q = asyncio.Queue[tuple[bool, TenError | None]](maxsize=1)
        err = self._internal.get_property_bool_async(
            path,
            lambda result, error: self._result_handler(result, error, q),
        )
        if err is not None:
            return False, err

        [result, err] = await q.get()

        return result, err

    async def set_property_bool(self, path: str, value: int) -> TenError | None:
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.set_property_bool_async(
            path,
            value,
            lambda error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def get_property_float(
        self, path: str
    ) -> tuple[float, TenError | None]:
        q = asyncio.Queue[tuple[float, TenError | None]](maxsize=1)
        err = self._internal.get_property_float_async(
            path,
            lambda result, error: self._result_handler(result, error, q),
        )
        if err is not None:
            return 0.0, err

        [result, err] = await q.get()

        return result, err

    async def set_property_float(
        self, path: str, value: float
    ) -> TenError | None:
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.set_property_float_async(
            path,
            value,
            lambda error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def is_property_exist(
        self, path: str
    ) -> tuple[bool, TenError | None]:
        q = asyncio.Queue[tuple[bool, TenError | None]](maxsize=1)
        err = self._internal.is_property_exist_async(
            path,
            lambda result, error: self._result_handler(result, error, q),
        )
        if err is not None:
            return False, err

        [result, err] = await q.get()
        return result, err

    async def init_property_from_json(self, json_str: str) -> TenError | None:
        q = asyncio.Queue[TenError | None](maxsize=1)
        err = self._internal.init_property_from_json_async(
            json_str,
            lambda error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def _close_loop(self):
        self._ten_all_tasks_done_event.set()

    def _on_release(self) -> None:
        # At this point, all tasks that were submitted before `on_deinit_done`
        # have been completed. Therefore, at this time, the run loop of
        # `_ten_thread` will be closed by setting a flag.
        #
        # At the `_on_release` point in time, we can guarantee that there are no
        # TEN API-related tasks in the Python asyncio task queue. However, there
        # may still be other asyncio tasks (e.g., `await asyncio.sleep(...)`).
        # Allowing these non-"TEN" API tasks to receive a cancellation exception
        # caused by the termination of the asyncio runloop is reasonable.
        #
        # The reason we can guarantee that there are no TEN API-related tasks in
        # the Python asyncio task queue at this point is that within
        # `ten_py_ten_env_on_deinit_done`, an `on_deinit_done` C task is added
        # as the last task to the C runloop. At the same time, `ten_env_proxy`
        # is synchronously set to `NULL`, ensuring that no new TEN API-related C
        # tasks will be added to the C runloop task queue afterward. This is
        # because Python TEN API calls will immediately return an error
        # synchronously at the Python binding layer when
        # `ten_env_proxy == NULL`.
        asyncio.run_coroutine_threadsafe(self._close_loop(), self._ten_loop)

        # Check if we need to join the thread based on reference count
        if self._global_thread_manager is not None:
            # Decrement reference count and check if it reaches 0
            ref_count = self._global_thread_manager.decrement_ref_count()

            # Only join the thread if reference count reaches 0
            if ref_count <= 0:
                # Wait for the internal thread to finish.
                #
                # Note: This action is needed if each async extension has its
                # own asyncio thread, but is not needed when all async
                # extensions use a single shared asyncio thread.
                self._ten_thread.join()
                self._global_thread_manager.reset()
        else:
            # Fallback: if no global thread manager reference, join the thread
            # This maintains backward compatibility
            self._ten_thread.join()
