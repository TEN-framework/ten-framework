from ten_runtime import AsyncTenEnv
from ten_ai_base.tts2_http import (
    AsyncTTS2HttpExtension,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpClient,
)
from .config import GeminiTTSConfig
from .gemini_tts import GeminiTTSClient


class GeminiTTSExtension(AsyncTTS2HttpExtension):
    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        """Parse JSON config into Pydantic model"""
        return GeminiTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        """Instantiate Gemini TTS client"""
        return GeminiTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        """Return vendor name for logging/metrics"""
        return "gemini"

    def synthesize_audio_sample_rate(self) -> int:
        """Return fixed sample rate (Gemini = 24000 Hz)"""
        return 24000
