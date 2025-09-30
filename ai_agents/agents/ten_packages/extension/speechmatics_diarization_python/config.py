#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from dataclasses import dataclass, field
from typing import Any, Dict

from ..speechmatics_asr_python.config import SpeechmaticsASRConfig


@dataclass
class SpeechmaticsDiarizationConfig(SpeechmaticsASRConfig):
    """Configuration model with diarization-friendly defaults."""

    diarization: str | None = "speaker"
    speaker_diarization_config: Dict[str, Any] = field(default_factory=dict)

    def apply_defaults(self) -> None:
        """Ensure nested diarization config exists when mode is speaker."""
        if self.diarization != "speaker":
            return

        self.speaker_diarization_config.setdefault("prefer_current_speaker", True)
        self.speaker_diarization_config.setdefault("speaker_sensitivity", 0.5)
