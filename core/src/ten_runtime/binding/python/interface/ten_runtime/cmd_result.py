#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import TypeVar, cast
from enum import IntEnum

from libten_runtime_python import (
    _CmdResult,  # pyright: ignore[reportPrivateUsage]
    _Cmd,  # pyright: ignore[reportPrivateUsage]
    _ten_py_cmd_result_register_type,  # pyright: ignore[reportPrivateUsage] # noqa: E501
)

from .msg import Msg
from .cmd import Cmd

T = TypeVar("T", bound="CmdResult")


# StatusCode values. These definitions need to be the same as the
# TEN_STATUS_CODE enum in C.
#
# Note: To achieve the best compatibility, any new enum item, should be added
# to the end to avoid changing the value of previous enum items.
class StatusCode(IntEnum):
    OK = 0
    ERROR = 1


class CmdResult(_CmdResult, Msg):
    def __init__(  # pyright: ignore[reportMissingSuperCall]
        self, status_code: int, target_cmd: _Cmd
    ):
        raise NotImplementedError("Use CmdResult.create instead.")

    @classmethod
    def create(cls: type[T], status_code: StatusCode, target_cmd: Cmd) -> T:
        return cast(T, cls.__new__(cls, status_code, target_cmd))

    def clone(self) -> "CmdResult":  # pyright: ignore[reportImplicitOverride]
        return cast("CmdResult", _CmdResult.clone(self))

    def get_status_code(  # pyright: ignore[reportImplicitOverride]
        self,
    ) -> StatusCode:
        return StatusCode(_CmdResult.get_status_code(self))

    def set_final(  # pyright: ignore[reportImplicitOverride]
        self, is_final: bool
    ):
        return _CmdResult.set_final(self, is_final)


_ten_py_cmd_result_register_type(CmdResult)
