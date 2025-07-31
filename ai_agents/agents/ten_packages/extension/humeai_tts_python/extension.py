#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import os
import traceback

from ten_ai_base.helper import PCMWriter, generate_file_name
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
)
from ten_ai_base.struct import TTSTextInput, TTSFlush
from ten_ai_base.tts2 import AsyncTTS2BaseExtension

from .config import HumeAiTTSConfig
from .humeTTS import HumeAiTTS, EVENT_TTS_RESPONSE, EVENT_TTS_END, EVENT_TTS_ERROR, EVENT_TTS_INVALID_KEY_ERROR
from ten_runtime import AsyncTenEnv


class HumeaiTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: HumeAiTTSConfig | None = None
        self.client: HumeAiTTS | None = None
        self.recorder: PCMWriter | None = None
        self.sent_ts: datetime | None = None
        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.total_audio_bytes: int = 0
        self.current_request_finished: bool = False

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError("Configuration is empty. Required parameter 'key' is missing.")

            self.config = HumeAiTTSConfig.model_validate_json(config_json_str)
            self.config.update_params()

            ten_env.log_info(f"config: {self.config.to_str(sensitive_handling=True)}")
            if not self.config.key:
                raise ValueError("API key is required")

            self.client = HumeAiTTS(config=self.config, ten_env=ten_env)

            if self.config.dump:
                self.recorder = PCMWriter(
                    os.path.join(self.config.dump_path, generate_file_name("hume_dump"))
                )

        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                "", ModuleError(
                    message=f"Initialization failed: {e}",
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR
                )
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.client:
            self.client.clean()
            self.client = None
        if self.recorder:
            await self.recorder.flush()
            self.recorder = None
        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    def vendor(self) -> str:
        return "humeai"

    def synthesize_audio_sample_rate(self) -> int:
        return 48000 # Hume TTS default sample rate

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0
        bytes_per_sample = 2  # 16-bit PCM
        channels = 1  # Mono
        duration_sec = self.total_audio_bytes / (self.synthesize_audio_sample_rate() * bytes_per_sample * channels)
        return int(duration_sec * 1000)

    async def on_flush(self, t: TTSFlush) -> None:
        if self.client and t.flush_id == self.current_request_id:
            self.ten_env.log_info(f"Flushing TTS for request ID: {t.flush_id}")
            #await self.send_tts_flush_start(t.flush_id, self.current_turn_id)
            await self.client.cancel()
            #await self.send_tts_flush_end(t.flush_id, self.current_turn_id)

    async def request_tts(self, t: TTSTextInput) -> None:
        try:
            if not self.client or not self.config:
                raise RuntimeError("Extension is not initialized properly.")

            if t.request_id != self.current_request_id:
                self.current_request_id = t.request_id
                self.total_audio_bytes = 0
                self.current_request_finished = False
                if t.metadata:
                    self.current_turn_id = t.metadata.get("turn_id", -1)
            elif self.current_request_finished:
                error_msg = f"Received a message for a finished request_id: {self.current_request_id}"
                self.ten_env.log_error(error_msg)
                await self.send_tts_error(
                    self.current_request_id,
                    ModuleError(
                        message=error_msg,
                        module_name=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(vendor=self.vendor())
                    )
                )
                return

            if not t.text or t.text.isspace():
                if t.text_input_end and self.current_request_id:
                    # If it's the end of input, we should still send audio_end
                    await self.send_tts_audio_end(self.current_request_id, 0, 0, self.current_turn_id)
                return

            self.sent_ts = datetime.now()
            first_chunk = True

            async for audio_chunk, event in self.client.get(t.text):
                if event == EVENT_TTS_RESPONSE and audio_chunk:
                    self.total_audio_bytes += len(audio_chunk)

                    if first_chunk and self.sent_ts and self.current_request_id:
                        ttfb = int((datetime.now() - self.sent_ts).total_seconds() * 1000)
                        await self.send_tts_audio_start(self.current_request_id)
                        await self.send_tts_ttfb_metrics(self.current_request_id, ttfb, self.current_turn_id)
                        first_chunk = False

                    if self.config.dump and self.recorder:
                        asyncio.create_task(self.recorder.write(audio_chunk))

                    await self.send_tts_audio_data(audio_chunk)

                elif event == EVENT_TTS_END and self.sent_ts and self.current_request_id:
                    duration_ms = self._calculate_audio_duration_ms()
                    request_interval = int((datetime.now() - self.sent_ts).total_seconds() * 1000)
                    await self.send_tts_audio_end(self.current_request_id, request_interval, duration_ms, self.current_turn_id)
                    self.current_request_finished = True
                    break

                elif event == EVENT_TTS_INVALID_KEY_ERROR:
                    error_msg = audio_chunk.decode('utf-8') if audio_chunk else "Unknown API key error"
                    await self.send_tts_error(
                        self.current_request_id or t.request_id,
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info=ModuleErrorVendorInfo(vendor=self.vendor())
                        )
                    )
                    return

                elif event == EVENT_TTS_ERROR:
                    error_msg = audio_chunk.decode('utf-8') if audio_chunk else "Unknown client error"
                    raise RuntimeError(error_msg)

        except Exception as e:
            self.ten_env.log_error(f"Error in request_tts: {traceback.format_exc()}")
            await self.send_tts_error(
                self.current_request_id or t.request_id,
                ModuleError(
                    message=str(e),
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor())
                )
            )
