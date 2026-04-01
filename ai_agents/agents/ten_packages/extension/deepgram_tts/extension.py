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
    ModuleType,
    ModuleErrorVendorInfo,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_ai_base.const import LOG_CATEGORY_VENDOR, LOG_CATEGORY_KEY_POINT
from .config import DeepgramTTSConfig

from .deepgram_tts import (
    EVENT_TTS_END,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
    EVENT_TTS_ERROR,
    DeepgramTTSClient,
    DeepgramTTSConnectionException,
)
from ten_runtime import AsyncTenEnv


class DeepgramTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: DeepgramTTSConfig | None = None
        self.client: DeepgramTTSClient | None = None
        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.sent_ts: datetime | None = None
        self.current_request_finished: bool = False
        self.total_audio_bytes: int = 0
        self._is_stopped: bool = False
        self.recorder_map: dict[str, PCMWriter] = {}
        self._audio_start_sent: bool = False  # Track if tts_audio_start was sent

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")
            ten_env.log_info(f"config_json_str: {config_json_str}")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError(
                    "Configuration is empty. "
                    "Required parameter 'api_key' is missing."
                )

            self.config = DeepgramTTSConfig.model_validate_json(config_json_str)
            self.config.update_params()
            ten_env.log_info(
                f"LOG_CATEGORY_KEY_POINT: "
                f"{self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            if not self.config.api_key:
                raise ValueError("API key is required")

            self.client = DeepgramTTSClient(
                config=self.config,
                ten_env=ten_env,
                send_fatal_tts_error=self.send_fatal_tts_error,
                send_non_fatal_tts_error=self.send_non_fatal_tts_error,
            )
            asyncio.create_task(self.client.start())
            ten_env.log_debug("DeepgramTTS client initialized successfully")
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
        self._is_stopped = True
        ten_env.log_debug("Extension stopping, rejecting new requests")

        if self.client:
            await self.client.stop()
            self.client = None

        # Clean up all PCMWriters
        for request_id, recorder in list(self.recorder_map.items()):
            try:
                await recorder.flush()
                ten_env.log_debug(
                    f"Flushed PCMWriter for request_id: {request_id}"
                )
            except Exception as e:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {e}"
                )

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    async def cancel_tts(self) -> None:
        self.current_request_finished = True
        if self.current_request_id:
            self.ten_env.log_debug(
                f"Current request {self.current_request_id} is being "
                f"cancelled. Sending INTERRUPTED."
            )

            if self.client:
                await self.client.cancel()
                if self.sent_ts:
                    request_event_interval = int(
                        (datetime.now() - self.sent_ts).total_seconds() * 1000
                    )
                    duration_ms = self._calculate_audio_duration_ms()
                    await self.send_tts_audio_end(
                        request_id=self.current_request_id,
                        request_event_interval_ms=request_event_interval,
                        request_total_audio_duration_ms=duration_ms,
                        reason=TTSAudioEndReason.INTERRUPTED,
                    )
                    if self.current_request_id in self.recorder_map:
                        await self.recorder_map[self.current_request_id].flush()
                    await self.finish_request(
                        request_id=self.current_request_id,
                        reason=TTSAudioEndReason.INTERRUPTED,
                    )
        else:
            self.ten_env.log_warn(
                "No current request found, skipping TTS cancellation."
            )

    def vendor(self) -> str:
        return "deepgram"

    def synthesize_audio_sample_rate(self) -> int:
        if self.config is None:
            return 24000  # Default sample rate
        return self.config.sample_rate

    async def request_tts(self, t: TTSTextInput) -> None:
        """Handle TTS requests."""
        try:
            self.ten_env.log_info(
                f"Requesting TTS for text: {t.text}, "
                f"text_input_end: {t.text_input_end} "
                f"request ID: {t.request_id}",
            )
            # Reconnect if needed
            if self.client is None:
                self.ten_env.log_debug(
                    "TTS client is not initialized, attempting to reconnect..."
                )
                self.client = DeepgramTTSClient(
                    config=self.config,
                    ten_env=self.ten_env,
                    send_fatal_tts_error=self.send_fatal_tts_error,
                    send_non_fatal_tts_error=self.send_non_fatal_tts_error,
                )
                asyncio.create_task(self.client.start())
                self.ten_env.log_debug("TTS client reconnected successfully.")

            self.ten_env.log_debug(
                f"current_request_id: {self.current_request_id}, "
                f"new request_id: {t.request_id}, "
                f"current_request_finished: {self.current_request_finished}"
            )

            if t.request_id != self.current_request_id:
                self.ten_env.log_debug(
                    f"New TTS request with ID: {t.request_id}"
                )
                if self.client:
                    self.client.reset_ttfb()
                self.current_request_id = t.request_id
                self.current_request_finished = False
                self.total_audio_bytes = 0
                self.sent_ts = None
                self._audio_start_sent = False  # Reset for new request
                if t.metadata is not None:
                    self.session_id = t.metadata.get("session_id", "")
                    self.current_turn_id = t.metadata.get("turn_id", -1)
                # Create new PCMWriter for new request_id
                if self.config and self.config.dump:
                    # Clean up old PCMWriters
                    old_request_ids = [
                        rid
                        for rid in self.recorder_map.keys()
                        if rid != t.request_id
                    ]
                    for old_rid in old_request_ids:
                        try:
                            await self.recorder_map[old_rid].flush()
                            del self.recorder_map[old_rid]
                            self.ten_env.log_debug(
                                f"Cleaned up old PCMWriter for "
                                f"request_id: {old_rid}"
                            )
                        except Exception as e:
                            self.ten_env.log_error(
                                f"Error cleaning up PCMWriter for "
                                f"request_id {old_rid}: {e}"
                            )

                    # Create new PCMWriter
                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"deepgram_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )
                        self.ten_env.log_debug(
                            f"Created PCMWriter for request_id: "
                            f"{t.request_id}, file: {dump_file_path}"
                        )
            elif self.current_request_finished:
                self.ten_env.log_error(
                    f"Received a message for a finished request_id "
                    f"'{t.request_id}' with text_input_end=False."
                )
                return

            if t.text_input_end:
                self.ten_env.log_debug(
                    f"KEYPOINT finish session for request ID: {t.request_id}"
                )
                self.current_request_finished = True

            prepared_text = t.text.strip()

            if self._is_stopped:
                self.ten_env.log_debug(
                    f"TTS is stopped, skipping request_id: {t.request_id}"
                )
                return

            if prepared_text != "":
                self.ten_env.log_debug(
                    f"send_text_to_tts_server: {prepared_text} "
                    f"of request_id: {t.request_id}",
                    category=LOG_CATEGORY_VENDOR,
                )
                data = self.client.get(prepared_text)

                chunk_count = 0
                if self.sent_ts is None:
                    self.sent_ts = datetime.now()
                async for data_msg, event_status in data:
                    self.ten_env.log_debug(
                        f"Received event_status: {event_status}"
                    )
                    if event_status == EVENT_TTS_RESPONSE:
                        if (
                            data_msg is not None
                            and isinstance(data_msg, bytes)
                            and len(data_msg) > 0
                        ):
                            chunk_count += 1
                            self.total_audio_bytes += len(data_msg)
                            self.ten_env.log_info(
                                f"Received audio chunk #{chunk_count}, "
                                f"size: {len(data_msg)} bytes"
                            )
                            # Write to dump file if enabled
                            if (
                                self.config
                                and self.config.dump
                                and self.current_request_id
                                and self.current_request_id in self.recorder_map
                            ):
                                self.ten_env.log_debug(
                                    f"Writing audio chunk to dump file, "
                                    f"dump url: {self.config.dump_path}"
                                )
                                asyncio.create_task(
                                    self.recorder_map[
                                        self.current_request_id
                                    ].write(data_msg)
                                )

                            # Send audio data
                            await self.send_tts_audio_data(data_msg)
                        else:
                            self.ten_env.log_debug(
                                "Received empty payload for TTS response"
                            )
                            if t.text_input_end:
                                duration_ms = (
                                    self._calculate_audio_duration_ms()
                                )
                                request_event_interval = (
                                    self._current_request_interval_ms()
                                )
                                await self.send_tts_audio_end(
                                    request_id=self.current_request_id,
                                    request_event_interval_ms=(
                                        request_event_interval
                                    ),
                                    request_total_audio_duration_ms=duration_ms,
                                )
                                if self.current_request_id in self.recorder_map:
                                    await self.recorder_map[
                                        self.current_request_id
                                    ].flush()
                                await self.finish_request(
                                    request_id=self.current_request_id,
                                    reason=TTSAudioEndReason.REQUEST_END,
                                )
                                self.sent_ts = None
                                self.ten_env.log_debug(
                                    f"Sent TTS audio end event, "
                                    f"interval: {request_event_interval}ms, "
                                    f"duration: {duration_ms}ms"
                                )
                    elif event_status == EVENT_TTS_TTFB_METRIC:
                        if data_msg is not None and isinstance(data_msg, int):
                            self.sent_ts = datetime.now()
                            ttfb = data_msg
                            await self.send_tts_audio_start(
                                request_id=self.current_request_id,
                            )
                            self._audio_start_sent = True
                            extra_metadata = {
                                "model": self.config.model,
                            }
                            await self.send_tts_ttfb_metrics(
                                request_id=self.current_request_id,
                                ttfb_ms=ttfb,
                                extra_metadata=extra_metadata,
                            )

                            self.ten_env.log_debug(
                                f"Sent TTS audio start and "
                                f"TTFB metrics: {ttfb}ms"
                            )
                    elif event_status == EVENT_TTS_END:
                        self.ten_env.log_info(
                            "Received TTS_END event from Deepgram TTS"
                        )
                        # Send TTS audio end event
                        if t.text_input_end:
                            request_event_interval = (
                                self._current_request_interval_ms()
                            )
                            duration_ms = self._calculate_audio_duration_ms()
                            await self.send_tts_audio_end(
                                request_id=self.current_request_id,
                                request_event_interval_ms=request_event_interval,
                                request_total_audio_duration_ms=duration_ms,
                            )
                            if self.current_request_id in self.recorder_map:
                                await self.recorder_map[
                                    self.current_request_id
                                ].flush()
                            self.sent_ts = None
                            self.ten_env.log_debug(
                                f"Sent TTS audio end event, "
                                f"interval: {request_event_interval}ms, "
                                f"duration: {duration_ms}ms"
                            )
                            await self.finish_request(
                                request_id=self.current_request_id,
                                reason=TTSAudioEndReason.REQUEST_END,
                            )
                        break
                    elif event_status == EVENT_TTS_ERROR:
                        self.ten_env.log_error(
                            "Received TTS_ERROR event from Deepgram TTS"
                        )
                        # Decode error message if bytes
                        error_msg = (
                            data_msg.decode("utf-8")
                            if isinstance(data_msg, bytes)
                            else str(data_msg)
                        )
                        # Send TTS audio end event
                        if t.text_input_end:
                            # Ensure tts_audio_start is sent before tts_audio_end
                            if not self._audio_start_sent:
                                await self.send_tts_audio_start(
                                    request_id=self.current_request_id,
                                )
                                self._audio_start_sent = True
                            request_event_interval = (
                                self._current_request_interval_ms()
                            )
                            duration_ms = self._calculate_audio_duration_ms()
                            await self.send_tts_audio_end(
                                request_id=self.current_request_id,
                                request_event_interval_ms=request_event_interval,
                                request_total_audio_duration_ms=duration_ms,
                            )
                            await self.finish_request(
                                request_id=self.current_request_id,
                                reason=TTSAudioEndReason.ERROR,
                                error=ModuleError(
                                    message=error_msg,
                                    module=ModuleType.TTS,
                                    code=ModuleErrorCode.NON_FATAL_ERROR,
                                    vendor_info=ModuleErrorVendorInfo(
                                        vendor=self.vendor()
                                    ),
                                ),
                            )
                            if self.current_request_id in self.recorder_map:
                                await self.recorder_map[
                                    self.current_request_id
                                ].flush()
                            self.sent_ts = None
                            self.ten_env.log_debug(
                                f"Sent TTS audio end event, "
                                f"interval: {request_event_interval}ms, "
                                f"duration: {duration_ms}ms"
                            )
                        break

                self.ten_env.log_debug(
                    f"TTS processing completed, total chunks: {chunk_count}"
                )
            elif t.text_input_end:
                # Ensure tts_audio_start is sent before tts_audio_end
                if not self._audio_start_sent:
                    await self.send_tts_audio_start(
                        request_id=self.current_request_id,
                    )
                    self._audio_start_sent = True
                duration_ms = self._calculate_audio_duration_ms()
                request_event_interval = self._current_request_interval_ms()
                await self.send_tts_audio_end(
                    request_id=self.current_request_id,
                    request_event_interval_ms=request_event_interval,
                    request_total_audio_duration_ms=duration_ms,
                )
                if self.current_request_id in self.recorder_map:
                    await self.recorder_map[self.current_request_id].flush()
                await self.finish_request(
                    request_id=self.current_request_id,
                    reason=TTSAudioEndReason.REQUEST_END,
                )
                self.sent_ts = None
                self.ten_env.log_debug(
                    f"Sent TTS audio end event, "
                    f"interval: {request_event_interval}ms, "
                    f"duration: {duration_ms}ms"
                )

        except DeepgramTTSConnectionException as e:
            self.ten_env.log_error(
                f"DeepgramTTSConnectionException in request_tts: "
                f"{e.body}. text: {t.text}"
            )

            if e.status_code == 401:
                await self.send_tts_error(
                    request_id=self.current_request_id,
                    error=ModuleError(
                        message=e.body,
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.status_code),
                            message=e.body,
                        ),
                    ),
                )
                await self.finish_request(
                    request_id=self.current_request_id,
                    reason=TTSAudioEndReason.ERROR,
                    error=ModuleError(
                        message=e.body,
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.status_code),
                            message=e.body,
                        ),
                    ),
                )
            else:
                await self.send_tts_error(
                    request_id=self.current_request_id,
                    error=ModuleError(
                        message=e.body,
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.status_code),
                            message=e.body,
                        ),
                    ),
                )
                await self.finish_request(
                    request_id=self.current_request_id,
                    reason=TTSAudioEndReason.ERROR,
                    error=ModuleError(
                        message=e.body,
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.status_code),
                            message=e.body,
                        ),
                    ),
                )

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}. "
                f"text: {t.text}"
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
            await self.finish_request(
                request_id=self.current_request_id,
                reason=TTSAudioEndReason.ERROR,
                error=ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
            # Destroy client on connection error
            if isinstance(e, ConnectionRefusedError) and self.client:
                await self.client.stop()
                self.client = None
                self.ten_env.log_debug(
                    "Client connection dropped, instance destroyed. "
                    "Will attempt to reconnect on next request."
                )

    async def send_fatal_tts_error(self, error_message: str) -> None:
        await self.send_tts_error(
            request_id=self.current_request_id or "",
            error=ModuleError(
                message=error_message,
                module=ModuleType.TTS,
                code=ModuleErrorCode.FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )

    async def send_non_fatal_tts_error(self, error_message: str) -> None:
        await self.send_tts_error(
            request_id=self.current_request_id or "",
            error=ModuleError(
                message=error_message,
                module=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            ),
        )

    def _current_request_interval_ms(self) -> int:
        if not self.sent_ts:
            return 0
        return int((datetime.now() - self.sent_ts).total_seconds() * 1000)

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0
        bytes_per_sample = 2  # 16-bit PCM
        channels = 1  # Mono
        duration_sec = self.total_audio_bytes / (
            self.synthesize_audio_sample_rate() * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)
