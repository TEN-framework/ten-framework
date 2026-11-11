#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from pathlib import Path
from typing import Any

from pydantic import ConfigDict, Field, ValidationError
from typecast.models import OutputStream
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


class TypecastTTSConfig(AsyncTTS2HttpConfig):
    """Typecast TTS config."""

    model_config = ConfigDict(hide_input_in_errors=True)

    dump: bool = Field(default=False, description="Typecast TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "typecast_tts_in.pcm"
        ),
        description="Typecast TTS dump path",
        repr=False,
    )
    url: str = Field(
        default="https://api.typecast.ai",
        description="Typecast API URL",
        repr=False,
    )
    params: dict[str, Any] = Field(
        default_factory=dict,
        description="Typecast TTS params",
        repr=False,
    )
    chunk_size: int = Field(
        default=8192,
        strict=True,
        ge=1,
        description="Maximum bytes read from each Typecast streaming chunk",
    )

    def update_params(self) -> None:
        if "url" in self.params:
            self.url = str(
                self.params.pop("url")  # pylint: disable=no-member
            ).rstrip("/")
        else:
            self.url = self.url.rstrip("/")

        if "model" not in self.params:
            self.params["model"] = "ssfm-v30"

        output = self.params.get("output")  # pylint: disable=no-member
        if output is not None and not isinstance(output, dict):
            raise ValueError("Invalid Typecast output settings")
        output = dict(output or {})
        output["audio_format"] = "wav"
        self.params["output"] = output

    def validate(self) -> None:
        if not self.params.get("api_key"):  # pylint: disable=no-member
            raise ValueError("api_key is required for Typecast TTS")
        if not self.params.get("voice_id"):  # pylint: disable=no-member
            raise ValueError("voice_id is required for Typecast TTS")
        if not self.params.get("model"):  # pylint: disable=no-member
            raise ValueError("model is required for Typecast TTS")
        output = self.params.get("output")  # pylint: disable=no-member
        if output is not None and not isinstance(output, dict):
            raise ValueError("Invalid Typecast output settings")
        output = output or {}
        for name in ("audio_pitch", "audio_tempo", "target_lufs"):
            if isinstance(output.get(name), bool):
                raise ValueError("Invalid Typecast output settings")
        try:
            OutputStream.model_validate(output)
        except ValidationError:
            # SDK validation errors include the rejected input verbatim.
            raise ValueError("Invalid Typecast output settings") from None

    def to_str(self, sensitive_handling: bool = True) -> str:
        if not sensitive_handling:
            return f"{self}"

        return (
            f"TypecastTTSConfig(dump={self.dump}, "
            f"chunk_size={self.chunk_size}, params=[REDACTED])"
        )
