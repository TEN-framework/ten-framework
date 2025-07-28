#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from enum import IntEnum
from typing import TypeVar, cast
from libten_runtime_python import (
    _AudioFrame,  # pyright: ignore[reportPrivateUsage]
)

T = TypeVar("T", bound="AudioFrame")


# AudioFrameDataFmt values. These definitions need to be the same as the
# TEN_AUDIO_FRAME_DATA_FMT enum in C.
#
# Note: To achieve the best compatibility, any new enum item, should be added
# to the end to avoid changing the value of previous enum items.
class AudioFrameDataFmt(IntEnum):
    INTERLEAVE = 1
    NON_INTERLEAVE = 2


class AudioFrame(_AudioFrame):
    def __init__(self, name: str):
        raise NotImplementedError("Use AudioFrame.create instead.")

    @classmethod
    def create(cls: type[T], name: str) -> T:
        # AudioFrame is a wrapper around _AudioFrame, so this cast is safe
        return cast(T, cls.__new__(cls, name))

    def clone(  # pyright: ignore[reportImplicitOverride]
        self,
    ) -> "AudioFrame":
        # AudioFrame is a wrapper around _AudioFrame, so this cast is safe
        return cast("AudioFrame", _AudioFrame.clone(self))
