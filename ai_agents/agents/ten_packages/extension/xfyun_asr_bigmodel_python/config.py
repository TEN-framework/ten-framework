from typing import Dict, Any
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class XfyunASRConfig(BaseModel):
    """Xfyun ASR Bigmodel Configuration"""

    app_id: str = ""
    api_key: str = ""
    api_secret: str = ""
    lang: str = "zh_cn"  # ten use language, zh_cn, en_us
    language: str = "mix"  # api use language zh_cn support zh, en,
    aue: str = "raw"
    accent: str = "mandarin"
    domain: str = "ist_cbm_mix"
    host: str = "ist-api.xfyun.cn"
    sample_rate: int = 16000
    finalize_mode: str = "disconnect"  # "disconnect" or "mute_pkg"
    mute_pkg_duration_ms: int = 1000
    dump: bool = False
    dump_path: str = "/tmp"

    # Xfyun specific parameters
    dwa: str = "wpgs"
    dhw: str = ""
    eos: int = 99999999
    punc: int = 1
    nunum: int = 1
    vto: int = 3000

    params: Dict[str, Any] = Field(default_factory=dict)

    def update(self, params: Dict[str, Any]) -> None:
        """Update configuration with additional parameters."""
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert config to JSON string with optional sensitive data handling."""
        config_dict = self.model_dump()
        if sensitive_handling:
            if self.api_key:
                config_dict["api_key"] = encrypt(config_dict["api_key"])
            if self.api_secret:
                config_dict["api_secret"] = encrypt(config_dict["api_secret"])
            if self.app_id:
                config_dict["app_id"] = encrypt(config_dict["app_id"])
        if config_dict["params"]:
            for key, value in config_dict["params"].items():
                if key == "api_key":
                    config_dict["params"][key] = encrypt(value)
                if key == "api_secret":
                    config_dict["params"][key] = encrypt(value)
                if key == "app_id":
                    config_dict["params"][key] = encrypt(value)
        return str(config_dict)

    @property
    def normalized_language(self):
        if self.lang == "zh_cn":
            return "zh-CN"
        elif self.lang == "en_us":
            return "en-US"
        else:
            return self.lang
