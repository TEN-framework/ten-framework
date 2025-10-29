from typing import Any, Dict, List
from pydantic import BaseModel, Field
from dataclasses import dataclass
from ten_ai_base.utils import encrypt


@dataclass
class DeepgramWSASRConfig(BaseModel):
    api_key: str = ""
    url: str = "wss://api.deepgram.com/v1/listen"
    language: str = "en-US"
    model: str = "nova-3"
    sample_rate: int = 16000
    encoding: str = "linear16"
    interim_results: bool = True
    punctuate: bool = True
    # Flux-specific parameters
    eot_threshold: float = 0.7
    eot_timeout_ms: int = 3000
    eager_eot_threshold: float = 0.0  # 0 = disabled
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
        if sensitive_handling and self.api_key:
            config_dict["api_key"] = encrypt(config_dict["api_key"])
        if config_dict["params"]:
            for key, value in config_dict["params"].items():
                if key == "api_key":
                    config_dict["params"][key] = encrypt(value)
        return str(config_dict)

    def is_v2_endpoint(self) -> bool:
        """Detect if we should use v2 API based on URL or model."""
        url_is_v2 = "/v2/" in self.url
        model_is_flux = self.model.startswith("flux")
        return url_is_v2 or model_is_flux
