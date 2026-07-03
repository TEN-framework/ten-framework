"""
Configuration module for Gradium ASR extension.
"""

import json
from pydantic import BaseModel, Field
from typing import Any, Dict, Literal


class GradiumASRConfig(BaseModel):
    """Configuration for Gradium ASR extension."""

    api_key: str = ""
    """API key for Gradium API authentication."""

    region: Literal["eu", "us"] = "us"
    """Gradium API region. Options: 'eu' (Europe) or 'us' (USA)."""

    model_name: str = "default"
    """Name of the Gradium ASR model to use."""

    input_format: Literal["pcm", "wav", "opus"] = "pcm"
    """Audio input format. Options: 'pcm', 'wav', 'opus'."""

    sample_rate: int = 24000
    """Audio sample rate in Hz. Gradium expects 24kHz."""

    channels: int = 1
    """Number of audio channels. Gradium expects mono (1 channel)."""

    bits_per_sample: int = 16
    """Bits per sample. Gradium expects 16-bit PCM."""

    language: str = ""
    """Language code for transcription (if supported by model)."""

    dump: bool = False
    """Enable audio dumping for debugging."""

    dump_path: str = "/tmp"
    """Path to dump audio files when debugging."""

    params: Dict[str, Any] = Field(default_factory=dict)
    """Additional parameters from property.json."""

    def update(self, params: Dict[str, Any]) -> None:
        """
        Update configuration with additional parameters.

        Args:
            params: Dictionary of parameters to update.
        """
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def to_json(self, sensitive_handling: bool = False) -> str:
        """
        Convert configuration to JSON string.

        Args:
            sensitive_handling: If True, mask sensitive information like API keys.

        Returns:
            JSON string representation of the configuration.
        """
        data = self.model_dump()
        if sensitive_handling and data.get("api_key"):
            data["api_key"] = "***"
        return json.dumps(data, indent=2)

    def get_websocket_url(self) -> str:
        """
        Get the WebSocket URL based on the region.

        Returns:
            WebSocket URL for the specified region.
        """
        if self.region == "eu":
            return "wss://eu.api.gradium.ai/api/speech/asr"
        else:
            return "wss://us.api.gradium.ai/api/speech/asr"
