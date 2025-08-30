from typing import Any, Dict
from pydantic import BaseModel, Field
from ten_ai_base import utils


class HumeAiTTSConfig(BaseModel):
    # Hume AI TTS credentials
    key: str = ""

    # Hume AI TTS specific configs
    voice_id: str = ""
    voice_name: str = ""
    provider: str = "HUME_AI"
    speed: float = 1.0
    trailing_silence: float = 0.35
    generation_id: str | None = None

    # Hume AI TTS pass through parameters
    params: Dict[str, Any] = Field(default_factory=dict)

    # Debug and dump settings
    dump: bool = False
    dump_path: str = "/tmp"

    def update_params(self) -> None:
        ##### get value from params #####
        if "key" in self.params:
            self.key = self.params["key"]
            del self.params["key"]
        if "voice_id" in self.params:
            self.voice_id = self.params["voice_id"]
        if "voice_name" in self.params:
            self.voice_name = self.params["voice_name"]
        if "provider" in self.params:
            self.provider = self.params["provider"]
        if "speed" in self.params:
            self.speed = self.params["speed"]
        if "trailing_silence" in self.params:
            self.trailing_silence = self.params["trailing_silence"]
        if "generation_id" in self.params:
            self.generation_id = self.params["generation_id"]

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        required_fields = ["key"]

        for field_name in required_fields:
            value = getattr(self, field_name)
            if not value or (isinstance(value, str) and value.strip() == ""):
                raise ValueError(
                    f"required fields are missing or empty: params.{field_name}"
                )

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"
        config = self.copy(deep=True)
        if config.key:
            config.key = utils.encrypt(config.key)
        return f"{config}"
