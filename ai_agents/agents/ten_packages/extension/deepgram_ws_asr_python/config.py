from typing import Any, Dict
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
    # v1 API parameters (Nova models)
    endpointing: int = (
        300  # Silence duration in ms before finalizing transcript
    )
    utterance_end_ms: int = 1000  # Max silence before ending utterance
    # Flux-specific parameters (v2 API)
    eot_threshold: float = 0.7  # End-of-turn probability threshold (0.0-1.0)
    eot_timeout_ms: int = 3000  # Max time to wait for EOT confirmation (ms)
    eager_eot_threshold: float = 0.0  # Eager EOT threshold (0 = disabled)
    # Confidence threshold for interim results (filter noise)
    min_interim_confidence: float = (
        0.5  # Minimum confidence to accept interim results (0.0-1.0)
    )
    # Interrupt settings - send flush on user speech detection
    interrupt_on_speech: bool = True  # Enable interrupt when user speaks
    interrupt_min_confidence: float = (
        0.7  # Min confidence to trigger interrupt (0.0-1.0)
    )
    interrupt_min_words: int = 1  # Min word count to trigger interrupt
    # Echo cancel settling - filter short utterances at session start
    echo_cancel_duration: float = 5.0  # Duration in seconds
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
