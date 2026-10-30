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

from .config import BreezeBlueTTSConfig
from .breezeblue_tts import BreezeBlueTTSClient


class BreezeBlueTTSExtension(AsyncTTS2HttpExtension):
    """
    BreezeBlue TTS Extension implementation.

    Provides text-to-speech synthesis using BreezeBlue's HTTP streaming API
    (`/v1/text-to-speech/{voice_id}/stream`). Inherits all common HTTP TTS
    functionality from AsyncTTS2HttpExtension.
    """

    def __init__(self, name: str) -> None:
        super().__init__(name)
        # Type hints for better IDE support
        self.config: BreezeBlueTTSConfig = None
        self.client: BreezeBlueTTSClient = None

    # ============================================================
    # Required method implementations
    # ============================================================

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Create BreezeBlue TTS configuration from JSON string."""
        return BreezeBlueTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Create BreezeBlue TTS client."""
        return BreezeBlueTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name."""
        return "breezeblue"

    def synthesize_audio_sample_rate(self) -> int:
        # BreezeBlue streams 24 kHz mono PCM16 regardless of voice or model.
        return 24000
