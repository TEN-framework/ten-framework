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
from ten_ai_base.tts2 import AsyncTTS2BaseExtension

from .config import GoogleTTSConfig
from .google_tts import (
    GoogleTTS,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_REQUEST_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_INVALID_KEY_ERROR,
)

from ten_runtime import AsyncTenEnv


class GoogleTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: GoogleTTSConfig | None = None
        self.client: GoogleTTS | None = None
        self.sent_ts: datetime | None = None
        self.current_request_id: str | None = None
        self.current_turn_id: int = -1
        self.total_audio_bytes: int = 0
        self.current_request_finished: bool = False
        self.recorder_map: dict[str, PCMWriter] = (
            {}
        )  # Store PCMWriter instances for different request_ids
        self.completed_request_ids: set[str] = (
            set()
        )  # Track completed request IDs

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")
            ten_env.log_info(f"config_json_str: {config_json_str}")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError(
                    "Configuration is empty. Required parameter 'credentials' is missing."
                )

            self.config = GoogleTTSConfig.model_validate_json(config_json_str)
            self.config.update_params()

            ten_env.log_info(
                f"config: {self.config.to_str(sensitive_handling=True)}"
            )
            if not self.config.credentials:
                raise ValueError(
                    "Configuration is empty. Required parameter 'credentials' is missing."
                )

            self.client = GoogleTTS(config=self.config, ten_env=ten_env)
        except ValueError as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                "",
                ModuleError(
                    message=f"Initialization failed: {e}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
        except Exception as e:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                "",
                ModuleError(
                    message=f"Initialization failed: {e}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("GoogleTTS extension on_stop started")

        # Clean up client
        if self.client:
            try:
                self.client.clean()
                ten_env.log_info("GoogleTTS client cleaned successfully")
            except Exception as e:
                ten_env.log_error(f"Error cleaning GoogleTTS client: {e}")
            finally:
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

        # Clear all maps and sets
        self.recorder_map.clear()
        self.completed_request_ids.clear()

        ten_env.log_info("GoogleTTS extension on_stop completed")
        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("GoogleTTS extension on_deinit started")
        await super().on_deinit(ten_env)
        ten_env.log_info("GoogleTTS extension on_deinit completed")
        ten_env.log_debug("on_deinit")

    def vendor(self) -> str:
        return "google"

    def synthesize_audio_sample_rate(self) -> int:
        if self.config and hasattr(self.config, "sample_rate"):
            return self.config.sample_rate
        return 24000  # Google TTS default sample rate

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0

        bytes_per_sample = 2  # 16-bit PCM
        channels = 1  # Mono
        duration_sec = self.total_audio_bytes / (
            self.synthesize_audio_sample_rate() * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)

    def _reset_request_state(self) -> None:
        """Reset request state for new requests"""
        self.total_audio_bytes = 0
        self.current_request_finished = False
        self.sent_ts = None

    async def on_data(self, ten_env: AsyncTenEnv, data) -> None:
        name = data.get_name()
        if name == "tts_flush":
            ten_env.log_info(f"Received tts_flush data: {name}")

            try:
                if self.client is not None:
                    ten_env.log_info(
                        "Flushing Google TTS client - cleaning old connection"
                    )
                    self.client.clean()  # Clean up old connection first
                    ten_env.log_info(
                        "Flushing Google TTS client - initializing new connection"
                    )
                    await self.client.reset()  # Initialize new connection
                    ten_env.log_info(
                        "Google TTS client flush completed successfully"
                    )
                else:
                    ten_env.log_warning(
                        "Client is not initialized, skipping reset"
                    )
            except Exception as e:
                ten_env.log_error(f"Error in handle_flush: {e}")
                # Check if ten_env is available before calling send_tts_error
                if self.ten_env is not None:
                    await self.send_tts_error(
                        self.current_request_id,
                        ModuleError(
                            message=str(e),
                            module=ModuleType.TTS,
                            code=ModuleErrorCode.NON_FATAL_ERROR,
                            vendor_info=ModuleErrorVendorInfo(
                                vendor=self.vendor()
                            ),
                        ),
                    )
                else:
                    ten_env.log_error(
                        "Cannot send error: ten_env is not initialized"
                    )

            # Check if ten_env is available before calling handle_completed_request
            if self.ten_env is not None:
                await self.handle_completed_request(
                    TTSAudioEndReason.INTERRUPTED
                )
            else:
                ten_env.log_warning(
                    "Cannot handle completed request: ten_env is not initialized"
                )
        await super().on_data(ten_env, data)

    async def handle_completed_request(self, reason: TTSAudioEndReason):
        # update request_id
        self.completed_request_ids.add(self.current_request_id)
        self.ten_env.log_info(
            f"add completed request_id to: {self.current_request_id}"
        )
        # send audio_end
        request_event_interval = 0
        if self.sent_ts is not None:
            request_event_interval = int(
                (datetime.now() - self.sent_ts).total_seconds() * 1000
            )
        await self.send_tts_audio_end(
            self.current_request_id,
            request_event_interval,
            self._calculate_audio_duration_ms(),
            self.current_turn_id,
            reason,
        )
        self.ten_env.log_info(
            f"Sent tts_audio_end with INTERRUPTED reason for request_id: {self.current_request_id}"
        )

    async def request_tts(self, t: TTSTextInput) -> None:
        try:
            if not self.client or not self.config:
                raise RuntimeError("Extension is not initialized properly.")

            # Check if request_id has already been completed
            if (
                self.completed_request_ids
                and t.request_id in self.completed_request_ids
            ):
                self.ten_env.log_warn(
                    f"Request ID {t.request_id} has already been completed, ignoring TTS request"
                )
                return

            # Handle new request_id
            if t.request_id != self.current_request_id:
                self.current_request_id = t.request_id
                self._reset_request_state()
                if t.metadata:
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
                            f"google_dump_{t.request_id}.pcm",
                        )
                        self.recorder_map[t.request_id] = PCMWriter(
                            dump_file_path
                        )
                        self.ten_env.log_info(
                            f"Created PCMWriter for request_id: {t.request_id}, file: {dump_file_path}"
                        )

                # Process the TTS request
                self.sent_ts = datetime.now()

            # Initialize variables for all cases
            first_chunk = True
            cur_duration_bytes = 0

            self.ten_env.log_info(
                f"Processing TTS request for text: '{t.text[:50]}...'"
            )

            # Process audio chunks
            audio_generator = self.client.get(t.text)
            try:
                async for audio_chunk, event in audio_generator:

                    if event == EVENT_TTS_RESPONSE and audio_chunk:
                        self.total_audio_bytes += len(audio_chunk)
                        cur_duration_bytes += len(audio_chunk)

                        if (
                            first_chunk
                            and self.sent_ts
                            and self.current_request_id
                        ):
                            start_datetime = datetime.now()
                            ttfb = int(
                                (start_datetime - self.sent_ts).total_seconds()
                                * 1000
                            )
                            await self.send_tts_audio_start(
                                self.current_request_id
                            )
                            await self.send_tts_ttfb_metrics(
                                self.current_request_id,
                                ttfb,
                                self.current_turn_id,
                            )
                            first_chunk = False

                        if (
                            self.config.dump
                            and self.current_request_id
                            and self.current_request_id in self.recorder_map
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
                            else "Unknown API key error"
                        )
                        await self.send_tts_error(
                            self.current_request_id or t.request_id,
                            ModuleError(
                                message=error_msg,
                                module=ModuleType.TTS,
                                code=ModuleErrorCode.FATAL_ERROR,
                                vendor_info=ModuleErrorVendorInfo(
                                    vendor=self.vendor()
                                ),
                            ),
                        )
                        return  # Exit early on error, don't send audio_end

                    elif event == EVENT_TTS_ERROR:
                        error_msg = (
                            audio_chunk.decode("utf-8")
                            if audio_chunk
                            else "Unknown client error"
                        )
                        raise RuntimeError(error_msg)
            except Exception as e:
                # Handle exceptions from the async for loop
                self.ten_env.log_error(
                    f"Error in audio processing: {traceback.format_exc()}"
                )
                await self.send_tts_error(
                    self.current_request_id or t.request_id,
                    ModuleError(
                        message=str(e),
                        module=ModuleType.TTS,
                        code=ModuleErrorCode.NON_FATAL_ERROR,
                        vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                    ),
                )

            finally:
                # Ensure the async generator is properly closed
                try:
                    await audio_generator.aclose()
                except Exception as e:
                    self.ten_env.log_warn(f"Error closing audio generator: {e}")

            # Handle end of request (only if no error occurred)
            if t.text_input_end:
                self.current_request_finished = True
                # Only send audio_end if not flushed
                await self.handle_completed_request(
                    TTSAudioEndReason.REQUEST_END
                )

            # Ensure all async operations are completed
            self.ten_env.log_info(
                f"TTS request {t.request_id} processing completed"
            )

        except Exception as e:
            self.ten_env.log_error(
                f"Error in request_tts: {traceback.format_exc()}"
            )
            await self.send_tts_error(
                self.current_request_id or t.request_id,
                ModuleError(
                    message=str(e),
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
