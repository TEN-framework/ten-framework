#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from __future__ import annotations

from typing import Any
import copy

from pydantic import BaseModel, Field
from ten_ai_base import utils

GRADIUM_DEFAULT_WS_URL = "wss://api.gradium.ai/api/speech/tts"
PCM_OUTPUT_FORMATS = {
    "pcm",
    "pcm_8000",
    "pcm_16000",
    "pcm_22050",
    "pcm_24000",
    "pcm_44100",
    "pcm_48000",
}


class GradiumTTSConfig(BaseModel):
    """Configuration for Gradium TTS."""

    api_key: str = ""
    base_url: str = GRADIUM_DEFAULT_WS_URL
    model_name: str = "default"
    voice_id: str = "cLONiZ4hQ8VpQ4Sz"
    voice: str = ""
    output_format: str = ""
    sample_rate: int = 24000
    json_config: dict[str, Any] | str | None = None
    close_ws_on_eos: bool = True
    retry_for_s: float | None = None
    pronunciation_id: str = ""
    dump: bool = False
    dump_path: str = "/tmp"
    params: dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        """Normalize extension-owned config from params and keep vendor extras."""
        params = self._ensure_dict(self.params)
        self.params = params

        for key in (
            "api_key",
            "base_url",
            "model_name",
            "voice_id",
            "voice",
            "output_format",
            "json_config",
            "close_ws_on_eos",
            "retry_for_s",
            "pronunciation_id",
        ):
            if key in params:
                setattr(self, key, params.pop(key))

        if "sample_rate" in params:
            self.sample_rate = int(params.pop("sample_rate"))

        if "dump" in params:
            self.dump = bool(params.pop("dump"))

        if "dump_path" in params:
            self.dump_path = str(params.pop("dump_path"))

        self.output_format = self._normalize_output_format(
            self.output_format, self.sample_rate
        )
        self.sample_rate = self._sample_rate_from_output_format(
            self.output_format, self.sample_rate
        )

    def validate(self) -> None:
        if not self.api_key.strip():
            raise ValueError("API key is required")
        if not self.voice_id.strip() and not self.voice.strip():
            raise ValueError("Either voice_id or voice is required")
        if self.output_format not in PCM_OUTPUT_FORMATS:
            raise ValueError(
                "output_format must be one of " f"{sorted(PCM_OUTPUT_FORMATS)}"
            )

    def to_str(self, sensitive_handling: bool = True) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        if isinstance(config.params, dict) and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])
        return f"{config}"

    def websocket_url(self) -> str:
        return self.base_url.strip() or GRADIUM_DEFAULT_WS_URL

    def get_sample_rate(self) -> int:
        return self.sample_rate

    @staticmethod
    def _ensure_dict(value: Any) -> dict[str, Any]:
        if isinstance(value, dict):
            return value
        return {}

    @staticmethod
    def _normalize_output_format(output_format: str, sample_rate: int) -> str:
        if output_format:
            return str(output_format).lower()
        return f"pcm_{int(sample_rate)}"

    @staticmethod
    def _sample_rate_from_output_format(
        output_format: str, current_sample_rate: int
    ) -> int:
        if output_format == "pcm":
            return current_sample_rate or 48000
        if output_format.startswith("pcm_"):
            try:
                return int(output_format.split("_", maxsplit=1)[1])
            except (IndexError, ValueError):
                return current_sample_rate
        return current_sample_rate
