#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from typing import TypeVar, cast

from libten_runtime_python import (
    _Data,  # pyright: ignore[reportPrivateUsage]
    _ten_py_data_register_type,  # pyright: ignore[reportPrivateUsage]
)

from .msg import Msg

T = TypeVar("T", bound="Data")


class Data(_Data, Msg):
    def __init__(self, name: str):  # pyright: ignore[reportMissingSuperCall]
        raise NotImplementedError("Use Data.create instead.")

    @classmethod
    def create(cls: type[T], name: str) -> T:
        return cast(T, cls.__new__(cls, name))

    def clone(self) -> "Data":  # pyright: ignore[reportImplicitOverride]
        return cast("Data", _Data.clone(self))


_ten_py_data_register_type(Data)
