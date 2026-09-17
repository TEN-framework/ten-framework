#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
Speechmatics TTS Extension

This extension implements text-to-speech using Speechmatics TTS API.
It provides low-latency, high-quality speech synthesis.
"""

from ten_ai_base.tts2_http import (
    AsyncTTS2HttpExtension,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpClient,
)
from ten_runtime import AsyncTenEnv

from .config import SpeechmaticsTTSConfig
from .speechmatics_tts import SpeechmaticsTTSClient


class SpeechmaticsTTSExtension(AsyncTTS2HttpExtension):
    """
    Speechmatics TTS Extension implementation.

    Provides text-to-speech synthesis using Speechmatics HTTP API.
    Inherits all common HTTP TTS functionality from AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: SpeechmaticsTTSConfig = None
        self.client: SpeechmaticsTTSClient = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create Speechmatics TTS configuration from JSON string."""
        return SpeechmaticsTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create Speechmatics TTS client."""
        return SpeechmaticsTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "speechmatics"

    def synthesize_audio_sample_rate(self) -> int:
        """Return the sample rate for synthesized audio."""
        return self.config.params.get("sample_rate", 16000)
