#
# Breeze TTS extension for the TEN Framework.
# Licensed under the Apache License, Version 2.0.
#
from __future__ import annotations

from ten_ai_base.tts2_http import (
    AsyncTTS2HttpClient,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpExtension,
)
from ten_runtime import AsyncTenEnv

try:  # loaded as a TEN package
    from .breeze_tts import SAMPLE_RATE, BreezeTTSClient
    from .config import BreezeTTSConfig
except ImportError:  # loaded flat, e.g. by the unit tests
    from breeze_tts import SAMPLE_RATE, BreezeTTSClient
    from config import BreezeTTSConfig


class BreezeTTSExtension(AsyncTTS2HttpExtension):
    """Breeze TTS extension.

    All request lifecycle work (audio start/end, TTFB, dumping, cancellation)
    is handled by ``AsyncTTS2HttpExtension``; this class only supplies the
    Breeze config, client and the fixed 24 kHz PCM sample rate.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: BreezeTTSConfig | None = None
        self.client: BreezeTTSClient | None = None
        self._sample_rate: int = SAMPLE_RATE

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        config = BreezeTTSConfig.model_validate_json(config_json_str)
        self._sample_rate = config.params.sample_rate
        return config

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        return BreezeTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        return "breeze"

    def vendor_metadata(self) -> dict:
        if self.config is None:
            return {}
        params = self.config.params
        return {
            "url": params.base_url,
            "voice_id": params.voice_id,
            "language": params.language,
            "speed": params.speed,
        }

    def synthesize_audio_sample_rate(self) -> int:
        return self._sample_rate

