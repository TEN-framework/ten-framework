import json
from typing import Dict, Any
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class XfyunDialectASRConfig(BaseModel):
    """Xfyun ASR Configuration for new dialect API"""
    app_id: str = ""
    access_key_id: str = ""
    access_key_secret: str = ""
    language: str = "en-US"

    host: str = "office-api-ast-dx.iflyaisol.com"

    lang: str = "autodialect"  # Languages to recognize when lang is autominor
    audio_encode: str = "pcm"  # Audio encoding format
    samplerate: int = 16000  # Sample rate
    codec: str = "pcm"


    # Engine optimization parameters
    multiFuncData: str = "false"
    use_tts: str = "false"
    nrtMode: str = "true"

    # Legacy parameters for compatibility
    finalize_mode: str = "disconnect"  # "disconnect" or "mute_pkg"
    mute_pkg_duration_ms: int = 1000
    dump: bool = False
    dump_path: str = "/tmp"

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
            if self.access_key_id:
                config_dict["access_key_id"] = encrypt(config_dict["access_key_id"])
            if self.access_key_secret:
                config_dict["access_key_secret"] = encrypt(config_dict["access_key_secret"])
            if self.app_id:
                config_dict["app_id"] = encrypt(config_dict["app_id"])
        if config_dict["params"]:
            for key, value in config_dict["params"].items():
                if key == "access_key_id":
                    config_dict["params"][key] = encrypt(value)
                if key == "access_key_secret":
                    config_dict["params"][key] = encrypt(value)
                if key == "app_id":
                    config_dict["params"][key] = encrypt(value)
        return str(config_dict)
