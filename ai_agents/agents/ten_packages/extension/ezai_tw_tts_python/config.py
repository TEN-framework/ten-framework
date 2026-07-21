import copy
from typing import Any
from pathlib import Path

from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class EZAITWTTSConfig(AsyncTTS2HttpConfig):
    """EZAI TW TTS Config"""

    # EZAI-specific audio settings
    sample_rate: int = Field(default=24000, description="PCM sample rate")
    channels: int = Field(default=1, description="PCM channel count")
    sample_width: int = Field(
        default=2, description="Bytes per sample (PCM_16)"
    )

    # Debug and logging
    dump: bool = Field(
        default=False, description="Dump PCM to disk for debugging"
    )
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent / "ezai_tts_in.pcm"),
        description="Path for dump file",
    )

    # Passthrough params dict (contains url, voice, speed, denoise, zh_model, api_key)
    params: dict[str, Any] = Field(
        default_factory=dict, description="EZAI TW TTS params"
    )
    url: str = "https://matcha.ezai-k8s.freeddns.org/tts"
    voice: str = "IU_IUF1003"
    denoise: bool = True
    speed: float = 0.8
    zh_model: str = "nllb"
    api_key: str = ""

    def update_param(self, key, dtype) -> None:
        if key in self.params:
            try:
                val = dtype(self.params[key]) if dtype else self.params[key]
                if key == "voice" and not val.strip():
                    val = "IU_IUF1003"
                if key == "speed" and float(val) < 0.1:
                    val = 0.8
                setattr(self, key, val)
            except Exception:
                pass

    def update_params(self) -> None:
        for k, d in [
            ("url", str),
            ("voice", str),
            ("zh_model", str),
            ("speed", float),
            ("channels", int),
            ("sample_rate", int),
            ("sample_width", int),
            ("denoise", bool),
            ("api_key", str),
        ]:
            self.update_param(k, d)

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields in params
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)

        return f"{config}"

    def validate(self) -> None:
        """Validate EZAI-specific configuration."""
        if "api_key" not in self.params or not self.params["api_key"]:
            raise ValueError("API key is required for EZAI TW TTS")
