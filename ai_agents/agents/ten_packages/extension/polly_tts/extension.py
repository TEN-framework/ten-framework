#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
Amazon Polly TTS Extension

This extension implements text-to-speech using the Amazon Polly TTS API.
It extends the AsyncTTS2HttpExtension for HTTP-based TTS services.
"""

from ten_ai_base.tts2_http import (
    AsyncTTS2HttpExtension,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpClient,
)
from ten_runtime import AsyncTenEnv

from .config import PollyTTSConfig
from .polly_tts import PollyTTSClient


class PollyTTSExtension(AsyncTTS2HttpExtension):
    """
    Amazon Polly TTS Extension implementation.

    Provides text-to-speech synthesis using Amazon Polly's API.
    Inherits all common HTTP TTS functionality from AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: PollyTTSConfig = None
        self.client: PollyTTSClient = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create Amazon Polly TTS configuration from JSON string."""
        return PollyTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create Amazon Polly TTS client."""
        return PollyTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "amazon"

    def synthesize_audio_sample_rate(self) -> int:
        """Return the sample rate for synthesized audio."""
        return int(self.config.params.get("sample_rate", "16000"))
