#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
/* [INPUT]: 依赖 ten_ai_base.tts2_http 的 HTTP TTS 基座，依赖 config.py 和 siliconflow_tts.py
 * [OUTPUT]: 对外提供 SiliconFlowTTSExtension 扩展类
 * [POS]: siliconflow_tts2_python 的运行时适配层，负责把 TEN 生命周期接到 SiliconFlow 客户端
 * [PROTOCOL]: 变更时更新此头部，然后检查 AGENT.md
 */
"""

from ten_ai_base.tts2_http import (
    AsyncTTS2HttpClient,
    AsyncTTS2HttpConfig,
    AsyncTTS2HttpExtension,
)
from ten_runtime import AsyncTenEnv

from .config import SiliconFlowTTSConfig
from .siliconflow_tts import SiliconFlowTTSClient


class SiliconFlowTTSExtension(AsyncTTS2HttpExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: SiliconFlowTTSConfig | None = None
        self.client: SiliconFlowTTSClient | None = None

    async def create_config(self, config_json_str: str) -> AsyncTTS2HttpConfig:
        return SiliconFlowTTSConfig.model_validate_json(config_json_str)

    async def create_client(
        self, config: AsyncTTS2HttpConfig, ten_env: AsyncTenEnv
    ) -> AsyncTTS2HttpClient:
        return SiliconFlowTTSClient(config=config, ten_env=ten_env)

    def vendor(self) -> str:
        return "siliconflow"

    def synthesize_audio_sample_rate(self) -> int:
        if self.config is None:
            return 32000
        return self.config.sample_rate

