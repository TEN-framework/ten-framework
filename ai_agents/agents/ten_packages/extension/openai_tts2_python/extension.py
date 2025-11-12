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

from .config import OpenAITTSConfig
from .openai_tts import OpenAITTSClient


class OpenAITTSExtension(AsyncTTS2HttpExtension):
    """
    OpenAI TTS Extension implementation.

    Provides text-to-speech synthesis using OpenAI's HTTP API.
    Inherits all common HTTP TTS functionality from AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: OpenAITTSConfig = None
        self.client: OpenAITTSClient = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create OpenAI TTS configuration from JSON string."""
        return OpenAITTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create OpenAI TTS client."""
        return OpenAITTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "openai"

    def synthesize_audio_sample_rate(self) -> int:
        """Return the sample rate for synthesized audio."""
        # OpenAI TTS fixed sample rate
        return 24000
