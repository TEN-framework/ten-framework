import copy
from typing import Any, Dict

from pydantic import BaseModel, Field
from ten_ai_base import utils

class EZAITWTTSConfig(BaseModel):
    url: str = "https://matcha.ezai-k8s.freeddns.org/tts"
    voice: str = "IU_IUF1003"
    denoise: bool = True
    speed: float = 0.8
    zh_model: str = "nllb"
    sample_rate: int = 24000
    channels: int = 1
    sample_width: int = 2
    dump: bool = False
    dump_path: str = ""
    api_key: str = ""
    params: Dict[str, Any] = Field(default_factory=dict)

    def update_param(self, key, dtype) -> None:
        if key in self.params:
            try:
                val = dtype(self.params[key]) if dtype else self.params[key]
                if key == 'voice' and not val.strip():
                    val = "IU_IUF1003"
                if key == 'speed' and float(val) < 0.1:
                    val = 0.8
                setattr(self, key, val)
            except:
                pass
            finally:
                del self.params[key]


    def update_params(self) -> None:
        for k, d in [
            ('url', str),
            ('voice', str),
            ('zh_model', str),
            ('speed', int),
            ('channels', int),
            ('sample_rate', int),
            ('sample_width', int),
            ('denoise', bool),
            ('api_key', str),
        ]:
            self.update_param(k, d)

    def to_str(self, sensitive_handling=True) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields in params
        if config.params:
            if "api_key" in config.params:
                config.params["api_key"] = utils.encrypt(
                    config.params["api_key"]
                )
        return f"{config}"
