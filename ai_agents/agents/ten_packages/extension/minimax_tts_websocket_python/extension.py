#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import os
import traceback
import uuid

from ten_ai_base.helper import PCMWriter, generate_file_name
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    ModuleVendorException,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput, TTSFlush
from ten_ai_base.tts2 import AsyncTTS2BaseExtension

from .config import MinimaxTTSWebsocketConfig
from .minimax_tts import MinimaxTTSWebsocket, MinimaxTTSTaskFailedException, EVENT_TTSSentenceEnd, EVENT_TTSResponse, EVENT_TTSFlush
from ten_runtime import (
    AsyncTenEnv,
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
        self.finished_request_ids: set[str] = set()
        self.recorder_map: dict[str, PCMWriter] = {}  # Store PCMWriter instances for different request_ids

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("on_init")

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.ten_env.log_info(f"Raw config JSON: {config_json}")

                # Check if config is empty or missing required fields
                if not config_json or config_json.strip() == "{}":
                    error_msg = "Configuration is empty. Required parameters: api_key, group_id are missing."
                    self.ten_env.log_error(error_msg)

                    await self.send_tts_error(
                        self.current_request_id or "",
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info={},
                        ),
                    )
                    return

                try:
                    self.config = MinimaxTTSWebsocketConfig.model_validate_json(config_json)
                except Exception as validation_error:
                    error_msg = f"Configuration validation failed: {str(validation_error)}"
                    self.ten_env.log_error(error_msg)

                    await self.send_tts_error(
                        self.current_request_id or "",
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info={},
                        ),
                    )
                    return

                self.ten_env.log_info(f"Parsed config: {self.config.to_str()}")

                # Check if API key is still a placeholder
                if self.config.api_key.startswith("${env:"):
                    error_msg = f"Environment variable not resolved: {self.config.api_key}. Please set the MINIMAX_TTS_API_KEY environment variable."
                    self.ten_env.log_error(error_msg)

                    await self.send_tts_error(
                        self.current_request_id or "",
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info={},
                        ),
                    )
                    return

                if not self.config.api_key:
                    error_msg = "Required parameter 'api_key' is missing or empty."
                    self.ten_env.log_error(error_msg)

                    await self.send_tts_error(
                        self.current_request_id or "",
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info={},
                        ),
                    )
                    return

                if not self.config.group_id:
                    error_msg = "Required parameter 'group_id' is missing or empty."
                    self.ten_env.log_error(error_msg)

                    await self.send_tts_error(
                        self.current_request_id or "",
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.FATAL_ERROR,
                            vendor_info={},
                        ),
                    )
                    return

                # extract audio_params and additions from config
                self.config.update_params()

            self.client = MinimaxTTSWebsocket(self.config, ten_env, self.vendor())
            # Preheat websocket connection
            await self.client.start()
            ten_env.log_info("MinimaxTTSWebsocket client initialized and preheated successfully")
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")

            # Send FATAL ERROR for unexpected exceptions during initialization
            await self.send_tts_error(
                self.current_request_id or "",
                ModuleError(
                    message=f"Unexpected error during initialization: {str(e)}",
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info={},
                ),
            )

    async def on_data(self, ten_env: AsyncTenEnv, data) -> None:
        # Get the necessary properties
        self.ten_env.log_info(f"on_data12345: {data.get_name()}")
        data_name = data.get_name()
        ten_env.log_info(f"on_data:{data_name}")
        if data.get_name() == "tts_flush":
            await self.client.cancel()
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
                ten_env.log_info(f"Flushed PCMWriter for request_id: {request_id}")
            except Exception as e:
                ten_env.log_error(f"Error flushing PCMWriter for request_id {request_id}: {e}")

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    def vendor(self) -> str:
        return "minimax"

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0
        bytes_per_sample = 2  # Assuming 16-bit audio
        channels = 1  # Assuming mono
        duration_sec = self.total_audio_bytes / (self.config.sample_rate * bytes_per_sample * channels)
        return int(duration_sec * 1000)

    async def on_flush(self, t: TTSFlush) -> None:
        if self.client and t.flush_id == self.current_request_id:
            self.ten_env.log_info(f"Flushing TTS for request ID: {t.flush_id}")
            await self.client.cancel()


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
                self.ten_env.log_info("TTS client is not initialized, attempting to reconnect...")
                self.client = MinimaxTTSWebsocket(self.config, self.ten_env, self.vendor())
                await self.client.start()
                self.ten_env.log_info("TTS client reconnected successfully.")

            if t.request_id != self.current_request_id:
                self.ten_env.log_info(
                    f"KEYPOINT New TTS request with ID: {t.request_id}"
                )
                self.current_request_id = t.request_id
                self.current_request_finished = False
                self.total_audio_bytes = 0 # Reset for new request
                if t.metadata is not None:
                    self.session_id = t.metadata.get("session_id", "")
                    self.current_turn_id = t.metadata.get("turn_id", -1)

                # Create new PCMWriter for new request_id and clean up old ones
                if self.config and self.config.dump:
                    # Clean up old PCMWriters (except current request_id)
                    old_request_ids = [rid for rid in self.recorder_map.keys() if rid != t.request_id]
                    for old_rid in old_request_ids:
                        try:
                            await self.recorder_map[old_rid].flush()
                            del self.recorder_map[old_rid]
                            self.ten_env.log_info(f"Cleaned up old PCMWriter for request_id: {old_rid}")
                        except Exception as e:
                            self.ten_env.log_error(f"Error cleaning up PCMWriter for request_id {old_rid}: {e}")

                    # Create new PCMWriter
                    if t.request_id not in self.recorder_map:
                        dump_file_path = os.path.join(
                            self.config.dump_path,
                            f"minimax_dump_{t.request_id}.pcm"
                        )
                        self.recorder_map[t.request_id] = PCMWriter(dump_file_path)
                        self.ten_env.log_info(f"Created PCMWriter for request_id: {t.request_id}, file: {dump_file_path}")
            elif self.current_request_finished:
                if not t.text_input_end:
                    error_msg = f"Received a message for a finished request_id '{t.request_id}' with text_input_end=False."
                    self.ten_env.log_error(error_msg)
                    await self.send_tts_error(
                        t.request_id,
                        ModuleError(
                            message=error_msg,
                            module_name=ModuleType.TTS,
                            code=ModuleErrorCode.NON_FATAL_ERROR,
                            vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                        ),
                    )
                return

            if t.text.strip() == "":
                self.ten_env.log_info("Received empty text for TTS request")
                if t.text_input_end:
                    self.current_request_finished = True
                return

            # Record TTFB timing
            if self.sent_ts is None:
                self.sent_ts = datetime.now()

            # Get audio stream from Minimax TTS
            self.ten_env.log_info(f"Calling client.get() with text: {t.text}")
            data = self.client.get(t.text)
            self.ten_env.log_info(f"Got data generator: {data}")
            first_chunk = True

            self.ten_env.log_info("Starting async for loop to process audio chunks")
            chunk_count = 0
            async for audio_chunk, event_status in data:
                self.ten_env.log_info(f"Received event_status: {event_status}")
                if event_status == EVENT_TTSResponse:
                    if audio_chunk is not None and len(audio_chunk) > 0:
                        chunk_count += 1
                        self.total_audio_bytes += len(audio_chunk)
                        self.ten_env.log_info(f"[tts] Received audio chunk #{chunk_count}, size: {len(audio_chunk)} bytes")

                        # Send TTS audio start on first chunk
                        if first_chunk:
                            if self.sent_ts:
                                await self.send_tts_audio_start(self.current_request_id)
                                ttfb = int((datetime.now() - self.sent_ts).total_seconds() * 1000)
                                await self.send_tts_ttfb_metrics(
                                    self.current_request_id, ttfb, self.current_turn_id
                                )
                                self.ten_env.log_info(f"KEYPOINT Sent TTS audio start and TTFB metrics: {ttfb}ms")
                            first_chunk = False

                        # Write to dump file if enabled
                        if self.config and self.config.dump and self.current_request_id and self.current_request_id in self.recorder_map:
                            self.ten_env.log_info(f"KEYPOINT Writing audio chunk to dump file, dump url: {self.config.dump_path}")
                            asyncio.create_task(self.recorder_map[self.current_request_id].write(audio_chunk))

                        # Send audio data
                        await self.send_tts_audio_data(audio_chunk)
                    else:
                        self.ten_env.log_error("Received empty payload for TTS response")

                elif event_status == EVENT_TTSSentenceEnd:
                    self.ten_env.log_info("Received TTSSentenceEnd event from Minimax TTS")
                    # Send TTS audio end event
                    if self.sent_ts:
                        duration_ms = self._calculate_audio_duration_ms()
                        request_event_interval = int((datetime.now() - self.sent_ts).total_seconds() * 1000)
                        await self.send_tts_audio_end(
                            self.current_request_id,
                            request_event_interval,
                            duration_ms,
                            self.current_turn_id,
                        )
                        self.ten_env.log_info(f"KEYPOINT Sent TTS audio end event, interval: {request_event_interval}ms, duration: {duration_ms}ms")
                    break
                elif event_status == EVENT_TTSFlush:
                    self.ten_env.log_info("Received TTSFlush event, sending audio end with flush reason.")
                    duration_ms = self._calculate_audio_duration_ms()
                    await self.send_tts_audio_end(
                        self.current_request_id,
                        duration_ms,
                        duration_ms,  # Using same for request_event_interval for now
                        self.current_turn_id,
                        TTSAudioEndReason.INTERRUPTED
                    )
                    self.ten_env.log_info(f"KEYPOINT Sent TTS audio end event due to flush, duration: {duration_ms}ms")
                    break

            self.ten_env.log_info(f"TTS processing completed, total chunks: {chunk_count}")
            self.sent_ts = None  # Reset for next request

            if t.text_input_end:
                self.ten_env.log_info(
                    f"KEYPOINT finish session for request ID: {t.request_id}"
                )
                self.current_request_finished = True

        except MinimaxTTSTaskFailedException as e:
            self.ten_env.log_error(
                f"MinimaxTTSTaskFailedException in request_tts: {e.error_msg} (code: {e.error_code}). text: {t.text}"
            )
            if e.error_code == 2054:
                await self.send_tts_error(
                    self.current_request_id,
                    ModuleError(
                        message=e.error_msg,
                        module_name=ModuleType.TTS,
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
                        module_name=ModuleType.TTS,
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
                    module_name=ModuleType.TTS,
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
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(
                        vendor=self.vendor()
                    )
                ),
            )
            # When a connection error occurs, destroy the client instance.
            # It will be recreated on the next request.
            if isinstance(e, ConnectionRefusedError) and self.client:
                await self.client.stop()
                self.client = None
                self.ten_env.log_info("Client connection dropped, instance destroyed. Will attempt to reconnect on next request.")