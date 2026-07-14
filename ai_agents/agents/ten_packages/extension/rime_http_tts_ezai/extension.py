#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
EZAI TW TTS Extension

This extension implements text-to-speech using the EZAI TW TTS HTTP API.
It extends the AsyncTTS2HttpExtension for HTTP-based TTS services.
"""

from ten_ai_base.tts2_http import (
    AsyncTTS2HttpExtension,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpClient,
)
from ten_runtime import AsyncTenEnv

from .config import EZAITWTTSConfig
from .ezai_tts import EZAITWTTSClient


class EZAITWTTSExtension(AsyncTTS2HttpExtension):
    """
    EZAI TW TTS Extension implementation.

    Provides Traditional-Chinese text-to-speech synthesis using the EZAI TW
    HTTP API. Inherits all common HTTP TTS functionality from
    AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: EZAITWTTSConfig = None
        self.client: EZAITWTTSClient = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create EZAI TW TTS configuration from JSON string."""
        return EZAITWTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create EZAI TW TTS client."""
        return EZAITWTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "ezai"

    def synthesize_audio_sample_rate(self) -> int:
        """Return the sample rate for synthesized audio."""
        return self.config.sample_rate if self.config else 24000

    def synthesize_audio_channels(self) -> int:
        """Return the number of audio channels."""
        return self.config.channels if self.config else 1

    def synthesize_audio_sample_width(self) -> int:
        """Return the sample width (bytes per sample)."""
        return self.config.sample_width if self.config else 2
