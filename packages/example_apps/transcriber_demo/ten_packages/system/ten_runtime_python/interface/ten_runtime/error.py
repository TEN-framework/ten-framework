#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from enum import IntEnum
from typing import TypeVar, cast

from libten_runtime_python import (
    _TenError,  # pyright: ignore[reportPrivateUsage]
    _ten_py_error_register_type,  # pyright: ignore[reportPrivateUsage]
)

T = TypeVar("T", bound="TenError")


# We use IntEnum so it can be directly compared with integers without converting
# to enum
class TenErrorCode(IntEnum):
    # ErrorCodeGeneric is the default errno, for those users only care error
    # msgs.
    ErrorCodeGeneric = 1

    # ErrorCodeInvalidJSON means the json data is invalid.
    ErrorCodeInvalidJSON = 2

    # ErrorCodeInvalidArgument means invalid parameter.
    ErrorCodeInvalidArgument = 3

    # ErrorCodeInvalidType means invalid type.
    ErrorCodeInvalidType = 4

    # ErrorCodeInvalidGraph means invalid graph.
    ErrorCodeInvalidGraph = 5

    # ErrorCodeTenIsClosed means the TEN world is closed.
    ErrorCodeTenIsClosed = 6

    # ErrorCodeMsgNotConnected means the msg is not connected in the graph.
    ErrorCodeMsgNotConnected = 7

    # ErrorCodeTimeout means timed out.
    ErrorCodeTimeout = 8


class TenError(_TenError):
    def __init__(self, error_code: int, error_message: str | None):
        raise NotImplementedError("Use TenError.create instead.")

    @classmethod
    def create(
        cls: type[T],
        error_code: TenErrorCode,
        error_message: str | None = None,
    ) -> T:
        return cast(T, cls.__new__(cls, error_code.value, error_message))


_ten_py_error_register_type(TenError)
