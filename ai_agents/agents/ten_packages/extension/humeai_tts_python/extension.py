#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
Hume AI TTS Extension

This extension implements text-to-speech using the Hume AI TTS API.
It extends the AsyncTTS2HttpExtension for HTTP-based TTS services.
"""

from ten_ai_base.tts2_http import (
    AsyncTTS2HttpExtension,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpClient,
)
from ten_runtime import AsyncTenEnv

from .config import HumeAiTTSConfig
from .humeTTS import HumeAiTTS


class HumeaiTTSExtension(AsyncTTS2HttpExtension):
    """
    Hume AI TTS Extension implementation.

    Provides text-to-speech synthesis using Hume AI's HTTP API.
    Inherits all common HTTP TTS functionality from AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: HumeAiTTSConfig = None
        self.client: HumeAiTTS = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create Hume AI TTS configuration from JSON string."""
        return HumeAiTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create Hume AI TTS client."""
        return HumeAiTTS(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "humeai"

    def synthesize_audio_sample_rate(self) -> int:
        """Return the sample rate for synthesized audio."""
        return 48000  # Hume TTS default sample rate
