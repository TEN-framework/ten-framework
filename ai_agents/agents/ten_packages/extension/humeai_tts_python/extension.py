#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import os
import traceback

from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR

from .config import HumeAiTTSConfig
from .humeTTS import (
    HumeAiTTS,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_INVALID_KEY_ERROR,
)
from ten_runtime import AsyncTenEnv, Data


class HumeaiTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: HumeAiTTSConfig | None = None
        self.client: HumeAiTTS | None = None
        self.sent_ts: datetime | None = None
        self.current_request_id: str | None = None
        self.total_audio_bytes: int = 0
        self.first_chunk: bool = False
        self.current_request_finished: bool = False
        self.recorder_map: dict[str, PCMWriter] = (
            {}
        )  # Store PCMWriter instances for different request_ids

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json, _ = await self.ten_env.get_property_to_json("")

            self.config = HumeAiTTSConfig.model_validate_json(config_json)
            self.config.update_params()
            self.config.validate_params()

            ten_env.log_info(
                f"config: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            self.client = HumeAiTTS(config=self.config, ten_env=ten_env)

        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                "",
                ModuleError(
                    message=f"Initialization failed: {e}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.client:
            self.client.clean()
            self.client = None

        # Clean up all PCMWriters
        for request_id, recorder in self.recorder_map.items():
            try:
                await recorder.flush()
                ten_env.log_info(
                    f"Flushed PCMWriter for request_id: {request_id}"
                )
            except Exception as e:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {e}"
                )

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    def vendor(self) -> str:
        return "humeai"

    def synthesize_audio_sample_rate(self) -> int:
        return 48000  # Hume TTS default sample rate

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0
        bytes_per_sample = 2  # 16-bit PCM
        channels = 1  # Mono
        duration_sec = self.total_audio_bytes / (
            self.synthesize_audio_sample_rate() * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"on_data: {data_name}")

        if data_name == "tts_flush":
            flush_id, _ = data.get_property_string("flush_id")
            if flush_id:
                ten_env.log_info(f"Received flush request for ID: {flush_id}")

                # Cancel the TTS client first and wait for it to complete
                if self.client:
                    await self.client.cancel()

                if self.current_request_id:
                    ten_env.log_info(
                        f"Current request {self.current_request_id} is being flushed. Sending INTERRUPTED."
                    )

                    # Then send the interrupted event
                    if self.sent_ts:
                        request_event_interval = int(
                            (datetime.now() - self.sent_ts).total_seconds()
                            * 1000
                        )
                        duration_ms = self._calculate_audio_duration_ms()
                        await self.send_tts_audio_end(
                            request_id=self.current_request_id,
                            request_event_interval_ms=request_event_interval,
                            request_total_audio_duration_ms=duration_ms,
                            reason=TTSAudioEndReason.INTERRUPTED,
                        )
                        self.sent_ts = None
                        self.current_request_finished = True
        await super().on_data(ten_env, data)

    async def request_tts(self, t: TTSTextInput) -> None:
        try:
            if not self.client or not self.config:
                raise RuntimeError("Extension is not initialized properly.")

            self.ten_env.log_info(
                f"KEYPOINT request_tts: {t.text}, request_id: {t.request_id}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            if t.request_id != self.current_request_id:
                self.first_chunk = True
                self.sent_ts = datetime.now()
                self.current_request_id = t.request_id
                self.total_audio_bytes = 0
                self.current_request_finished = False

                # Create new PCMWriter for new request_id and clean up old ones
                if self.config and self.config.dump:
                    # Clean up old PCMWriters (except current request_id)
                    old_request_ids = [
                        rid
                        for rid in self.recorder_map.keys()
                        if rid != t.request_id
                    ]
                    for old_rid in old_request_ids:
                        try:
                            await self.recorder_map[old_rid].flush()
                            del self.recorder_map[old_rid]
                            self.ten_env.log_info(
                                f"Cleaned up old PCMWriter for request_id: {old_rid}"
                            )
                        except Exception as e:
                            self.ten_env.log_error(
                                f"Error cleaning up PCMWriter for request_id {old_rid}: {e}"
                            )

                    # Create new PCMWriter
                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"hume_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )
                        self.ten_env.log_info(
                            f"Created PCMWriter for request_id: {t.request_id}, file: {dump_file_path}"
                        )
            elif self.current_request_finished:
                error_msg = f"Received a message for a finished request_id: {self.current_request_id}"
                self.ten_env.log_error(error_msg)
                return

            async for audio_chunk, event in self.client.get(
                t.text, t.request_id
            ):
                if event == EVENT_TTS_RESPONSE and audio_chunk:
                    self.total_audio_bytes += len(audio_chunk)

                    self.ten_env.log_debug(
                        f"receive_audio: request_id: {t.request_id}",
                        category=LOG_CATEGORY_VENDOR,
                    )

                    if (
                        self.first_chunk
                        and self.sent_ts
                        and self.current_request_id
                    ):
                        ttfb = int(
                            (datetime.now() - self.sent_ts).total_seconds()
                            * 1000
                        )
                        await self.send_tts_audio_start(self.current_request_id)
                        await self.send_tts_ttfb_metrics(
                            request_id=self.current_request_id,
                            ttfb_ms=ttfb,
                            extra_metadata={
                                "voice_id": (
                                    self.config.voice_id if self.config else ""
                                ),
                                "voice_name": (
                                    self.config.voice_name
                                    if self.config
                                    else ""
                                ),
                            },
                        )
                        self.first_chunk = False

                    if (
                        self.config.dump
                        and self.current_request_id
                        and self.current_request_id in self.recorder_map
                    ):
                        asyncio.create_task(
                            self.recorder_map[self.current_request_id].write(
                                audio_chunk
                            )
                        )

                    await self.send_tts_audio_data(audio_chunk)

                elif (
                    event == EVENT_TTS_END
                    and self.sent_ts
                    and self.current_request_id
                    and t.text_input_end
                ):
                    duration_ms = self._calculate_audio_duration_ms()
                    request_interval = int(
                        (datetime.now() - self.sent_ts).total_seconds() * 1000
                    )
                    await self.send_tts_audio_end(
                        request_id=self.current_request_id,
                        request_event_interval_ms=request_interval,
                        request_total_audio_duration_ms=duration_ms,
                    )
                    self.current_request_id = None
                    break

                elif event == EVENT_TTS_INVALID_KEY_ERROR:
                    error_msg = (
                        audio_chunk.decode("utf-8")
                        if audio_chunk
                        else "Unknown API key error"
                    )
                    await self.send_tts_error(
                        self.current_request_id or t.request_id,
                        ModuleError(
                            message=error_msg,
                            module=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR.value,
                            vendor_info=ModuleErrorVendorInfo(
                                vendor=self.vendor()
                            ),
                        ),
                    )
                    return

                elif event == EVENT_TTS_ERROR:
                    error_msg = (
                        audio_chunk.decode("utf-8")
                        if audio_chunk
                        else "Unknown client error"
                    )
                    raise RuntimeError(error_msg)

            if t.text_input_end:
                self.ten_env.log_info(f"t.text_input_end: {t.text_input_end}")
                self.current_request_finished = True

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}"
            )
            await self.send_tts_error(
                self.current_request_id or t.request_id,
                ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
