#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from __future__ import annotations

import sys
import types


def _install_ten_runtime_stub() -> None:
    if "ten_runtime" in sys.modules:
        return

    module = types.ModuleType("ten_runtime")

    class AsyncExtension:
        def __init__(self, _name: str):
            pass

    class AsyncTenEnv:
        pass

    class AudioFrame:
        pass

    class VideoFrame:
        pass

    class Cmd:
        def __init__(self, name: str):
            self._name = name

        @classmethod
        def create(cls, name: str) -> "Cmd":
            return cls(name)

        def get_name(self) -> str:
            return self._name

    class StatusCode:
        OK = 0

    class CmdResult:
        @classmethod
        def create(cls, _status_code, _cmd):
            return cls()

    class Data:
        def __init__(self, name: str):
            self._name = name
            self._payload = ""

        @classmethod
        def create(cls, name: str) -> "Data":
            return cls(name)

        def get_name(self) -> str:
            return self._name

        def set_property_from_json(self, _path, payload: str) -> None:
            self._payload = payload

        def get_property_to_json(self, _path):
            return self._payload, None

    class Addon:
        pass

    class TenEnv:
        def on_create_instance_done(self, *_args, **_kwargs):
            pass

    def register_addon_as_extension(_name: str):
        def decorator(cls):
            return cls

        return decorator

    module.AsyncExtension = AsyncExtension
    module.AsyncTenEnv = AsyncTenEnv
    module.AudioFrame = AudioFrame
    module.VideoFrame = VideoFrame
    module.Cmd = Cmd
    module.StatusCode = StatusCode
    module.CmdResult = CmdResult
    module.Data = Data
    module.Addon = Addon
    module.TenEnv = TenEnv
    module.register_addon_as_extension = register_addon_as_extension
    sys.modules["ten_runtime"] = module


def _install_ten_ai_base_stub() -> None:
    if "ten_ai_base" in sys.modules:
        return

    module = types.ModuleType("ten_ai_base")
    utils_module = types.ModuleType("ten_ai_base.utils")

    def encrypt(value: str) -> str:
        if not value:
            return value
        return "*" * min(len(value), 6)

    class ErrorMessage:
        def __init__(self, module: str, message: str, code: int):
            self.module = module
            self.message = message
            self.code = code

        def model_dump_json(self) -> str:
            return (
                '{"module":"%s","message":"%s","code":%d}'
                % (self.module, self.message, self.code)
            )

    utils_module.encrypt = encrypt
    module.utils = utils_module
    module.ErrorMessage = ErrorMessage
    sys.modules["ten_ai_base"] = module
    sys.modules["ten_ai_base.utils"] = utils_module


_install_ten_runtime_stub()
_install_ten_ai_base_stub()
