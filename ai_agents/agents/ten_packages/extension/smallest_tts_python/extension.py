#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_ai_base.tts2_http import (
    AsyncTTS2HttpExtension,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpClient,
)
from ten_runtime import AsyncTenEnv

from .config import SmallestTTSConfig, DEFAULT_SAMPLE_RATE
from .smallest_tts import SmallestTTSClient


class SmallestTTSExtension(AsyncTTS2HttpExtension):
    """
    Smallest AI (Lightning) TTS Extension implementation.

    Provides text-to-speech synthesis using Smallest AI's Lightning models
    over the SSE streaming endpoint (`/waves/v1/tts/live`). Inherits all
    common HTTP TTS functionality from AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: SmallestTTSConfig = None
        self.client: SmallestTTSClient = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create Smallest TTS configuration from JSON string."""
        return SmallestTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create Smallest TTS client."""
        return SmallestTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "smallest"

    def synthesize_audio_sample_rate(self) -> int:
        # Lightning emits PCM16 mono at the requested sample_rate
        # (8000-44100 Hz, 24000 recommended).
        if self.config and self.config.params.get("sample_rate"):
            return int(self.config.params["sample_rate"])
        return DEFAULT_SAMPLE_RATE
