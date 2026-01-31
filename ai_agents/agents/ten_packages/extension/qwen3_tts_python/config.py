#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import Any, Dict, List, Literal
from pydantic import BaseModel, Field


class Qwen3TTSConfig(BaseModel):
    dump: bool = Field(
        default=False, description="Enable audio dumping for debugging"
    )
    dump_path: str = Field(default="./", description="Path to dump audio files")
    sample_rate: int = Field(
        default=24000, description="Audio sample rate in Hz"
    )
    params: Dict[str, Any] = Field(
        default_factory=dict, description="TTS parameters"
    )
    black_list_keys: List[str] = Field(
        default_factory=list, description="Keys to exclude from params"
    )

    # Extracted params for easier access
    model: str = Field(
        default="Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice",
        description="Qwen3 TTS model name",
    )
    language: str = Field(
        default="English",
        description="Language for TTS (Chinese, English, Japanese, Korean, German, French, Russian, Portuguese, Spanish, Italian)",
    )
    speaker: str = Field(
        default="Vivian",
        description="Speaker voice (Vivian, Serena, Uncle_Fu, Dylan, Eric, Ryan, Aiden, Ono_Anna, Sohee)",
    )
    instruct: str = Field(
        default="", description="Optional instruction for tone/emotion control"
    )
    mode: Literal["custom_voice", "voice_clone", "voice_design"] = Field(
        default="custom_voice",
        description="TTS mode: custom_voice, voice_clone, or voice_design",
    )
    ref_audio_path: str = Field(
        default="", description="Reference audio path for voice cloning"
    )
    ref_text: str = Field(
        default="", description="Reference audio transcript for voice cloning"
    )
    voice_description: str = Field(
        default="",
        description="Natural language voice description for voice_design mode",
    )
    device: str = Field(
        default="cuda:0",
        description="Device for model inference (e.g., cuda:0, cpu)",
    )
    dtype: str = Field(
        default="bfloat16",
        description="Data type for model (bfloat16, float16)",
    )
    attn_implementation: str = Field(
        default="flash_attention_2",
        description="Attention implementation (flash_attention_2 for optimized inference)",
    )

    def to_str(self, _sensitive_handling: bool = False) -> str:
        return str(self.model_dump())

    def update_params(self) -> None:
        """Update config fields from params dict"""
        # pylint: disable=no-member
        for key, value in self.params.items():
            if hasattr(self, key) and value is not None and value != "":
                setattr(self, key, value)

        # Delete blacklisted keys
        for key in self.black_list_keys:
            if key in self.params:
                del self.params[key]
