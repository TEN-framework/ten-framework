#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any
from pydantic import BaseModel, Field
from ten_ai_base import utils


class GradiumTTSConfig(BaseModel):
    """Configuration for Gradium TTS."""

    api_key: str = ""
    base_url: str = ""
    region: str = "us"
    model_name: str = "default"
    voice_id: str = ""
    output_format: str = "pcm"
    params: dict[str, Any] = Field(default_factory=dict)
    dump: bool = False
    dump_path: str = "/tmp"

    def update_params(self) -> None:
        """Merge nested params into top-level fields for convenience."""
        params = dict(self.params)

        if "api_key" in params:
            self.api_key = params.pop("api_key")
        if "base_url" in params:
            self.base_url = params.pop("base_url")
        if "region" in params:
            self.region = params.pop("region")
        if "model_name" in params:
            self.model_name = params.pop("model_name")
        if "voice_id" in params:
            self.voice_id = params.pop("voice_id")
        if "output_format" in params:
            self.output_format = params.pop("output_format")
        if "dump" in params:
            self.dump = params.pop("dump")
        if "dump_path" in params:
            self.dump_path = params.pop("dump_path")

        self.params = params

    def validate(self) -> None:
        """Validate configuration values."""
        if not self.api_key or self.api_key.strip() == "":
            raise ValueError("api_key is required for Gradium TTS")
        if not self.voice_id or self.voice_id.strip() == "":
            raise ValueError("voice_id is required for Gradium TTS")

        allowed_formats = {"pcm", "pcm_16000", "pcm_24000"}
        self.output_format = self.output_format.lower()
        if self.output_format not in allowed_formats:
            raise ValueError(
                f"output_format must be one of {sorted(allowed_formats)}"
            )

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Return a stringified config with sensitive fields optionally masked."""
        if not sensitive_handling:
            return f"{self}"

        config = self.copy(deep=True)
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        return f"{config}"

    def websocket_url(self) -> str:
        """Build the websocket URL based on region or explicit base_url."""
        if self.base_url:
            return self.base_url

        region = (self.region or "us").lower()
        if region not in {"us", "eu"}:
            region = "us"
        return f"wss://{region}.api.gradium.ai/api/speech/tts"

    def get_sample_rate(self) -> int:
        """Return sample rate based on output format."""
        fmt = self.output_format.lower()
        if fmt == "pcm_16000":
            return 16000
        if fmt == "pcm_24000":
            return 24000
        return 48000
