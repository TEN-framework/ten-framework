#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from enum import IntEnum
from typing import Type, TypeVar
from libten_runtime_python import _AudioFrame

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
    def __init__(self):
        raise NotImplementedError("Use AudioFrame.create instead.")

    @classmethod
    def create(cls: Type[T], name: str) -> T:
        return cls.__new__(cls, name)

    def clone(self) -> "AudioFrame":
        return _AudioFrame.clone(self)  # type: ignore

    alloc_buf = _AudioFrame.alloc_buf
    lock_buf = _AudioFrame.lock_buf
    unlock_buf = _AudioFrame.unlock_buf
    get_buf = _AudioFrame.get_buf
    get_timestamp = _AudioFrame.get_timestamp
    set_timestamp = _AudioFrame.set_timestamp
    get_sample_rate = _AudioFrame.get_sample_rate
    set_sample_rate = _AudioFrame.set_sample_rate
    get_samples_per_channel = _AudioFrame.get_samples_per_channel
    set_samples_per_channel = _AudioFrame.set_samples_per_channel
    get_bytes_per_sample = _AudioFrame.get_bytes_per_sample
    set_bytes_per_sample = _AudioFrame.set_bytes_per_sample
    get_number_of_channels = _AudioFrame.get_number_of_channels
    set_number_of_channels = _AudioFrame.set_number_of_channels
    get_data_fmt = _AudioFrame.get_data_fmt
    set_data_fmt = _AudioFrame.set_data_fmt
    get_line_size = _AudioFrame.get_line_size
    set_line_size = _AudioFrame.set_line_size
    is_eof = _AudioFrame.is_eof
    set_eof = _AudioFrame.set_eof
