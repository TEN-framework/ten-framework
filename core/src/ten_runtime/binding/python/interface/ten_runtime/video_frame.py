#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from enum import IntEnum
from typing import Type, TypeVar
from libten_runtime_python import _VideoFrame

T = TypeVar("T", bound="VideoFrame")


# PixelFmt values. These definitions need to be the same as the PixelFmt
# enum in C.
#
# Note: To achieve the best compatibility, any new enum item, should be added
# to the end to avoid changing the value of previous enum items.
class PixelFmt(IntEnum):
    RGB24 = 1
    RGBA = 2
    BGR24 = 3
    BGRA = 4
    I422 = 5
    I420 = 6
    NV21 = 7
    NV12 = 8


class VideoFrame(_VideoFrame):
    def __init__(self):
        raise NotImplementedError("Use VideoFrame.create instead.")

    @classmethod
    def create(cls: Type[T], name: str) -> T:
        return cls.__new__(cls, name)

    def clone(self) -> "VideoFrame":
        return _VideoFrame.clone(self)  # type: ignore

    alloc_buf = _VideoFrame.alloc_buf
    lock_buf = _VideoFrame.lock_buf
    unlock_buf = _VideoFrame.unlock_buf
    get_buf = _VideoFrame.get_buf
    get_width = _VideoFrame.get_width
    set_width = _VideoFrame.set_width
    get_height = _VideoFrame.get_height
    set_height = _VideoFrame.set_height
    get_timestamp = _VideoFrame.get_timestamp
    set_timestamp = _VideoFrame.set_timestamp
    get_pixel_fmt = _VideoFrame.get_pixel_fmt
    set_pixel_fmt = _VideoFrame.set_pixel_fmt
    is_eof = _VideoFrame.is_eof
    set_eof = _VideoFrame.set_eof
