#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import traceback
from typing import AsyncGenerator

from pydantic import BaseModel

from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension

from .bytedance_tts import (
    BytedanceV3Client,
    EVENT_SessionFinished,
    EVENT_TTSResponse,
    EVENT_TTSSentenceEnd,
)
from ten_runtime import (
    AsyncTenEnv,
)


class BytedanceTTSDuplexConfig(BaseModel):
    appid: str
    token: str
    speaker: str = "zh_female_shuangkuaisisi_moon_bigtts"


class BytedanceTTSDuplexExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: BytedanceTTSDuplexConfig = None
        self.client: BytedanceV3Client = None
        self.current_request_id: str = None
        self.stop_event: asyncio.Event = None

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_start(ten_env)
            ten_env.log_debug("on_start")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = BytedanceTTSDuplexConfig.model_validate_json(
                    config_json
                )
                self.ten_env.log_debug(f"config: {self.config}")

                if not self.config.appid:
                    self.ten_env.log_error("get property appid")
                    return ValueError("appid is required")

                if not self.config.token:
                    self.ten_env.log_error("get property token")
                    return ValueError("token is required")

            self.client = BytedanceV3Client(
                app_id=self.config.appid,
                token=self.config.token,
                speaker=self.config.speaker,
                ten_env=ten_env,
            )
        except Exception:
            ten_env.log_error(f"on_start failed: {traceback.format_exc()}")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.client:
            await self.client.close()

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    async def _loop(self) -> None:
        while True:
            try:
                event, audio_data = await self.client.response_msgs.get()
                self.ten_env.log_info(f"Received event: {event}")

                if event == EVENT_TTSResponse:
                    if audio_data is not None:
                        await self.send_tts_audio_data(audio_data)
                    else:
                        self.ten_env.log_error(
                            "Received empty payload for TTS response"
                        )
                elif event == EVENT_SessionFinished:
                    if self.stop_event:
                        self.stop_event.set()
                        self.stop_event = None
                    break

            except Exception as e:
                self.ten_env.log_error(f"Error in _loop: {e}")
                break

    def vendor(self) -> str:
        return "bytedance"

    def synthesize_audio_sample_rate(self) -> int:
        return 24000

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            if t.text.strip() == "":
                self.ten_env.log_info("Received empty text for TTS request")
                return
            if t.request_id != self.current_request_id:
                self.ten_env.log_info(
                    f"New TTS request with ID: {t.request_id}"
                )
                self.current_request_id = t.request_id
                await self.client.finish_session()
                await self.client.finish_connection()
                await self.client.close()
                self.client = BytedanceV3Client(
                    app_id=self.config.appid,
                    token=self.config.token,
                    speaker=self.config.speaker,
                    ten_env=self.ten_env,
                )
                await self.client.connect()

                asyncio.create_task(self._loop())

                await self.client.start_connection()
                await self.client.start_session()

            await self.client.send_text(t.text)

            if t.text_input_end:
                self.ten_env.log_info(
                    f"Received TTS text input end for request ID: {t.request_id}"
                )
                await self.client.finish_session()

                self.stop_event = asyncio.Event()
                await self.stop_event.wait()

                await self.client.finish_connection()
                await self.client.close()
                self.client = None


        except Exception as e:
            self.ten_env.log_error(f"Error in request_tts: {e}")
            # yield b""

        # self.ten_env.log_info("TTS request completed")
