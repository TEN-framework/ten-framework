#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any

from ten_ai_base.tts2_http import (
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpClient,
    AsyncTTS2HttpExtension,
)
from ten_runtime import AsyncTenEnv

from .client import GmicloudTTSClient, GmicloudTTSOptions
from .config import GmicloudTTS2Config


class GmicloudTTS2Extension(AsyncTTS2HttpExtension):
    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        config = GmicloudTTS2Config.model_validate_json(config_json_str)
        config.update_params()
        config.validate()
        return config

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        cfg: GmicloudTTS2Config = config  # type: ignore[assignment]
        return GmicloudTTSClient(
            GmicloudTTSOptions(
                api_key=cfg.api_key,
                base_url=cfg.base_url,
                model=cfg.model,
                voice=cfg.voice,
                audio_format=cfg.audio_format,
                speed=cfg.speed,
                temperature=cfg.temperature,
                sample_rate=cfg.sample_rate,
                text_chunk_chars=cfg.text_chunk_chars,
                request_timeout_sec=cfg.request_timeout_sec,
                poll_interval_sec=cfg.poll_interval_sec,
                poll_attempts=cfg.poll_attempts,
            )
        )

    def vendor(self) -> str:
        return "gmicloud"

    def vendor_metadata(self) -> dict[str, Any]:
        if self.config is None:
            return {}
        config: GmicloudTTS2Config = self.config  # type: ignore[assignment]
        return {
            "model": config.model,
            "voice": config.voice or "",
            "base_url": config.base_url,
        }

    def synthesize_audio_sample_rate(self) -> int:
        config: GmicloudTTS2Config | None = self.config  # type: ignore[assignment]
        return config.sample_rate if config else 24000

    def synthesize_audio_sample_width(self) -> int:
        return 2

    def synthesize_audio_channels(self) -> int:
        config: GmicloudTTS2Config | None = self.config  # type: ignore[assignment]
        return config.channels if config else 1
