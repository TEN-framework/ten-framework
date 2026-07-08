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

from .config import MistralTTSConfig
from .mistral_tts import MistralTTSClient


class MistralTTSExtension(AsyncTTS2HttpExtension):
    """
    Mistral (Voxtral) TTS Extension implementation.

    Provides text-to-speech synthesis using Mistral's OpenAI-compatible HTTP
    API (`/v1/audio/speech`, model family `voxtral-*-tts`). Inherits all common
    HTTP TTS functionality from AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: MistralTTSConfig = None
        self.client: MistralTTSClient = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create Mistral TTS configuration from JSON string."""
        return MistralTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create Mistral TTS client."""
        return MistralTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "mistral"

    def synthesize_audio_sample_rate(self) -> int:
        # Voxtral TTS emits 24 kHz audio. The client converts the vendor's raw
        # float32 PCM stream to PCM16 mono at this rate.
        return 24000
