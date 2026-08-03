import copy
from typing import Any

from pydantic import BaseModel, Field
from ten_ai_base import utils


SUPPORTED_PCM_SAMPLE_RATES = frozenset(
    {8000, 16000, 22050, 24000, 44100, 48000}
)

_EXTENSION_PARAM_NAMES = frozenset(
    {
        "api_key",
        "cancel_timeout_ms",
        "dump",
        "dump_path",
        "first_audio_timeout_ms",
        "format",
        "headers",
        "input_idle_timeout_ms",
        "model",
        "pool_size",
        "pool_wait_timeout_ms",
        "sample_rate",
        "task_timeout_ms",
        "url",
        "voice",
        "workspace_id",
        "black_list_params",
    },
)


class CosyTTSConfig(BaseModel):
    # Cosy TTS credentials
    api_key: str = ""  # Cosy TTS API Key

    # TTS specific configs
    model: str = ""  # Model name
    voice: str = ""  # Voice name
    sample_rate: int = 16000  # Audio sample rate
    url: str = ""  # DashScope WebSocket URL
    workspace_id: str = ""
    headers: dict[str, str] = Field(default_factory=dict)
    pool_size: int = 1  # Preconnected synthesizers shared by this worker
    pool_wait_timeout_ms: int = 1000

    # Request lifecycle timeouts.
    first_audio_timeout_ms: int = 5000
    task_timeout_ms: int = 30000
    input_idle_timeout_ms: int = 20000
    cancel_timeout_ms: int = 1000

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
        """Update config attributes from params dictionary."""
        param_names = [
            "api_key",
            "model",
            "sample_rate",
            "voice",
            "url",
            "workspace_id",
            "headers",
            "pool_size",
            "pool_wait_timeout_ms",
            "first_audio_timeout_ms",
            "task_timeout_ms",
            "input_idle_timeout_ms",
            "cancel_timeout_ms",
        ]

        for param_name in param_names:
            if param_name in self.params and not self.is_black_list_params(
                param_name
            ):
                setattr(self, param_name, self.params[param_name])

    def validate_params(self) -> None:
        """Validate required configuration parameters."""
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

        if not 1 <= self.pool_size <= 100:
            raise ValueError("params.pool_size must be between 1 and 100")

        if self.sample_rate not in SUPPORTED_PCM_SAMPLE_RATES:
            supported = ", ".join(
                str(rate) for rate in sorted(SUPPORTED_PCM_SAMPLE_RATES)
            )
            raise ValueError(
                f"params.sample_rate must be one of: {supported}",
            )

        for field_name in (
            "pool_wait_timeout_ms",
            "first_audio_timeout_ms",
            "task_timeout_ms",
            "input_idle_timeout_ms",
            "cancel_timeout_ms",
        ):
            if getattr(self, field_name) <= 0:
                raise ValueError(f"params.{field_name} must be greater than 0")

        if self.input_idle_timeout_ms >= 23000:
            raise ValueError(
                "params.input_idle_timeout_ms must be less than 23000"
            )

        if self.provider_params().get("enable_ssml") is True:
            raise ValueError(
                "params.enable_ssml=true is incompatible with multi-chunk "
                "streaming input",
            )

    def provider_params(self) -> dict[str, Any]:
        """Return task parameters that should be forwarded to DashScope."""
        return {
            key: copy.deepcopy(value)
            for key, value in self.params.items()  # pylint: disable=no-member
            if key not in _EXTENSION_PARAM_NAMES
        }
