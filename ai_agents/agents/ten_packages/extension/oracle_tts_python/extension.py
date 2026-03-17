#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
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
from ten_ai_base.tts2 import AsyncTTS2BaseExtension, RequestState

from .config import OracleTTSConfig
from .oracle_tts import (
    OracleTTS,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_REQUEST_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_INVALID_KEY_ERROR,
)
from typing_extensions import override
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
from ten_runtime import AsyncTenEnv


class OracleTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: OracleTTSConfig | None = None
        self.client: OracleTTS | None = None
        self.sent_ts: datetime | None = None
        self.current_request_id: str | None = None
        self.total_audio_bytes: int = 0
        self.current_request_finished: bool = False
        self.recorder_map: dict[str, PCMWriter] = {}
        self.last_complete_request_id: str | None = None
        self._flush_requested = False

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError(
                    "Configuration is empty. Required OCI parameters are missing."
                )

            self.config = OracleTTSConfig.model_validate_json(config_json_str)

            ten_env.log_info(
                f"config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            self.config.validate_params()

            self.client = OracleTTS(
                config=self.config,
                ten_env=ten_env,
            )
        except ValueError as e:
            ten_env.log_error(
                f"invalid property: {e}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    message=f"Initialization failed: {e}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    message=f"Initialization failed: {e}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug(
            "OracleTTS extension on_stop started",
            category=LOG_CATEGORY_KEY_POINT,
        )

        if self.client:
            try:
                self.client.clean()
            except Exception as e:
                ten_env.log_error(
                    f"Error cleaning OracleTTS client: {e}",
                    category=LOG_CATEGORY_VENDOR,
                )
            finally:
                self.client = None

        recorder_items = list(self.recorder_map.items())
        for request_id, recorder in recorder_items:
            try:
                await recorder.flush()
            except Exception as e:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {e}",
                    category=LOG_CATEGORY_KEY_POINT,
                )

        self.recorder_map.clear()
        await super().on_stop(ten_env)

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)

    @override
    def vendor(self) -> str:
        return "oracle"

    def synthesize_audio_sample_rate(self) -> int:
        if self.config and self.config.params:
            return int(self.config.params.get("sample_rate", 16000))
        return 16000

    def _calculate_audio_duration_ms(self) -> int:
        bytes_per_sample = 2  # 16-bit PCM
        channels = 1
        sample_rate = self.synthesize_audio_sample_rate()
        if sample_rate == 0:
            return 0
        duration_sec = self.total_audio_bytes / (
            sample_rate * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)

    def _reset_request_state(self) -> None:
        self.total_audio_bytes = 0
        self.current_request_finished = False
        self.sent_ts = None

    async def cancel_tts(self) -> None:
        self._flush_requested = True
        try:
            if self.client is not None:
                self.client.cancel()
            else:
                self.ten_env.log_warn(
                    "Client is not initialized, skipping cancel",
                    category=LOG_CATEGORY_KEY_POINT,
                )
        except Exception as e:
            self.ten_env.log_error(
                f"Error in cancel_tts: {e}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            await self.send_tts_error(
                request_id=self.current_request_id,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

        await self._handle_completed_request(TTSAudioEndReason.INTERRUPTED)

    async def _handle_completed_request(
        self, reason: TTSAudioEndReason
    ) -> None:
        if self.last_complete_request_id == self.current_request_id:
            self.ten_env.log_debug(
                f"{self.current_request_id} was completed, skip.",
                category=LOG_CATEGORY_KEY_POINT,
            )
            return
        self.last_complete_request_id = self.current_request_id

        if (
            self.config
            and self.config.dump
            and self.current_request_id
            and self.current_request_id in self.recorder_map
        ):
            try:
                await self.recorder_map[self.current_request_id].flush()
            except Exception as e:
                self.ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {self.current_request_id}: {e}"
                )

        request_event_interval = 0
        if self.sent_ts is not None:
            request_event_interval = int(
                (datetime.now() - self.sent_ts).total_seconds() * 1000
            )
        await self.send_tts_audio_end(
            request_id=self.current_request_id or "",
            request_event_interval_ms=request_event_interval,
            request_total_audio_duration_ms=self._calculate_audio_duration_ms(),
            reason=reason,
        )

        await self.finish_request(
            request_id=self.current_request_id or "",
            reason=reason,
        )

    async def _handle_error_with_end(
        self,
        request_id: str,
        error_msg: str,
        error_code: ModuleErrorCode = ModuleErrorCode.NON_FATAL_ERROR,
    ) -> None:
        """Send error and, if text_input_end was received, also send audio_end."""
        has_text_input_end = False
        if request_id and request_id in self.request_states:
            if self.request_states[request_id] == RequestState.FINALIZING:
                has_text_input_end = True

        await self.send_tts_error(
            request_id=request_id,
            error=ModuleError(
                message=error_msg,
                module=ModuleType.TTS,
                code=error_code,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )

        if has_text_input_end:
            self.ten_env.log_info(
                f"Error after text_input_end for request {request_id}, sending tts_audio_end with ERROR reason",
                category=LOG_CATEGORY_KEY_POINT,
            )
            request_total_audio_duration = self._calculate_audio_duration_ms()
            await self.send_tts_audio_end(
                request_id=request_id,
                request_event_interval_ms=0,
                request_total_audio_duration_ms=request_total_audio_duration,
                reason=TTSAudioEndReason.ERROR,
            )
            await self.finish_request(
                request_id=request_id,
                reason=TTSAudioEndReason.ERROR,
            )

    async def request_tts(self, t: TTSTextInput) -> None:
        try:
            if not self.client or not self.config:
                raise RuntimeError("Extension is not initialized properly.")

            if self.last_complete_request_id == t.request_id:
                self.ten_env.log_debug(
                    f"Request ID {t.request_id} has already been completed, ignoring"
                )
                return

            if t.request_id != self.current_request_id:
                self.current_request_id = t.request_id
                self._reset_request_state()
                self._flush_requested = False

                if self.config.dump:
                    old_request_ids = [
                        rid
                        for rid in self.recorder_map.keys()
                        if rid != t.request_id
                    ]
                    for old_rid in old_request_ids:
                        try:
                            await self.recorder_map[old_rid].flush()
                            del self.recorder_map[old_rid]
                        except Exception as e:
                            self.ten_env.log_error(
                                f"Error cleaning up PCMWriter for request_id {old_rid}: {e}"
                            )

                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"oracle_tts_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )

            audio_generator = None
            if t.text.strip():
                try:
                    audio_generator = self.client.get(t.text, t.request_id)
                    async for audio_chunk, event, ttfb_ms in audio_generator:
                        if self._flush_requested:
                            self.ten_env.log_debug(
                                "Flush requested, stopping audio processing"
                            )
                            break

                        if event == EVENT_TTS_RESPONSE and audio_chunk:
                            self.total_audio_bytes += len(audio_chunk)
                            duration_ms = (
                                self.total_audio_bytes
                                / (
                                    self.synthesize_audio_sample_rate()
                                    * 2
                                    * 1
                                )
                                * 1000
                            )
                            self.ten_env.log_debug(
                                f"receive_audio: duration: {duration_ms:.0f}ms of request id: {t.request_id}",
                                category=LOG_CATEGORY_VENDOR,
                            )

                            if (
                                self.sent_ts is None
                                and self.current_request_id
                            ):
                                self.sent_ts = datetime.now()
                                await self.send_tts_audio_start(
                                    request_id=self.current_request_id,
                                )
                                extra_metadata = {
                                    "voice_id": self.config.params.get(
                                        "voice_id", ""
                                    ),
                                    "model_name": self.config.params.get(
                                        "model_name", ""
                                    ),
                                }
                                if ttfb_ms is not None:
                                    await self.send_tts_ttfb_metrics(
                                        request_id=self.current_request_id,
                                        ttfb_ms=ttfb_ms,
                                        extra_metadata=extra_metadata,
                                    )

                            if (
                                self.config.dump
                                and self.current_request_id
                                and self.current_request_id
                                in self.recorder_map
                            ):
                                await self.recorder_map[
                                    self.current_request_id
                                ].write(audio_chunk)

                            await self.send_tts_audio_data(audio_chunk)

                        elif event == EVENT_TTS_REQUEST_END:
                            break

                        elif event == EVENT_TTS_INVALID_KEY_ERROR:
                            error_msg = (
                                audio_chunk.decode("utf-8")
                                if audio_chunk
                                else "OCI authentication error"
                            )
                            request_id = (
                                self.current_request_id or t.request_id
                            )
                            await self._handle_error_with_end(
                                request_id,
                                error_msg,
                                error_code=ModuleErrorCode.FATAL_ERROR,
                            )
                            return

                        elif event == EVENT_TTS_ERROR:
                            error_msg = (
                                audio_chunk.decode("utf-8")
                                if audio_chunk
                                else "Unknown Oracle TTS error"
                            )
                            raise RuntimeError(error_msg)

                except RuntimeError:
                    raise
                except Exception as e:
                    self.ten_env.log_error(
                        f"Error in audio processing: {traceback.format_exc()}"
                    )
                    await self._handle_error_with_end(
                        self.current_request_id or t.request_id, str(e)
                    )

                finally:
                    if audio_generator is not None:
                        try:
                            await audio_generator.aclose()
                        except Exception as e:
                            self.ten_env.log_error(
                                f"Error closing audio generator: {e}"
                            )
            else:
                self.ten_env.log_debug(
                    f"skip_tts_text_input: empty text of request id: {t.request_id}",
                    category=LOG_CATEGORY_KEY_POINT,
                )

            if t.text_input_end:
                self.current_request_finished = True
                await self._handle_completed_request(
                    TTSAudioEndReason.REQUEST_END
                )

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}"
            )
            await self._handle_error_with_end(
                self.current_request_id or t.request_id, str(e)
            )
