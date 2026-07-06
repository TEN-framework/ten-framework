#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_ai_base.tts2_http import (
    AsyncTTS2HttpClient,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpExtension,
)
from ten_runtime import AsyncTenEnv

from .config import TypecastTTSConfig
from .typecast_tts import TYPECAST_STREAM_SAMPLE_RATE, TypecastTTSClient


class TypecastTTSExtension(AsyncTTS2HttpExtension):
    """Typecast TTS extension using Typecast streaming TTS."""

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: TypecastTTSConfig = None
        self.client: TypecastTTSClient = None

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        return TypecastTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        return TypecastTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        return "typecast"

    def synthesize_audio_sample_rate(self) -> int:
        return TYPECAST_STREAM_SAMPLE_RATE
