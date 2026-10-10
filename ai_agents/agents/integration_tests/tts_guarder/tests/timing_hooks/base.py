#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
"""Immutable provider references consumed by the common timing checks."""

from dataclasses import dataclass
from fractions import Fraction
from typing import Protocol


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
    complete: bool
    error: str | None = None


class TimingReferenceSource(Protocol):
    def begin_request(self, request_id: str) -> None:
        """Identify the next request using public TEN test input."""

    def reference(self, request_id: str) -> RequestTimingReference:
        """Return an independent, immutable snapshot of provider timing."""
