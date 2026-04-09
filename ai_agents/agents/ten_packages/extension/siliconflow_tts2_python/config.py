#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
/* [INPUT]: 依赖 pydantic 的 Field，依赖 ten_ai_base.tts2_http 的 AsyncTTS2HttpConfig
 * [OUTPUT]: 对外提供 SiliconFlowTTSConfig 配置模型和参数校验能力
 * [POS]: siliconflow_tts2_python 的配置归一化层，给 extension/client 提供单一真相源
 * [PROTOCOL]: 变更时更新此头部，然后检查 AGENT.md
 */
"""

from pathlib import Path
from typing import Any
import copy

from pydantic import Field

from ten_ai_base import utils
from ten_ai_base.tts2_http import AsyncTTS2HttpConfig


SUPPORTED_RESPONSE_FORMATS = {"wav", "pcm", "mp3"}


class SiliconFlowTTSConfig(AsyncTTS2HttpConfig):
    dump: bool = Field(default=False, description="SiliconFlow TTS dump")
    dump_path: str = Field(
        default_factory=lambda: str(
            Path(__file__).parent / "siliconflow_tts_in.pcm"
        ),
        description="SiliconFlow TTS dump path",
    )
    params: dict[str, Any] = Field(
        default_factory=dict, description="SiliconFlow TTS params"
    )
    sample_rate: int = Field(default=32000, description="PCM sample rate")

    def update_params(self) -> None:
        self.params.pop("input", None)
        self.params["stream"] = True
        self.params.setdefault("base_url", "https://api.siliconflow.cn/v1")
        self.params.setdefault("model", "IndexTeam/IndexTTS-2")
        self.params.setdefault("voice", "IndexTeam/IndexTTS-2:anna")
        self.params.setdefault("max_tokens", 2048)
        self.params.setdefault("speed", 1)
        self.params.setdefault("gain", 0)
        self.params.setdefault("response_format", "mp3")

        if "sample_rate" in self.params:
            self.sample_rate = int(self.params["sample_rate"])
        else:
            self.params["sample_rate"] = self.sample_rate

    def to_str(self, sensitive_handling: bool = True) -> str:
        if not sensitive_handling:
            return f"{self}"

        config = copy.deepcopy(self)
        if config.params and "api_key" in config.params:
            config.params["api_key"] = utils.encrypt(config.params["api_key"])
        return f"{config}"

    def validate(self) -> None:
        if "api_key" not in self.params or not self.params["api_key"]:
            raise ValueError("API key is required for SiliconFlow TTS")
        if "model" not in self.params or not self.params["model"]:
            raise ValueError("Model is required for SiliconFlow TTS")
        if "voice" not in self.params or not self.params["voice"]:
            raise ValueError("Voice is required for SiliconFlow TTS")

        response_format = str(self.params.get("response_format", "wav")).lower()
        if response_format not in SUPPORTED_RESPONSE_FORMATS:
            raise ValueError(
                "SiliconFlow TTS in TEN only supports 'wav', 'pcm' or 'mp3' "
                "response_format"
            )

        if self.sample_rate <= 0:
            raise ValueError("sample_rate must be a positive integer")
