from typing import Dict, Any
from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


# Default params for nova models (v1 API)
NOVA_DEFAULT_PARAMS = {
    "url": "wss://api.deepgram.com/v1/listen",
    "model": "nova-3",
    "language": "en-US",
    "sample_rate": 16000,
    "encoding": "linear16",
    "interim_results": True,
    "punctuate": True,
    "keep_alive": True,
}

# Default params for flux models (v2 API)
FLUX_DEFAULT_PARAMS = {
    "url": "wss://api.deepgram.com/v2/listen",
    "model": "flux-general-en",
    "sample_rate": 16000,
    "encoding": "linear16",
    "eager_eot_threshold": 0.6,
    "eot_threshold": 0.8,
    "eot_timeout_ms": 700,
    "keep_alive": False,
}


class DeepgramASRConfig(BaseModel):
    """Deepgram ASR Configuration"""

    # Debugging and dumping
    dump: bool = False
    dump_path: str = "/tmp"
    finalize_mode: str = "mute_pkg"  # "flush_api" or "mute_pkg" or "ignore"
    mute_pkg_duration_ms: int = 1000
    # Additional parameters
    params: dict[str, Any] = Field(default_factory=dict)

    def _get_default_params(self, model: str) -> Dict[str, Any]:
        """Get default params based on model type."""
        model_lower = (model or "").lower()
        if "flux" in model_lower:
            return FLUX_DEFAULT_PARAMS.copy()
        else:
            # Default to nova params (includes nova-3, nova-2, etc.)
            return NOVA_DEFAULT_PARAMS.copy()

    def _get_default_finalize_mode(self, model: str) -> str:
        """Get default finalize mode based on model type."""
        model_lower = (model or "").lower()
        if "flux" in model_lower:
            return "ignore"
        else:
            return "flush_api"

    def apply_defaults(self) -> None:
        """Apply default params based on model type."""
        params_dict = self.params or {}
        # Get current model or use default
        current_model = params_dict.get("model", "") or "nova-3"

        # Get defaults for this model type
        defaults = self._get_default_params(current_model)

        # Ensure model is set
        if not params_dict.get("model"):
            params_dict["model"] = current_model

        # Apply defaults for missing params
        for key, value in defaults.items():
            if key not in params_dict or params_dict[key] is None:
                params_dict[key] = value

        self.params = params_dict

        # Set finalize_mode from params or use default based on model type
        if (
            "finalize_mode" in params_dict
            and params_dict["finalize_mode"] is not None
        ):
            self.finalize_mode = params_dict["finalize_mode"]
        else:
            self.finalize_mode = self._get_default_finalize_mode(current_model)

    def update(self, params: Dict[str, Any]) -> None:
        """Update configuration with additional parameters."""
        for key, value in params.items():
            if hasattr(self, key):
                setattr(self, key, value)

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Convert config to JSON string with optional sensitive data handling."""
        config_dict = self.model_dump()
        if sensitive_handling and config_dict["params"]:
            for key, value in config_dict["params"].items():
                if key == "api_key":
                    config_dict["params"][key] = encrypt(value)
                if key == "key":
                    config_dict["params"][key] = encrypt(value)
        return str(config_dict)

    @property
    def is_flux_model(self) -> bool:
        params_dict = self.params or {}
        return "flux" in (params_dict.get("model", "") or "").lower()

    @property
    def normalized_language(self) -> str:
        """Convert language code to normalized format for Deepgram"""
        language_map = {
            "zh": "zh-CN",
            "en": "en-US",
            "ja": "ja-JP",
            "ko": "ko-KR",
            "de": "de-DE",
            "fr": "fr-FR",
            "ru": "ru-RU",
            "es": "es-ES",
            "pt": "pt-PT",
            "it": "it-IT",
            "hi": "hi-IN",
            "ar": "ar-AE",
        }
        params_dict = self.params or {}
        if self.is_flux_model:
            # For flux models, use the 'language' param directly
            language_code = params_dict.get("language", "en-US")
        else:
            language_code = params_dict.get("language", "") or ""
        return language_map.get(language_code, language_code)
