#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import re
from typing import Any
from urllib.parse import urlsplit

from pydantic import ConfigDict, Field, field_validator
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig

from .client import model_family


class GmicloudTTS2Config(AsyncTTS2HttpConfig):
    model_config = ConfigDict(validate_assignment=True)

    params: dict[str, Any] = Field(default_factory=dict)

    api_key: str = ""
    base_url: str = "https://console.gmicloud.ai"
    model: str = "inworld-tts-2"
    voice: str | None = None
    speed: float = 1.0
    audio_format: str = "WAV"
    sample_rate: int = 24000
    channels: int = 1
    text_chunk_chars: int = 1000
    request_timeout_sec: float = Field(default=60.0, gt=0, allow_inf_nan=False)
    poll_interval_sec: float = Field(default=2.0, gt=0, allow_inf_nan=False)
    poll_attempts: int = Field(default=90, gt=0)

    @field_validator("sample_rate")
    @classmethod
    def validate_sample_rate(cls, value: int) -> int:
        if not 8000 <= value <= 192000:
            raise ValueError("sample_rate must be between 8000 and 192000")
        return value

    @field_validator("channels")
    @classmethod
    def validate_channels(cls, value: int) -> int:
        if not 1 <= value <= 8:
            raise ValueError("channels must be between 1 and 8")
        return value

    @field_validator("speed")
    @classmethod
    def validate_speed(cls, value: float) -> float:
        if not 0.5 <= value <= 2.0:
            raise ValueError("speed must be between 0.5 and 2.0")
        return value

    @field_validator("audio_format")
    @classmethod
    def validate_audio_format(cls, value: str) -> str:
        if value.lower() != "wav":
            raise ValueError("audio_format must be WAV (PCM frames are emitted)")
        return value

    @field_validator("base_url")
    @classmethod
    def validate_base_url(cls, value: str) -> str:
        parts = urlsplit(value)
        if parts.scheme != "https" or not parts.netloc:
            raise ValueError("base_url must be an https:// URL")
        return value.rstrip("/")

    def update_params(self) -> None:
        values = dict(self.params)
        for name in (
            "api_key",
            "base_url",
            "model",
            "voice",
            "speed",
            "audio_format",
            "sample_rate",
            "channels",
            "text_chunk_chars",
            "request_timeout_sec",
            "poll_interval_sec",
            "poll_attempts",
        ):
            if name in values:
                setattr(self, name, values.pop(name))
        self.params = values

    def validate(self) -> None:
        if not self.api_key.strip():
            raise ValueError("api_key is required")
        if not self.model.strip():
            raise ValueError("model is required")
        if not re.fullmatch(r"[A-Za-z0-9._-]{1,128}", self.model):
            raise ValueError("model contains unsupported characters")
        if model_family(self.model) == "chatterbox" and not self.voice:
            raise ValueError(
                "Chatterbox models require a Resemble voice UUID (voice_uuid): "
                "set params.voice (or use a different model)."
            )

    def masked_api_key(self) -> str:
        key = self.api_key.strip()
        return f"{key[:4]}***" if len(key) > 4 else "***"

    def to_str(self, sensitive_handling: bool = True) -> str:
        api_key = self.masked_api_key() if sensitive_handling else self.api_key
        return (
            f"GmicloudTTS2Config(base_url={self.base_url}, model={self.model}, "
            f"voice={self.voice}, speed={self.speed}, "
            f"sample_rate={self.sample_rate}, channels={self.channels}, "
            f"text_chunk_chars={self.text_chunk_chars}, api_key={api_key})"
        )
