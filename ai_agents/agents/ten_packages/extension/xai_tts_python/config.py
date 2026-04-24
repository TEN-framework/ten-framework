from __future__ import annotations

from typing import Any
import copy

from ten_ai_base import utils

from pydantic import BaseModel, Field


class XAITTSConfig(BaseModel):
    api_key: str = ""
    base_url: str = "wss://api.x.ai/v1/tts"
    voice_id: str = "eve"
    language: str = "en"
    codec: str = "pcm"
    sample_rate: int = 24000
    bit_rate: int = 128000
    optimize_streaming_latency: int = 0
    text_normalization: bool = False

    dump: bool = False
    dump_path: str = "/tmp"
    params: dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        params = self._ensure_dict(self.params)
        self.params = params

        self.api_key = str(params.pop("api_key", self.api_key) or "")
        self.base_url = str(params.pop("base_url", self.base_url) or "")
        self.voice_id = str(params.pop("voice_id", self.voice_id) or "")
        self.language = str(params.pop("language", self.language) or "")
        self.codec = str(params.pop("codec", self.codec) or "")
        self.sample_rate = int(
            params.pop("sample_rate", self.sample_rate) or self.sample_rate
        )
        self.bit_rate = int(
            params.pop("bit_rate", self.bit_rate) or self.bit_rate
        )
        self.optimize_streaming_latency = int(
            params.pop(
                "optimize_streaming_latency",
                self.optimize_streaming_latency,
            )
            or self.optimize_streaming_latency
        )
        self.text_normalization = bool(
            params.pop("text_normalization", self.text_normalization)
        )

    def validate(self) -> None:
        if not self.api_key:
            raise ValueError("API key is required")
        if not (
            self.api_key.startswith("xai-")
            or self.api_key.startswith("test")
        ):
            raise ValueError("API key must start with 'xai-'")
        if self.sample_rate not in {8000, 16000, 22050, 24000, 44100, 48000}:
            raise ValueError(f"Unsupported sample rate: {self.sample_rate}")
        if self.codec not in {"pcm", "mp3", "wav", "mulaw", "ulaw", "alaw"}:
            raise ValueError(f"Unsupported codec: {self.codec}")

    def to_str(self, sensitive_handling: bool = True) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)

        return f"{config}"

    @staticmethod
    def _ensure_dict(value: Any) -> dict[str, Any]:
        if isinstance(value, dict):
            return value
        return {}
