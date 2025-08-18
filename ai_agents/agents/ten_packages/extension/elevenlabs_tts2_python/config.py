from typing import Any, Dict, List
from pydantic import BaseModel


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


class ElevenLabsTTS2Config(BaseModel):
    api_key: str = ""
    model_id: str = "eleven_multilingual_v2"
    optimize_streaming_latency: int = 0
    similarity_boost: float = 0.75
    speaker_boost: bool = False
    sample_rate: int = 16000
    stability: float = 0.5
    request_timeout_seconds: int = 10
    style: float = 0.0
    voice_id: str = "pNInz6obpgDQGcFmaJgB"
    dump: bool = False
    dump_path: str = "./"
    params: Dict[str, Any] = {}
    black_list_keys: List[str] = ["api_key"]

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = self.copy(deep=True)
        if config.api_key:
            config.api_key = mask_sensitive_data(config.api_key)
        return f"{config}"

    def update_params(self) -> None:
        # This function allows overriding default config values with 'params' from property.json
        # pylint: disable=no-member

        for key, value in self.params.items():
            if hasattr(self, key):
                setattr(self, key, value)

        # Delete keys after iteration is complete
        for key in self.black_list_keys:
            if key in self.params:
                del self.params[key]
