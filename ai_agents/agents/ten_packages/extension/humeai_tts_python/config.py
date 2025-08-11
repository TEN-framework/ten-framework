from typing import Any, Dict
import copy
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
    key: str
    adjust_volume: int = 1000
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
        # This function allows overriding default config values with 'params' from property.json
        for key, value in self.params.items():
            if hasattr(self, key):
                setattr(self, key, value)