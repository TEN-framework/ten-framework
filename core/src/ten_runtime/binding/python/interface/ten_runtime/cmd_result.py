#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import Type, TypeVar
from enum import IntEnum

from libten_runtime_python import _CmdResult

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


class CmdResult(_CmdResult):
    def __init__(self):
        raise NotImplementedError("Use CmdResult.create instead.")

    @classmethod
    def create(cls: Type[T], status_code: StatusCode, target_cmd: Cmd) -> T:
        return cls.__new__(cls, status_code, target_cmd)

    def clone(self) -> "CmdResult":
        return _CmdResult.clone(self)  # type: ignore

    def get_status_code(self) -> StatusCode:
        return StatusCode(_CmdResult.get_status_code(self))

    def set_final(self, is_final: bool):
        if is_final:
            return _CmdResult.set_final(self, 1)
        else:
            return _CmdResult.set_final(self, 0)

    def is_final(self) -> bool:
        return _CmdResult.is_final(self)

    def is_completed(self) -> bool:
        return _CmdResult.is_completed(self)
