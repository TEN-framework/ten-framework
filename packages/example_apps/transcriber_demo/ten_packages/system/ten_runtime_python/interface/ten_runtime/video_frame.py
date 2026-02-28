#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from enum import IntEnum
from typing import TypeVar, cast

from libten_runtime_python import (
    _VideoFrame,  # pyright: ignore[reportPrivateUsage]
    _ten_py_video_frame_register_type,  # pyright: ignore[reportPrivateUsage] # noqa: E501
)

from .msg import Msg

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


class VideoFrame(_VideoFrame, Msg):
    def __init__(self, name: str):  # pyright: ignore[reportMissingSuperCall]
        raise NotImplementedError("Use VideoFrame.create instead.")

    @classmethod
    def create(cls: type[T], name: str) -> T:
        return cast(T, cls.__new__(cls, name))

    def clone(self) -> "VideoFrame":  # pyright: ignore[reportImplicitOverride]
        return cast("VideoFrame", _VideoFrame.clone(self))


_ten_py_video_frame_register_type(VideoFrame)
