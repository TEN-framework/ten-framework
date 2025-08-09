#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import traceback

from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from .elevenlabs_tts import ElevenLabsTTS2, ElevenLabsTTS2Config
from ten_runtime import (
    AsyncTenEnv,
)


class ElevenLabsTTS2Extension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config = None
        self.client = None

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_start(ten_env)
            self.config = await ElevenLabsTTS2Config.create_async(
                ten_env=ten_env
            )

            if not self.config.api_key:
                raise ValueError("api_key is required")

            self.client = ElevenLabsTTS2(self.config)
        except Exception:
            ten_env.log_error(f"on_start failed: {traceback.format_exc()}")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    def vendor(self) -> str:
        return "elevenlabs"

    async def request_tts(self, t: TTSTextInput) -> None:
        audio_stream = self.client.text_to_speech_stream(t.text)
        async for audio_data in audio_stream:
            await self.send_tts_audio_data(audio_data)

    def synthesize_audio_sample_rate(self) -> int:
        return 16000
