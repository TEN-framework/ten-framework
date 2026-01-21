#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#
from typing import Any
import copy
from pydantic import BaseModel, Field
from pathlib import Path


class NvidiaRivaTTSConfig(BaseModel):
    """NVIDIA Riva TTS Config"""

    dump: bool = Field(default=False, description="NVIDIA Riva TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "nvidia_riva_tts_in.pcm"
        ),
        description="NVIDIA Riva TTS dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="NVIDIA Riva TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)
        return f"{config}"

    def validate(self) -> None:
        """Validate NVIDIA Riva-specific configuration."""
        if "server" not in self.params or not self.params["server"]:
            raise ValueError("Server address is required for NVIDIA Riva TTS")
        if (
            "language_code" not in self.params
            or not self.params["language_code"]
        ):
            raise ValueError("Language code is required for NVIDIA Riva TTS")
        if "voice_name" not in self.params or not self.params["voice_name"]:
            raise ValueError("Voice name is required for NVIDIA Riva TTS")
