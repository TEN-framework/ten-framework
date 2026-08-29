#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
Aivis TTS Extension

Japanese-first realtime TTS via Aivis Cloud API.
Extends AsyncTTS2HttpExtension for HTTP streaming synthesis.
"""

from ten_ai_base.tts2_http import (
    AsyncTTS2HttpExtension,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpClient,
)
from ten_runtime import AsyncTenEnv

from .config import AivisTTSConfig
from .aivis_tts import AivisTTSClient


class AivisTTSExtension(AsyncTTS2HttpExtension):
    """Aivis Cloud TTS — HTTP WAV stream → PCM for TEN voice graphs."""

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: AivisTTSConfig = None
        self.client: AivisTTSClient = None

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        return AivisTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        return AivisTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        return "aivis"

    def synthesize_audio_sample_rate(self) -> int:
        if self.config is None:
            return 16000
        return int(self.config.params.get("output_sampling_rate", 16000))
