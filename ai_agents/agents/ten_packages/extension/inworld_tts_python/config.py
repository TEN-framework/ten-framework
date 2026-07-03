#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any
import copy
from pathlib import Path
from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class InworldTTSConfig(AsyncTTS2HttpConfig):
    """Inworld TTS Config"""

    dump: bool = Field(default=False, description="Inworld TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "inworld_tts_in.pcm"
        ),
        description="Inworld TTS dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="Inworld TTS params"
    )

    def update_params(self) -> None:
        """Update configuration from params dictionary"""
        # Remove text if present (will be set from input)
        if "text" in self.params:
            del self.params["text"]

        # Set default model if not specified
        if "model" not in self.params:
            self.params["model"] = "inworld-tts-1.5-max"

        # Set default voice if not specified
        if "voice" not in self.params:
            self.params["voice"] = "Ashley"

        # Set default sample_rate if not specified
        if "sample_rate" not in self.params:
            self.params["sample_rate"] = 24000

        # Set default encoding to LINEAR16 (PCM)
        if "encoding" not in self.params:
            self.params["encoding"] = "LINEAR16"

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields in params
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])

        return f"{config}"

    def validate(self) -> None:
        """Validate Inworld-specific configuration."""
        if "api_key" not in self.params or not self.params["api_key"]:
            raise ValueError("API key is required for Inworld TTS")
        if "model" not in self.params or not self.params["model"]:
            raise ValueError("Model is required for Inworld TTS")
        if "voice" not in self.params or not self.params["voice"]:
            raise ValueError("Voice is required for Inworld TTS")
