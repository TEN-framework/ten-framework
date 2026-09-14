import json
from typing import Any
from urllib.parse import urlsplit, urlunsplit

from pydantic import BaseModel, Field, model_validator

MODEL_LANGUAGES = {
    "sommers_ko": "ko-KR",
    "sommers_ja": "ja-JP",
    "sommers_en": "en-US",
}
CONNECTION_PARAMS = {"client_id", "client_secret", "api_base", "websocket_url"}


class RTZRASRConfig(BaseModel):
    params: dict[str, Any] = Field(default_factory=dict)
    dump: bool = False
    dump_path: str = "/tmp"
    finalize_timeout: float = Field(default=5.0, gt=0, le=60)

    @model_validator(mode="after")
    def validate_params(self):
        self.params = {
            "api_base": "https://openapi.vito.ai",
            "model_name": "sommers_ko",
            "sample_rate": 16000,
            "encoding": "LINEAR16",
            **self.params,
        }
        for key in ("client_id", "client_secret"):
            if (
                not isinstance(self.params.get(key), str)
                or not self.params[key].strip()
            ):
                raise ValueError(f"params.{key} is required")
        if (
            not isinstance(self.params["model_name"], str)
            or self.params["model_name"] not in MODEL_LANGUAGES
        ):
            raise ValueError("unsupported RTZR model_name")
        rate = self.params["sample_rate"]
        if type(rate) is not int or not 8000 <= rate <= 48000:
            raise ValueError("sample_rate must be an integer in [8000, 48000]")
        if self.params["encoding"] != "LINEAR16":
            raise ValueError("TEN audio requires LINEAR16 encoding")
        self.params["api_base"] = self._base_url(
            self.params["api_base"], {"http", "https"}
        )
        websocket_url = self.params.get("websocket_url")
        if not websocket_url:
            parsed = urlsplit(self.params["api_base"])
            websocket_url = urlunsplit(
                parsed._replace(
                    scheme="wss" if parsed.scheme == "https" else "ws"
                )
            )
        self.params["websocket_url"] = self._base_url(
            websocket_url, {"ws", "wss"}
        )
        return self

    @staticmethod
    def _base_url(value: str, schemes: set[str]) -> str:
        if not isinstance(value, str):
            raise ValueError("RTZR base URL must be a string")
        parsed = urlsplit(value)
        if (
            parsed.scheme not in schemes
            or not parsed.hostname
            or parsed.username is not None
            or parsed.password is not None
            or parsed.query
            or parsed.fragment
        ):
            raise ValueError("invalid RTZR base URL")
        _ = parsed.port
        return value.rstrip("/")

    @property
    def sample_rate(self) -> int:
        return self.params["sample_rate"]

    @property
    def language(self) -> str:
        return MODEL_LANGUAGES[self.params["model_name"]]

    def query_params(self) -> dict[str, str]:
        return {
            key: str(value).lower() if isinstance(value, bool) else str(value)
            for key, value in self.params.items()
            if key not in CONNECTION_PARAMS and value is not None
        }

    def to_str(self) -> str:
        payload = self.model_dump()
        for key in ("client_id", "client_secret"):
            payload["params"][key] = "***"
        return json.dumps(payload, ensure_ascii=False)
