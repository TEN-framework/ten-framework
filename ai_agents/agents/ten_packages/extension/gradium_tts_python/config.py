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
SUPPORTED_SAMPLE_RATES = {8000, 16000, 22050, 24000, 44100, 48000}


class GradiumTTSConfig(BaseModel):
    """Configuration for Gradium TTS."""

    api_key: str = ""
    url: str = GRADIUM_DEFAULT_WS_URL
    model_name: str = "default"
    voice_id: str = "cLONiZ4hQ8VpQ4Sz"
    voice: str = ""
    sample_rate: int = 24000
    # JSON string carrying Gradium's optional voice-tuning payload. Kept as a
    # string so it matches the manifest schema (params.json_config: string);
    # the client parses it into an object before sending it on the wire.
    json_config: str | None = None
    close_ws_on_eos: bool = True
    retry_for_s: float | None = None
    pronunciation_id: str = ""
    dump: bool = False
    dump_path: str = "/tmp"
    params: dict[str, Any] = Field(default_factory=dict)

    # Gradium only supports PCM output. The wire-level format string is always
    # derived from `sample_rate` (see update_params); it is not a user-settable
    # knob, so format and sample rate can never disagree.
    output_format: str = ""

    def update_params(self) -> None:
        """Normalize extension-owned config from params and keep vendor extras."""
        params = self._ensure_dict(self.params)
        self.params = params

        for key in (
            "api_key",
            "url",
            "model_name",
            "voice_id",
            "voice",
            "json_config",
            "close_ws_on_eos",
            "retry_for_s",
            "pronunciation_id",
        ):
            if key in params:
                setattr(self, key, params.pop(key))

        if "sample_rate" in params:
            self.sample_rate = int(params.pop("sample_rate"))

        # Gradium only supports PCM; the output format is derived from
        # sample_rate. Drop any user-supplied output_format so it can neither
        # conflict with sample_rate nor be double-sent as a vendor extra.
        params.pop("output_format", None)

        if "dump" in params:
            self.dump = bool(params.pop("dump"))

        if "dump_path" in params:
            self.dump_path = str(params.pop("dump_path"))

        self.output_format = f"pcm_{int(self.sample_rate)}"

    def validate(self) -> None:
        if not self.api_key.strip():
            raise ValueError("API key is required")
        if not self.voice_id.strip() and not self.voice.strip():
            raise ValueError("Either voice_id or voice is required")
        if self.sample_rate not in SUPPORTED_SAMPLE_RATES:
            raise ValueError(
                "sample_rate must be one of "
                f"{sorted(SUPPORTED_SAMPLE_RATES)}"
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
        return self.url.strip() or GRADIUM_DEFAULT_WS_URL

    def get_sample_rate(self) -> int:
        return self.sample_rate

    @staticmethod
    def _ensure_dict(value: Any) -> dict[str, Any]:
        if isinstance(value, dict):
            return value
        return {}
