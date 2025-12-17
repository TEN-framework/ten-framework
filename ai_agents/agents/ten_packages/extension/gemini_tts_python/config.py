import copy
from typing import Any
from pydantic import Field
from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class GeminiTTSConfig(AsyncTTS2HttpConfig):
    dump: bool = Field(default=False)
    dump_path: str = Field(default="./")
    params: dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        """Transform params for Gemini API compatibility"""
        # Ensure model is set (default to Flash for low latency)
        if "model" not in self.params:
            self.params["model"] = "gemini-2.5-flash-preview-tts"

        # Ensure voice is set
        if "voice" not in self.params:
            self.params["voice"] = "Kore"

        # Set default language code
        if "language_code" not in self.params:
            self.params["language_code"] = "en-US"

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Safe logging with encrypted sensitive fields"""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])
        return f"{config}"

    def validate(self) -> None:
        """Validate required fields"""
        if "api_key" not in self.params or not self.params["api_key"]:
            raise ValueError("API key is required for Gemini TTS")
        if "model" not in self.params or not self.params["model"]:
            raise ValueError("Model is required for Gemini TTS")
