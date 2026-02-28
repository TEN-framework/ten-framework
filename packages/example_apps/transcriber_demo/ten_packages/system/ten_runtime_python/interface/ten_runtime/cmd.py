#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import TypeVar, cast

from libten_runtime_python import (
    _Cmd,  # pyright: ignore[reportPrivateUsage]
    _ten_py_cmd_register_type,  # pyright: ignore[reportPrivateUsage]
)

from .msg import Msg

T = TypeVar("T", bound="Cmd")


class Cmd(_Cmd, Msg):
    def __init__(self, name: str):  # pyright: ignore[reportMissingSuperCall]
        raise NotImplementedError("Use Cmd.create instead.")

    @classmethod
    def create(cls: type[T], name: str) -> T:
        return cast(T, cls.__new__(cls, name))

    def clone(self) -> "Cmd":  # pyright: ignore[reportImplicitOverride]
        return cast("Cmd", _Cmd.clone(self))


_ten_py_cmd_register_type(Cmd)
