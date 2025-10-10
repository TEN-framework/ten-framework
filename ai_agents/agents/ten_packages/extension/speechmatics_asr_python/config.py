#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from typing import Any, Dict, List
from dataclasses import dataclass, field
import copy

from pydantic import BaseModel
from ten_ai_base.utils import encrypt


@dataclass
class SpeechmaticsASRConfig(BaseModel):
    key: str = ""
    chunk_ms: int = 160  # 160ms per chunk
    language: str = "en-US"
    sample_rate: int = 16000
    uri: str = "wss://eu2.rt.speechmatics.com/v2"
    max_delay_mode: str = "flexible"  # "flexible" or "fixed"
    max_delay: float = 0.7  # 0.7 - 4.0
    encoding: str = "pcm_s16le"
    enable_partials: bool = True
    operating_point: str = "enhanced"
    hotwords: List[str] = field(default_factory=list)

    # Diarization configuration
    diarization: str | None = None  # "speaker", "channel", "channel_and_speaker"
    speaker_diarization_config: Dict[str, Any] = field(default_factory=dict)
    channel_diarization_labels: List[str] | None = None

    # True: streaming output final words, False: streaming output final sentences
    enable_word_final_mode: bool = False

    drain_mode: str = "disconnect"  # "disconnect" or "mute_pkg"
    mute_pkg_duration_ms: int = 1500

    dump: bool = False
    dump_path: str = "."

    def to_str(self, sensitive_handling: bool = False) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)
        if config.key:
            config.key = config.key[:4] + "****"
        return f"{config}"

    params: Dict[str, Any] = field(default_factory=dict)
    black_list_params: List[str] = field(default_factory=lambda: [])

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
        if sensitive_handling:
            if self.key:
                config_dict["key"] = encrypt(config_dict["key"])
        if config_dict["params"]:
            for key, value in config_dict["params"].items():
                if key == "key":
                    config_dict["params"][key] = encrypt(value)
        return str(config_dict)
