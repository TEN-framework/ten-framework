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
from typing import AsyncGenerator, Optional, final

from libten_runtime_python import _ExtensionTester
from .cmd_result import CmdResult
from .error import TenError
from .cmd import Cmd
from .data import Data
from .audio_frame import AudioFrame
from .video_frame import VideoFrame
from .test import TenEnvTester
from .test_base import TenEnvTesterBase

CmdResultTuple = tuple[Optional[CmdResult], Optional[TenError]]


class AsyncTenEnvTester(TenEnvTesterBase):
    def __init__(
        self,
        ten_env_tester: TenEnvTester,
        loop: asyncio.AbstractEventLoop,
        thread: threading.Thread,
    ) -> None:
        super().__init__(ten_env_tester._internal)

        self._ten_loop = loop
        self._ten_thread = thread

    def __del__(self) -> None:
        pass

    def _result_handler(
        self,
        result: Optional[CmdResult],
        error: Optional[TenError],
        queue: asyncio.Queue,
    ) -> None:
        asyncio.run_coroutine_threadsafe(
            queue.put([result, error]),
            self._ten_loop,
        )

    def _error_handler(
        self,
        error: Optional[TenError],
        queue: asyncio.Queue,
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

    async def return_result(
        self,
        cmd_result: CmdResult,
    ) -> Optional[TenError]:
        q = asyncio.Queue(maxsize=1)
        err = self._internal.return_result(
            cmd_result,
            lambda _, error: self._error_handler(error, q),
        )
        if err is not None:
            return err

        err = await q.get()
        return err

    def stop_test(self) -> None:
        return self._internal.stop_test()


class AsyncExtensionTester(_ExtensionTester):
    def __init__(self) -> None:
        self._ten_loop = None
        self._async_ten_env_tester = None
        self._ten_thread = None
        self._ten_stop_event = asyncio.Event()

    def __del__(self) -> None:
        pass

    def _exit_on_exception(
        self, async_ten_env_tester: AsyncTenEnvTester, e: Exception
    ):
        traceback_info = traceback.format_exc()

        err = async_ten_env_tester.log_fatal(
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

    async def _thread_routine(self, ten_env_tester: TenEnvTester) -> None:
        assert self._ten_thread is not None

        self._ten_loop = asyncio.get_running_loop()
        self._async_ten_env_tester = AsyncTenEnvTester(
            ten_env_tester, self._ten_loop, self._ten_thread
        )

        await self._wrapper_on_start(self._async_ten_env_tester)
        ten_env_tester.on_start_done()

        # Suspend the thread until stopEvent is set.
        await self._ten_stop_event.wait()

    @final
    async def _stop_thread(self) -> None:
        self._ten_stop_event.set()

    @final
    def _proxy_on_start(self, ten_env_tester: TenEnvTester) -> None:
        self._ten_thread = threading.Thread(
            target=asyncio.run, args=(self._thread_routine(ten_env_tester),)
        )
        self._ten_thread.start()

    async def _wrapper_on_start(
        self, ten_env_tester: AsyncTenEnvTester
    ) -> None:
        try:
            await self.on_start(ten_env_tester)
        except Exception as e:
            self._exit_on_exception(ten_env_tester, e)

    async def _wrapper_on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        try:
            await self.on_stop(ten_env_tester)
        except Exception as e:
            self._exit_on_exception(ten_env_tester, e)

    @final
    async def _proxy_async_on_stop(self, ten_env_tester: TenEnvTester):
        assert self._async_ten_env_tester is not None

        await self._wrapper_on_stop(self._async_ten_env_tester)
        # pylint: disable=protected-access
        ten_env_tester._internal.on_stop_done()
        # pylint: enable=protected-access

    @final
    def _proxy_on_stop(self, ten_env_tester: TenEnvTester) -> None:
        assert self._ten_loop is not None

        asyncio.run_coroutine_threadsafe(
            self._proxy_async_on_stop(ten_env_tester), self._ten_loop
        )

    async def _wrapper_on_deinit(
        self, ten_env_tester: AsyncTenEnvTester
    ) -> None:
        try:
            await self.on_deinit(ten_env_tester)
        except Exception as e:
            self._exit_on_exception(ten_env_tester, e)

    @final
    async def _proxy_async_on_deinit(
        self, ten_env_tester: TenEnvTester
    ) -> None:
        assert self._async_ten_env_tester is not None

        await self._wrapper_on_deinit(self._async_ten_env_tester)
        # pylint: disable=protected-access
        ten_env_tester._internal.on_deinit_done()
        # pylint: enable=protected-access

    @final
    def _proxy_on_deinit(self, _ten_env_tester: TenEnvTester) -> None:
        assert self._ten_loop is not None

        asyncio.run_coroutine_threadsafe(
            self._proxy_async_on_deinit(_ten_env_tester),
            self._ten_loop,
        )

    @final
    def _proxy_on_cmd(self, _ten_env_tester: TenEnvTester, cmd: Cmd) -> None:
        assert self._async_ten_env_tester is not None
        assert self._ten_loop is not None

        asyncio.run_coroutine_threadsafe(
            self._wrapper_on_cmd(self._async_ten_env_tester, cmd),
            self._ten_loop,
        )

    async def _wrapper_on_cmd(
        self, ten_env_tester: AsyncTenEnvTester, cmd: Cmd
    ) -> None:
        try:
            await self.on_cmd(ten_env_tester, cmd)
        except Exception as e:
            self._exit_on_exception(ten_env_tester, e)

    @final
    def _proxy_on_data(self, _ten_env_tester: TenEnvTester, data: Data) -> None:
        assert self._async_ten_env_tester is not None
        assert self._ten_loop is not None

        asyncio.run_coroutine_threadsafe(
            self._wrapper_on_data(self._async_ten_env_tester, data),
            self._ten_loop,
        )

    async def _wrapper_on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        try:
            await self.on_data(ten_env_tester, data)
        except Exception as e:
            self._exit_on_exception(ten_env_tester, e)

    @final
    def _proxy_on_audio_frame(
        self, _ten_env_tester: TenEnvTester, audio_frame: AudioFrame
    ) -> None:
        assert self._async_ten_env_tester is not None
        assert self._ten_loop is not None

        asyncio.run_coroutine_threadsafe(
            self._wrapper_on_audio_frame(
                self._async_ten_env_tester, audio_frame
            ),
            self._ten_loop,
        )

    async def _wrapper_on_audio_frame(
        self, ten_env_tester: AsyncTenEnvTester, audio_frame: AudioFrame
    ) -> None:
        try:
            await self.on_audio_frame(ten_env_tester, audio_frame)
        except Exception as e:
            self._exit_on_exception(ten_env_tester, e)

    @final
    def _proxy_on_video_frame(
        self, _ten_env_tester: TenEnvTester, video_frame: VideoFrame
    ) -> None:
        assert self._async_ten_env_tester is not None
        assert self._ten_loop is not None

        asyncio.run_coroutine_threadsafe(
            self._wrapper_on_video_frame(
                self._async_ten_env_tester, video_frame
            ),
            self._ten_loop,
        )

    async def _wrapper_on_video_frame(
        self, ten_env_tester: AsyncTenEnvTester, video_frame: VideoFrame
    ) -> None:
        try:
            await self.on_video_frame(ten_env_tester, video_frame)
        except Exception as e:
            self._exit_on_exception(ten_env_tester, e)

    @final
    def set_test_mode_single(
        self, addon_name: str, property_json_str: Optional[str] = None
    ) -> None:
        return _ExtensionTester.set_test_mode_single(
            self, addon_name, property_json_str
        )

    @final
    def run(self) -> None:
        # This is a blocking operation.
        _ExtensionTester.run(self)

        # The `extension_tester` has two attributes.
        #
        # 1. All `on_xxx` APIs of the `extension_tester` act as proxies for
        # `test_extension`. From this perspective, the `extension_tester` has
        # the attributes of an `extension`.
        #
        # 2. The `extension_tester` also has a `run` API, which is equivalent to
        # the `run` API of an `app` in a testing scenario. From this
        # perspective, the `extension_tester` has the attributes of an `app`.
        #
        # When the `run` API of the `extension_tester` completes, it signifies
        # that all tasks in the extension world have finished and no new tasks
        # will be added (since the extension context has fully ended). At this
        # point, it is an appropriate time to enqueue a final close task into
        # the asyncio run loop queue to terminate the asyncio thread.
        #
        # In the context of an `async_extension`, since there is no specific
        # `run` API completion point, a similar behavior is implemented using a
        # callback triggered after `on_deinit_done`.

        # If execution reaches this point, it means that the TEN app thread used
        # for testing has already ended. Therefore, the process to terminate the
        # asyncio thread/runloop is initiated.

        if self._ten_thread and self._ten_thread.is_alive():
            assert self._ten_loop is not None

            asyncio.run_coroutine_threadsafe(
                self._stop_thread(), self._ten_loop
            )

            # Wait for the asyncio thread to finish.
            self._ten_thread.join()

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        pass

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        pass

    async def on_deinit(self, ten_env: AsyncTenEnvTester) -> None:
        pass

    async def on_cmd(self, ten_env: AsyncTenEnvTester, cmd: Cmd) -> None:
        pass

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        pass

    async def on_audio_frame(
        self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame
    ) -> None:
        pass

    async def on_video_frame(
        self, ten_env: AsyncTenEnvTester, video_frame: VideoFrame
    ) -> None:
        pass
