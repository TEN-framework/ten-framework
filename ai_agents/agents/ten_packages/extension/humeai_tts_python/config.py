from typing import Any, Dict
from pydantic import BaseModel, Field


def mask_sensitive_data(
    s: str, unmasked_start: int = 3, unmasked_end: int = 3, mask_char: str = "*"
) -> str:
    if not s or len(s) <= unmasked_start + unmasked_end:
        return mask_char * len(s)

    return (
        s[:unmasked_start]
        + mask_char * (len(s) - unmasked_start - unmasked_end)
        + s[-unmasked_end:]
    )


class HumeAiTTSConfig(BaseModel):
    key: str = ""
    dump: bool = False
    dump_path: str = "/tmp"
    generation_id: str | None = None
    voice_id: str = "daisy"
    voice_name: str = ""
    provider: str = "HUME_VOICE"
    params: Dict[str, Any] = Field(default_factory=dict)
    speed: float = 1.0
    trailing_silence: float = 0.35

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = self.copy(deep=True)
        if config.key:
            config.key = mask_sensitive_data(config.key)
        if config.params and "key" in config.params:
            config.params["key"] = mask_sensitive_data(config.params["key"])
        return f"{config}"

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
