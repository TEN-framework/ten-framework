#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
"""Capture snapshots and immutable references for the common timing checks."""

from dataclasses import dataclass
from fractions import Fraction


@dataclass(frozen=True)
class CapturedSubtitle:
    sentence_start_bytes: int
    payload: dict


@dataclass(frozen=True)
class CapturedTiming:
    audio_setting: dict
    pcm_bytes: int
    subtitles: tuple[CapturedSubtitle, ...]
    error: str | None = None


@dataclass(frozen=True)
class ReferenceWord:
    word: str
    start_ms: Fraction
    end_ms: Fraction


@dataclass(frozen=True)
class RequestTimingReference:
    vendor: str
    sample_rate: int
    channels: int
    pcm_bytes: int
    captions: tuple[tuple[ReferenceWord, ...], ...]
    error: str | None = None


class TimingReferenceSource:
    """Keep capture and vendor reference calculation separate."""

    def __init__(self, hook, build_reference):
        self.hook = hook
        self.build_reference = build_reference

    def begin_request(self, request_id):
        self.hook.begin_request(request_id)

    def reference(self, request_id):
        return self.build_reference(self.hook.capture(request_id))
