import copy
from typing import Any

from pydantic import BaseModel, Field
from ten_ai_base import utils


SUPPORTED_PCM_SAMPLE_RATES = frozenset(
    {8000, 16000, 22050, 24000, 44100, 48000}
)


class CosyTTSConfig(BaseModel):
    # Cosy TTS credentials
    api_key: str = ""  # Cosy TTS API Key

    # TTS specific configs
    model: str = ""  # Model name
    voice: str = ""  # Voice name
    format: str = "pcm"
    sample_rate: int = 16000  # Audio sample rate
    url: str = ""  # DashScope WebSocket URL
    workspace_id: str = ""
    headers: dict[str, str] = Field(default_factory=dict)

    # Debug and dump settings
    dump: bool = False
    dump_path: str = "./"

    # Parameters
    # Function reserved, currently empty, may need to add content later
    black_list_params: list[str] = Field(default_factory=list)
    params: dict[str, Any] = Field(default_factory=dict)

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params

    def to_str(self, sensitive_handling: bool = True) -> str:
        """Convert config to string with optional sensitive data handling."""
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)

        # Encrypt sensitive fields
        if config.api_key:
            config.api_key = utils.encrypt(config.api_key)
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])
        for headers in (config.headers, config.params.get("headers", {})):
            if not isinstance(headers, dict):
                continue
            for key in ("Authorization", "authorization", "x-api-key"):
                if key in headers:
                    headers[key] = utils.encrypt(str(headers[key]))

        return f"{config}"

    def update_params(self) -> None:
        """Extract dedicated config fields, leaving provider params behind."""
        # pylint: disable-next=not-an-iterable
        for param_name in self.__class__.model_fields:
            if param_name == "params" or param_name not in self.params:
                continue

            value = self.params.pop(param_name)  # pylint: disable=no-member
            if not self.is_black_list_params(param_name):
                setattr(self, param_name, value)

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
        if "pool_size" in self.params:
            raise ValueError("params.pool_size is managed internally")

        required_fields = [
            "api_key",
            "model",
            "voice",
        ]

        for field_name in required_fields:
            value = getattr(self, field_name)
            match value:
                case str() if value.strip() == "":
                    missing = True
                case None:
                    missing = True
                case _:
                    missing = False
            if missing:
                raise ValueError(
                    f"required fields are missing or empty: params.{field_name}",
                )

        if self.sample_rate not in SUPPORTED_PCM_SAMPLE_RATES:
            supported = ", ".join(
                str(rate) for rate in sorted(SUPPORTED_PCM_SAMPLE_RATES)
            )
            raise ValueError(
                f"params.sample_rate must be one of: {supported}",
            )

        if self.format != "pcm":
            raise ValueError("params.format must be pcm")

        if self.provider_params().get("enable_ssml") is True:
            raise ValueError(
                "params.enable_ssml=true is incompatible with multi-chunk "
                "streaming input",
            )

    def provider_params(self) -> dict[str, Any]:
        """Return task parameters that should be forwarded to DashScope."""
        return copy.deepcopy(self.params)
