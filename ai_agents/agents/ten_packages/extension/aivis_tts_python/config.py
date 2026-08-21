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

DEFAULT_BASE_URL = "https://api.aivis-project.com"
DEFAULT_MODEL_UUID = "a59cb814-0083-4369-8542-f51a29e72af7"
DEFAULT_SAMPLE_RATE = 16000

# Keys used only by this client — not forwarded in the JSON body.
CLIENT_ONLY_KEYS = frozenset(
    {
        "api_key",
        "base_url",
        "endpoint",
    }
)


class AivisTTSConfig(AsyncTTS2HttpConfig):
    """Aivis Cloud TTS config."""

    dump: bool = Field(default=False, description="Dump PCM for debugging")
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "aivis_tts_in.pcm"),
        description="PCM dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="Aivis TTS params"
    )

    def update_params(self) -> None:
        """Normalize params for Aivis synthesize requests."""
        if "sample_rate" in self.params and "output_sampling_rate" not in self.params:
            self.params["output_sampling_rate"] = int(self.params.pop("sample_rate"))

        self.params.setdefault("output_format", "wav")
        self.params.setdefault("output_sampling_rate", DEFAULT_SAMPLE_RATE)
        self.params.setdefault("output_audio_channels", "mono")
        self.params.setdefault("language", "ja")
        self.params.setdefault("use_ssml", False)
        self.params.setdefault("leading_silence_seconds", 0.0)
        self.params.setdefault("trailing_silence_seconds", 0.1)
        self.params.setdefault("base_url", DEFAULT_BASE_URL)
        self.params.setdefault("model_uuid", DEFAULT_MODEL_UUID)

        # TEN needs raw PCM; force wav so WavStreamParser can strip the header.
        self.params["output_format"] = "wav"

    def request_body(self, text: str) -> dict[str, Any]:
        """Build the /v1/tts/synthesize JSON body."""
        body: dict[str, Any] = {"text": text}
        for key, value in self.params.items():
            if key in CLIENT_ONLY_KEYS:
                continue
            if value is None:
                continue
            body[key] = value
        return body

    def synthesize_url(self) -> str:
        if endpoint := self.params.get("endpoint"):
            return str(endpoint)
        base = str(self.params.get("base_url", DEFAULT_BASE_URL)).rstrip("/")
        return f"{base}/v1/tts/synthesize"

    def to_str(self, sensitive_handling: bool = True) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])
        return f"{config}"

    def validate(self) -> None:
        if "api_key" not in self.params or not self.params["api_key"]:
            raise ValueError("api_key is required for Aivis TTS")
        if "model_uuid" not in self.params or not self.params["model_uuid"]:
            raise ValueError("model_uuid is required for Aivis TTS")
