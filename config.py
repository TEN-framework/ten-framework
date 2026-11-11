"""Breeze TTS configuration for the TEN Framework extension."""

from __future__ import annotations

import copy
from pathlib import Path

from pydantic import Field

from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig

try:  # loaded as a TEN package
    from .breeze_tts import SAMPLE_RATE, BreezeTTSParams
except ImportError:  # loaded flat, e.g. by the unit tests
    from breeze_tts import SAMPLE_RATE, BreezeTTSParams

MAX_INSTRUCTIONS = 500


def _clamp_speed(value: float) -> float:
    """Breeze accepts 0.5 - 2.0; anything outside is clamped."""

    try:
        speed = float(value)
    except (TypeError, ValueError):
        return 1.0
    return min(2.0, max(0.5, speed))


class BreezeTTSConfig(AsyncTTS2HttpConfig):
    """Extension configuration; ``params`` mirrors Breeze request fields."""

    dump: bool = Field(
        default=False, description="Write synthesized PCM to disk for debugging"
    )
    dump_path: str = Field(
        default_factory=lambda: str(Path(__file__).parent),
        description="Directory that receives breeze_dump_<request_id>.pcm",
    )
    params: BreezeTTSParams = Field(default_factory=BreezeTTSParams)

    def update_params(self) -> None:
        params = self.params
        params.base_url = (params.base_url or "https://api.breeze.blue").rstrip(
            "/"
        )
        params.api_key = (params.api_key or "").strip()
        params.speed = _clamp_speed(params.speed)
        if len(params.instructions) > MAX_INSTRUCTIONS:
            params.instructions = params.instructions[:MAX_INSTRUCTIONS]

    def to_str(self, sensitive_handling: bool = True) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)
        config.params.api_key = utils.encrypt(config.params.api_key)
        return f"{config}"

    def validate(self) -> None:
        if not self.params.api_key:
            raise ValueError(
                "params.api_key is required (set BREEZE_API_KEY or configure it "
                "in property.json)"
            )
        if not self.params.voice_id:
            raise ValueError("params.voice_id is required")
        if SAMPLE_RATE != 24000:
            raise ValueError(
                "Breeze PCM is fixed at 24 kHz mono 16-bit; sample_rate must "
                "stay 24000"
            )
