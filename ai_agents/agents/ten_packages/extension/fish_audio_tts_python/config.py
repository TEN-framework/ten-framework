from typing import Any, Dict
from fish_audio_sdk.apis import Backends
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


class FishAudioTTSConfig(BaseModel):
    api_key: str = ""
    sample_rate: int = 16000
    dump: bool = False
    dump_path: str = "/tmp"
    backend: Backends = "speech-1.5"
    params: Dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        if "api_key" in self.params:
            self.api_key = self.params["api_key"]
            del self.params["api_key"]

        if "sample_rate" in self.params:
            self.sample_rate = int(self.params["sample_rate"])
        else:
            self.params["sample_rate"] = self.sample_rate

        if "format" not in self.params:
            self.params["format"] = "pcm"

        if "references" in self.params:
            del self.params["references"]

        if "mp3_bitrate" in self.params:
            del self.params["mp3_bitrate"]

        if "opus_bitrate" in self.params:
            del self.params["opus_bitrate"]

        if "chunk_length" in self.params:
            del self.params["chunk_length"]

        if "backend" in self.params:
            self.backend = self.params["backend"]
            del self.params["backend"]

        if "text" in self.params:
            del self.params["text"]

    def to_str(self) -> str:
        """
        Convert the configuration to a string representation, masking sensitive data.
        """
        return (
            f"FishAudioTTSConfig(api_key={mask_sensitive_data(self.api_key)}, "
            f"sample_rate={self.sample_rate}, "
            f"backend={self.backend}, "
            f"dump={self.dump}, "
            f"dump_path={self.dump_path}, "
            f"params={self.params}, "
        )
