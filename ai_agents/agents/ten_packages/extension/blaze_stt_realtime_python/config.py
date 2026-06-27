"""Configuration model for the Blaze realtime STT TEN extension.

The TEN framework passes the extension's `property.json` as a nested object
under `params`, e.g.::

    {"params": {"api_url": "...", "api_key": "...", "language": "vi", ...}}

This model parses that structure and exposes typed accessors, falling back to
environment variables / sane defaults the same way the standalone
``BlazeRealtimeClient`` does.
"""

import os
from typing import Any, Dict, Optional

from pydantic import BaseModel, Field

from .const import DEFAULT_API_URL, DEFAULT_HANDSHAKE_TIMEOUT


class BlazeSTTRealtimeConfig(BaseModel):
    """Configuration for BlazeSTTRealtimeExtension."""

    # Debugging / dumping
    dump: bool = False
    dump_path: str = "."

    # Connection params (as received from the TEN property's `params` object).
    params: Dict[str, Any] = Field(default_factory=dict)

    # pylint flags pydantic's `Field(...)` default as a `FieldInfo` instance,
    # so it doesn't see the `dict` API on `params`; disable the false positive.
    # pylint: disable=no-member
    @property
    def api_url(self) -> str:
        value = self.params.get("api_url")
        if isinstance(value, str) and value.strip():
            return value
        return os.getenv("BLAZE_STT_API_URL", DEFAULT_API_URL)

    @property
    def api_key(self) -> Optional[str]:
        value = self.params.get("api_key")
        if isinstance(value, str) and value.strip():
            return value
        return os.getenv("BLAZE_STT_API_KEY")

    @property
    def language(self) -> str:
        return self.params.get("language", "vi")

    @property
    def model(self) -> str:
        return self.params.get("model", "stt-stream-1.5")

    @property
    def timeout(self) -> int:
        return int(self.params.get("timeout", 3600))

    @property
    def handshake_timeout(self) -> int:
        """Seconds to wait for the server 'ready' signal during handshake."""
        return int(
            self.params.get("handshake_timeout", DEFAULT_HANDSHAKE_TIMEOUT)
        )

    @property
    def sample_rate(self) -> int:
        return int(self.params.get("sample_rate", 16000))

    @property
    def enable_log(self) -> bool:
        return bool(self.params.get("enable_log", False))

    # pylint: enable=no-member

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Render the config, optionally masking the API key for logging."""
        config_dict = self.model_dump()
        if sensitive_handling and config_dict.get("params"):
            params = config_dict["params"]
            if params.get("api_key"):
                value = str(params["api_key"])
                # Mask everything but the last 4 characters.
                params["api_key"] = "*" * max(0, len(value) - 4) + value[-4:]
        return str(config_dict)
