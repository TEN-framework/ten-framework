from typing import Any, Dict

from pydantic import BaseModel, Field


class EZAITWTTSConfig(BaseModel):
    url: str = "https://matcha.ezai-k8s.freeddns.org/tts"
    voice: str = "IU_IUF1003"
    denoise: bool = True
    zh_model: str = "nllb"
    sample_rate: int = 24000
    channels: int = 1
    sample_width: int = 2
    dump: bool = False
    dump_path: str = ""
    params: Dict[str, Any] = Field(default_factory=dict)

    def update_params(self) -> None:
        if "url" in self.params:
            self.url = str(self.params["url"])
            del self.params["url"]

        if "voice" in self.params:
            voice_val = self.params["voice"]
            del self.params["voice"]
            self.voice = str(voice_val) if voice_val else ""

        if "sample_rate" in self.params:
            try:
                self.sample_rate = int(self.params["sample_rate"])
                del self.params["sample_rate"]
            except (TypeError, ValueError):
                del self.params["sample_rate"]

        if "channels" in self.params:
            try:
                self.channels = int(self.params["channels"])
                del self.params["channels"]
            except (TypeError, ValueError):
                del self.params["channels"]

        if "sample_width" in self.params:
            try:
                self.sample_width = int(self.params["sample_width"])
                del self.params["sample_width"]
            except (TypeError, ValueError):
                del self.params["sample_width"]

    def to_str(self) -> str:
        return f"{self}"
