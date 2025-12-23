from typing import Any, Dict, List, Optional

from pydantic import BaseModel, Field


class VibeVoiceTTSConfig(BaseModel):
    url: str = "ws://127.0.0.1:3000/stream"
    cfg_scale: float = 1.5
    steps: Optional[int] = None
    voice: Optional[str] = None
    sample_rate: int = 24000
    channels: int = 1
    sample_width: int = 2
    dump: bool = False
    dump_path: str = ""
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_params: List[str] = Field(default_factory=list)

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params

    def update_params(self) -> None:
        if "url" in self.params:
            self.url = str(self.params["url"])
            del self.params["url"]

        if "cfg_scale" in self.params:
            try:
                self.cfg_scale = float(self.params["cfg_scale"])
                del self.params["cfg_scale"]
            except (TypeError, ValueError):
                del self.params["cfg_scale"]

        if "steps" in self.params:
            try:
                self.steps = int(self.params["steps"])
                del self.params["steps"]
            except (TypeError, ValueError):
                self.steps = None
                del self.params["steps"]

        if "voice" in self.params:
            voice_val = self.params["voice"]
            del self.params["voice"]
            self.voice = str(voice_val) if voice_val is not None else None

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

    def to_str(self, _sensitive_handling: bool = False) -> str:
        return f"{self}"
