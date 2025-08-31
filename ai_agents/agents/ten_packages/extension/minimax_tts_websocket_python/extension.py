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
    ModuleVendorException,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension

from .config import MinimaxTTSWebsocketConfig
from .minimax_tts import (
    MinimaxTTSWebsocket,
    MinimaxTTSTaskFailedException,
    EVENT_TTSSentenceEnd,
    EVENT_TTSResponse,
)
from ten_runtime import (
    AsyncTenEnv,
    Data,
)


class MinimaxTTSWebsocketExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: MinimaxTTSWebsocketConfig | None = None
        self.client: MinimaxTTSWebsocket | None = None
        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.sent_ts: datetime | None = None
        self.current_request_finished: bool = False
        self.total_audio_bytes: int = 0
        self.first_chunk: bool = False
        self.recorder_map: dict[str, PCMWriter] = (
            {}
        )  # Store PCMWriter instances for different request_ids

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = MinimaxTTSWebsocketConfig.model_validate_json(
                    config_json
                )
                self.config.update_params()
                self.config.validate_params()
                self.ten_env.log_info(
                    f"KEYPOINT config: {self.config.to_str(sensitive_handling=True)}"
                )

            self.client = MinimaxTTSWebsocket(
                self.config,
                ten_env,
                self.vendor(),
                self._websocket_error_callback,
            )
            # Preheat websocket connection
            asyncio.create_task(self.client.start())
            ten_env.log_info(
                "MinimaxTTSWebsocket client initialized and preheated successfully"
            )
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")

            # Send FATAL ERROR for unexpected exceptions during initialization
            await self.send_tts_error(
                self.current_request_id or "",
                ModuleError(
                    message=f"Unexpected error during initialization: {str(e)}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info={},
                ),
            )

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"on_data: {data_name}")

        if data_name == "tts_flush":
            flush_id, _ = data.get_property_string("flush_id")
            if flush_id:
                ten_env.log_info(f"Received flush request for ID: {flush_id}")
                if self.current_request_id:
                    ten_env.log_info(
                        f"Current request {self.current_request_id} is being flushed. Sending INTERRUPTED."
                    )
                    await self.client.cancel()
                    if self.sent_ts:
                        request_event_interval = int(
                            (datetime.now() - self.sent_ts).total_seconds()
                            * 1000
                        )
                        duration_ms = self._calculate_audio_duration_ms()
                        await self.send_tts_audio_end(
                            self.current_request_id,
                            request_event_interval,
                            duration_ms,
                            self.current_turn_id,
                            TTSAudioEndReason.INTERRUPTED,
                        )
                        self.current_request_finished = True
        await super().on_data(ten_env, data)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        # Clean up client if exists
        if self.client:
            # Stop the websocket connection
            await self.client.stop()
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

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    def vendor(self) -> str:
        return "minimax"

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    async def _websocket_error_callback(
        self, message: str, detail: str, is_fatal: bool = False
    ) -> None:
        """Callback for handling WebSocket errors from minimax_tts"""
        error_code = (
            ModuleErrorCode.FATAL_ERROR
            if is_fatal
            else ModuleErrorCode.NON_FATAL_ERROR
        )

        await self.send_tts_error(
            self.current_request_id,
            ModuleError(
                message=f"{message}: {detail}",
                module=ModuleType.TTS,
                code=error_code,
                vendor_info=ModuleErrorVendorInfo(
                    vendor=self.vendor(),
                    code="WEBSOCKET_ERROR",
                    message=detail,
                ),
            ),
        )

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0
        bytes_per_sample = 2  # Assuming 16-bit audio
        channels = self.config.channels
        duration_sec = self.total_audio_bytes / (
            self.config.sample_rate * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)

    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Override this method to handle TTS requests.
        This is called when the TTS request is made.
        """
        try:
            # If client is None, it means the connection was dropped or never initialized.
            # Attempt to re-establish the connection.
            self.ten_env.log_info(
                f"KEYPOINT Requesting TTS for text: {t.text}, text_input_end: {t.text_input_end} request ID: {t.request_id}"
            )
            if self.client is None:
                self.ten_env.log_info(
                    "TTS client is not initialized, attempting to reconnect..."
                )
                self.client = MinimaxTTSWebsocket(
                    self.config,
                    self.ten_env,
                    self.vendor(),
                    self._websocket_error_callback,
                )
                await self.client.start()
                self.ten_env.log_info("TTS client reconnected successfully.")

            self.ten_env.log_info(
                f"current_request_id: {self.current_request_id}, new request_id: {t.request_id}, current_request_finished: {self.current_request_finished}"
            )

            if t.request_id != self.current_request_id:
                self.ten_env.log_info(
                    f"KEYPOINT New TTS request with ID: {t.request_id}"
                )
                self.first_chunk = True
                self.sent_ts = datetime.now()
                self.current_request_id = t.request_id
                self.current_request_finished = False
                self.total_audio_bytes = 0  # Reset for new request
                if t.metadata is not None:
                    self.session_id = t.metadata.get("session_id", "")
                    self.current_turn_id = t.metadata.get("turn_id", -1)

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
                            f"minimax_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )
                        self.ten_env.log_info(
                            f"Created PCMWriter for request_id: {t.request_id}, file: {dump_file_path}"
                        )
            elif self.current_request_finished:
                error_msg = f"Received a message for a finished request_id '{t.request_id}' skip processing."
                self.ten_env.log_error(error_msg)
                return

            if t.text_input_end:
                self.ten_env.log_info(
                    f"KEYPOINT finish session for request ID: {t.request_id}"
                )
                self.current_request_finished = True

            # Get audio stream from Minimax TTS
            self.ten_env.log_info(f"Calling client.get() with text: {t.text}")
            data = self.client.get(t.text)

            self.ten_env.log_info(
                "Starting async for loop to process audio chunks"
            )
            chunk_count = 0
            async for audio_chunk, event_status in data:
                # self.ten_env.log_info(f"Received event_status: {event_status}")
                if event_status == EVENT_TTSResponse:
                    if audio_chunk is not None and len(audio_chunk) > 0:
                        chunk_count += 1
                        self.total_audio_bytes += len(audio_chunk)
                        # self.ten_env.log_info(
                        #     f"[tts] Received audio chunk #{chunk_count}, size: {len(audio_chunk)} bytes"
                        # )

                        # Send TTS audio start on first chunk
                        if self.first_chunk:
                            if self.sent_ts:
                                await self.send_tts_audio_start(
                                    self.current_request_id
                                )
                                ttfb = int(
                                    (
                                        datetime.now() - self.sent_ts
                                    ).total_seconds()
                                    * 1000
                                )
                                await self.send_tts_ttfb_metrics(
                                    self.current_request_id,
                                    ttfb,
                                    self.current_turn_id,
                                )
                                self.ten_env.log_info(
                                    f"KEYPOINT Sent TTS audio start and TTFB metrics: {ttfb}ms"
                                )
                            self.first_chunk = False

                        # Write to dump file if enabled
                        if (
                            self.config
                            and self.config.dump
                            and self.current_request_id
                            and self.current_request_id in self.recorder_map
                        ):
                            self.ten_env.log_info(
                                f"KEYPOINT Writing audio chunk to dump file, dump url: {self.config.dump_path}"
                            )
                            asyncio.create_task(
                                self.recorder_map[
                                    self.current_request_id
                                ].write(audio_chunk)
                            )

                        # Send audio data
                        await self.send_tts_audio_data(audio_chunk)
                    else:
                        self.ten_env.log_error(
                            "Received empty payload for TTS response"
                        )
                        if t.text_input_end:
                            duration_ms = self._calculate_audio_duration_ms()
                            request_event_interval = int(
                                (datetime.now() - self.sent_ts).total_seconds()
                                * 1000
                            )
                            await self.send_tts_audio_end(
                                self.current_request_id,
                                request_event_interval,
                                duration_ms,
                                self.current_turn_id,
                            )
                            self.ten_env.log_info(
                                f"KEYPOINT Sent TTS audio end event, interval: {request_event_interval}ms, duration: {duration_ms}ms"
                            )

                elif event_status == EVENT_TTSSentenceEnd:
                    self.ten_env.log_info(
                        "Received TTSSentenceEnd event from Minimax TTS"
                    )
                    # Send TTS audio end event
                    if self.sent_ts and t.text_input_end:
                        request_event_interval = int(
                            (datetime.now() - self.sent_ts).total_seconds()
                            * 1000
                        )
                        duration_ms = self._calculate_audio_duration_ms()
                        await self.send_tts_audio_end(
                            self.current_request_id,
                            request_event_interval,
                            duration_ms,
                            self.current_turn_id,
                        )
                        self.ten_env.log_info(
                            f"KEYPOINT Sent TTS audio end event, interval: {request_event_interval}ms, duration: {duration_ms}ms"
                        )
                    break

            self.ten_env.log_info(
                f"TTS processing completed, total chunks: {chunk_count}"
            )

        except MinimaxTTSTaskFailedException as e:
            self.ten_env.log_error(
                f"MinimaxTTSTaskFailedException in request_tts: {e.error_msg} (code: {e.error_code}). text: {t.text}"
            )
            if e.error_code == 2054:
                await self.send_tts_error(
                    self.current_request_id,
                    ModuleError(
                        message=e.error_msg,
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.error_code),
                            message=e.error_msg,
                        ),
                    ),
                )
            else:
                await self.send_tts_error(
                    self.current_request_id,
                    ModuleError(
                        message=e.error_msg,
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(
                            vendor=self.vendor(),
                            code=str(e.error_code),
                            message=e.error_msg,
                        ),
                    ),
                )
        except ModuleVendorException as e:
            self.ten_env.log_error(
                f"ModuleVendorException in request_tts: {traceback.format_exc()}. text: {t.text}"
            )
            await self.send_tts_error(
                self.current_request_id,
                ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=e.error,
                ),
            )
        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}. text: {t.text}"
            )
            await self.send_tts_error(
                self.current_request_id,
                ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
            # When a connection error occurs, destroy the client instance.
            # It will be recreated on the next request.
            if isinstance(e, ConnectionRefusedError) and self.client:
                await self.client.stop()
                self.client = None
                self.ten_env.log_info(
                    "Client connection dropped, instance destroyed. Will attempt to reconnect on next request."
                )
