#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import threading
from asyncio import AbstractEventLoop
from typing import AsyncGenerator, Optional

from .cmd import Cmd
from .data import Data
from .video_frame import VideoFrame
from .audio_frame import AudioFrame
from .cmd_result import CmdResult
from .ten_env import TenEnv
from .error import TenError
from .ten_env_base import TenEnvBase

CmdResultTuple = tuple[Optional[CmdResult], Optional[TenError]]


class AsyncTenEnv(TenEnvBase):

    def __init__(
        self, ten_env: TenEnv, loop: AbstractEventLoop, thread: threading.Thread
    ) -> None:
        super().__init__(ten_env._internal)

        self._ten_loop = loop
        self._ten_thread = thread
        self._ten_all_tasks_done_event = asyncio.Event()
        ten_env._set_release_handler(self._on_release)

    def __del__(self) -> None:
        pass

    def _result_handler(
        self,
        result,
        error: Optional[TenError],
        queue: asyncio.Queue,
    ) -> None:
        asyncio.run_coroutine_threadsafe(
            queue.put([result, error]),
            self._ten_loop,
        )

    def _error_handler(
        self, error: Optional[TenError], queue: asyncio.Queue
    ) -> None:
        asyncio.run_coroutine_threadsafe(
            queue.put(error),
            self._ten_loop,
        )

    async def send_cmd(self, cmd: Cmd) -> CmdResultTuple:
        q = asyncio.Queue(maxsize=1)
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
        q = asyncio.Queue(maxsize=10)
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

    async def send_data(self, data: Data) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
        err = self._internal.send_data(
            data,
            lambda _, error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def send_video_frame(
        self, video_frame: VideoFrame
    ) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
        err = self._internal.send_video_frame(
            video_frame,
            lambda _, error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def send_audio_frame(
        self, audio_frame: AudioFrame
    ) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
        err = self._internal.send_audio_frame(
            audio_frame,
            lambda _, error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def return_result(self, result: CmdResult) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
        err = self._internal.return_result(
            result,
            lambda _, error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def get_property_to_json(
        self, path: Optional[str] = None
    ) -> tuple[str, Optional[TenError]]:
        q = asyncio.Queue(maxsize=1)
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
    ) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
        err = self._internal.set_property_string_async(
            path,
            json_str,
            lambda error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    async def get_property_int(
        self, path: str
    ) -> tuple[int, Optional[TenError]]:
        q = asyncio.Queue(maxsize=1)
        err = self._internal.get_property_int_async(
            path,
            lambda result, error: self._result_handler(result, error, q),
        )
        if err is not None:
            return 0, err

        [result, err] = await q.get()

        return result, err

    async def set_property_int(
        self, path: str, value: int
    ) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
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
    ) -> tuple[str, Optional[TenError]]:
        q = asyncio.Queue(maxsize=1)
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
    ) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
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
    ) -> tuple[bool, Optional[TenError]]:
        q = asyncio.Queue(maxsize=1)
        err = self._internal.get_property_bool_async(
            path,
            lambda result, error: self._result_handler(result, error, q),
        )
        if err is not None:
            return False, err

        [result, err] = await q.get()

        return result, err

    async def set_property_bool(
        self, path: str, value: int
    ) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
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
    ) -> tuple[float, Optional[TenError]]:
        q = asyncio.Queue(maxsize=1)
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
    ) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
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
    ) -> tuple[bool, Optional[TenError]]:
        q = asyncio.Queue(maxsize=1)
        err = self._internal.is_property_exist_async(
            path,
            lambda result, error: self._result_handler(result, error, q),
        )
        if err is not None:
            return False, err

        [result, err] = await q.get()
        return result, err

    async def init_property_from_json(
        self, json_str: str
    ) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
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

        # Wait for the internal thread to finish.
        self._ten_thread.join()
