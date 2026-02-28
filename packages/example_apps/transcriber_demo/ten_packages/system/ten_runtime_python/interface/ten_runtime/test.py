#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import Callable, final

from libten_runtime_python import (
    _ExtensionTester,  # pyright: ignore[reportPrivateUsage]
    _ten_py_ten_env_tester_register_type,  # pyright: ignore[reportPrivateUsage] # noqa: E501
)


from .test_base import TenEnvTesterBase
from .error import TenError
from .cmd import Cmd
from .cmd_result import CmdResult
from .data import Data
from .audio_frame import AudioFrame
from .video_frame import VideoFrame


ResultHandler = Callable[
    ["TenEnvTester", CmdResult | None, TenError | None], None
]


ErrorHandler = Callable[["TenEnvTester", TenError | None], None]


class TenEnvTester(TenEnvTesterBase):
    def on_init_done(self) -> None:
        return self._internal.on_init_done()

    def on_start_done(self) -> None:
        return self._internal.on_start_done()

    def on_stop_done(self) -> None:
        return self._internal.on_stop_done()

    def on_deinit_done(self) -> None:
        return self._internal.on_deinit_done()

    def send_cmd(
        self, cmd: Cmd, result_handler: ResultHandler | None = None
    ) -> TenError | None:
        return self._internal.send_cmd(cmd, result_handler, False)

    def send_cmd_ex(
        self, cmd: Cmd, result_handler: ResultHandler | None = None
    ) -> TenError | None:
        return self._internal.send_cmd(cmd, result_handler, True)

    def send_data(
        self, data: Data, error_handler: ErrorHandler | None = None
    ) -> TenError | None:
        return self._internal.send_data(data, error_handler)

    def send_audio_frame(
        self,
        audio_frame: AudioFrame,
        error_handler: ErrorHandler | None = None,
    ) -> TenError | None:
        return self._internal.send_audio_frame(audio_frame, error_handler)

    def send_video_frame(
        self,
        video_frame: VideoFrame,
        error_handler: ErrorHandler | None = None,
    ) -> TenError | None:
        return self._internal.send_video_frame(video_frame, error_handler)

    def return_result(
        self,
        cmd_result: CmdResult,
        error_handler: ErrorHandler | None = None,
    ) -> TenError | None:
        return self._internal.return_result(cmd_result, error_handler)


class ExtensionTester(_ExtensionTester):
    @final
    def set_test_mode_single(
        self, addon_name: str, property_json_str: str | None = None
    ) -> None:
        return _ExtensionTester.set_test_mode_single_internal(
            self, addon_name, property_json_str
        )

    @final
    def set_test_mode_graph(self, graph_json: str) -> None:
        return _ExtensionTester.set_test_mode_graph_internal(self, graph_json)

    @final
    def run(self) -> TenError | None:
        return _ExtensionTester.run_internal(self)

    @final
    def _proxy_on_init(self, ten_env_tester: TenEnvTester) -> None:
        self.on_init(ten_env_tester)

    def on_init(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.on_init_done()

    @final
    def _proxy_on_start(self, ten_env_tester: TenEnvTester) -> None:
        self.on_start(ten_env_tester)

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.on_start_done()

    @final
    def _proxy_on_stop(self, ten_env_tester: TenEnvTester) -> None:
        self.on_stop(ten_env_tester)

    def on_stop(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.on_stop_done()

    @final
    def _proxy_on_deinit(self, ten_env_tester: TenEnvTester) -> None:
        self.on_deinit(ten_env_tester)

    def on_deinit(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.on_deinit_done()

    @final
    def _proxy_on_cmd(self, ten_env_tester: TenEnvTester, cmd: Cmd) -> None:
        self.on_cmd(ten_env_tester, cmd)

    def on_cmd(self, _ten_env_tester: TenEnvTester, _cmd: Cmd) -> None:
        pass

    @final
    def _proxy_on_data(self, ten_env_tester: TenEnvTester, data: Data) -> None:
        self.on_data(ten_env_tester, data)

    def on_data(self, _ten_env_tester: TenEnvTester, _data: Data) -> None:
        pass

    @final
    def _proxy_on_audio_frame(
        self, ten_env_tester: TenEnvTester, audio_frame: AudioFrame
    ) -> None:
        self.on_audio_frame(ten_env_tester, audio_frame)

    def on_audio_frame(
        self, _ten_env_tester: TenEnvTester, _audio_frame: AudioFrame
    ) -> None:
        pass

    @final
    def _proxy_on_video_frame(
        self, ten_env_tester: TenEnvTester, video_frame: VideoFrame
    ) -> None:
        self.on_video_frame(ten_env_tester, video_frame)

    def on_video_frame(
        self, _ten_env_tester: TenEnvTester, _video_frame: VideoFrame
    ) -> None:
        pass


_ten_py_ten_env_tester_register_type(TenEnvTester)
