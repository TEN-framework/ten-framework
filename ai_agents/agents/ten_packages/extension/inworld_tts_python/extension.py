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

from .config import InworldTTSConfig
from .inworld_tts import InworldTTSClient


class InworldTTSExtension(AsyncTTS2HttpExtension):
    """
    Inworld TTS Extension implementation.

    Provides text-to-speech synthesis using Inworld's TTS 1.5 API.
    Supports models: inworld-tts-1.5-max and inworld-tts-1.5-mini.
    Inherits all common HTTP TTS functionality from AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: InworldTTSConfig = None
        self.client: InworldTTSClient = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create Inworld TTS configuration from JSON string."""
        return InworldTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create Inworld TTS client."""
        return InworldTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "inworld"

    def synthesize_audio_sample_rate(self) -> int:
        """Return the sample rate for synthesized audio."""
        # Use configured sample rate or default to 24000
        if self.config and self.config.params:
            return self.config.params.get("sample_rate", 24000)
        return 24000
