from typing import Any, Dict, List
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


class SarvamASRConfig(BaseModel):
    api_key: str = ""
    url: str = "wss://api.sarvam.ai/speech-to-text/ws"
    base_url: str = "https://api.sarvam.ai/speech-to-text"
    streaming_url: str = "wss://api.sarvam.ai/speech-to-text/ws"
    language: str = "en-IN"  # BCP-47 language code, e.g., "hi-IN", "en-IN"
    model: str = "saarika:v2.5"  # "saarika:v2.5", "saarika:v2.0", "saaras:v2.5"
    sample_rate: int = 16000
    prompt: str | None = (
        None  # Optional prompt for STT translate (saaras models only)
    )
    dump: bool = False
    dump_path: str = "/tmp"
    params: Dict[str, Any] = Field(default_factory=dict)
    black_list_params: List[str] = Field(
        default_factory=lambda: [
            "sample_rate",
            "channels",
            "encoding",
        ]
    )

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params

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

    def _get_urls_for_model(self) -> tuple[str, str]:
        """Get base URL and streaming URL based on model type."""
        if self.model.startswith("saaras:"):
            return (
                "https://api.sarvam.ai/speech-to-text-translate",
                "wss://api.sarvam.ai/speech-to-text-translate/ws",
            )
        else:  # saarika models
            return (
                "https://api.sarvam.ai/speech-to-text",
                "wss://api.sarvam.ai/speech-to-text/ws",
            )

    def get_streaming_url(self) -> str:
        """Get the streaming URL based on model."""
        if (
            self.streaming_url
            and self.streaming_url != "wss://api.sarvam.ai/speech-to-text/ws"
        ):
            return self.streaming_url
        _, streaming_url = self._get_urls_for_model()
        return streaming_url

    def get_base_url(self) -> str:
        """Get the base URL based on model."""
        if (
            self.base_url
            and self.base_url != "https://api.sarvam.ai/speech-to-text"
        ):
            return self.base_url
        base_url, _ = self._get_urls_for_model()
        return base_url
